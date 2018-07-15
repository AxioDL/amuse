#include "amuse/AudioGroupPool.hpp"
#include "amuse/Common.hpp"
#include "amuse/Entity.hpp"
#include "amuse/AudioGroupData.hpp"
#include "athena/MemoryReader.hpp"
#include "logvisor/logvisor.hpp"

using namespace std::literals;

namespace amuse
{
static logvisor::Module Log("amuse::AudioGroupPool");

static bool AtEnd(athena::io::IStreamReader& r)
{
    uint32_t v = r.readUint32Big();
    r.seek(-4, athena::Current);
    return v == 0xffffffff;
}

template <athena::Endian DNAE>
AudioGroupPool AudioGroupPool::_AudioGroupPool(athena::io::IStreamReader& r)
{
    AudioGroupPool ret;

    PoolHeader<DNAE> head;
    head.read(r);

    if (head.soundMacrosOffset)
    {
        r.seek(head.soundMacrosOffset, athena::Begin);
        while (!AtEnd(r))
        {
            ObjectHeader<DNAE> objHead;
            atInt64 startPos = r.position();
            objHead.read(r);
            SoundMacro& macro = ret.m_soundMacros[objHead.objectId.id];
            macro.readCmds<DNAE>(r, objHead.size - 8);
            r.seek(startPos + objHead.size, athena::Begin);
        }
    }

    if (head.tablesOffset)
    {
        r.seek(head.tablesOffset, athena::Begin);
        while (!AtEnd(r))
        {
            ObjectHeader<DNAE> objHead;
            atInt64 startPos = r.position();
            objHead.read(r);
            auto& ptr = ret.m_tables[objHead.objectId.id];
            switch (objHead.size)
            {
            case 8:
                ptr = std::make_unique<ADSR>();
                static_cast<ADSR&>(*ptr).read(r);
                break;
            case 0x14:
                ptr = std::make_unique<ADSRDLS>();
                static_cast<ADSRDLS&>(*ptr).read(r);
                break;
            default:
                ptr = std::make_unique<Curve>();
                static_cast<Curve&>(*ptr).data.resize(objHead.size - 8);
                r.readUBytesToBuf(&static_cast<Curve&>(*ptr).data[0], objHead.size - 8);
                break;
            }
            r.seek(startPos + objHead.size, athena::Begin);
        }
    }

    if (head.keymapsOffset)
    {
        r.seek(head.keymapsOffset, athena::Begin);
        while (!AtEnd(r))
        {
            ObjectHeader<DNAE> objHead;
            atInt64 startPos = r.position();
            objHead.read(r);
            KeymapDNA<DNAE> kmData;
            kmData.read(r);
            ret.m_keymaps[objHead.objectId.id] = kmData;
            r.seek(startPos + objHead.size, athena::Begin);
        }
    }

    if (head.layersOffset)
    {
        r.seek(head.layersOffset, athena::Begin);
        while (!AtEnd(r))
        {
            ObjectHeader<DNAE> objHead;
            atInt64 startPos = r.position();
            objHead.read(r);
            std::vector<LayerMapping>& lm = ret.m_layers[objHead.objectId.id];
            uint32_t count;
            athena::io::Read<athena::io::PropType::None>::Do<decltype(count), DNAE>({}, count, r);
            lm.reserve(count);
            for (uint32_t i = 0; i < count; ++i)
            {
                LayerMappingDNA<DNAE> lmData;
                lmData.read(r);
                lm.push_back(lmData);
            }
            r.seek(startPos + objHead.size, athena::Begin);
        }
    }

    return ret;
}
template AudioGroupPool AudioGroupPool::_AudioGroupPool<athena::Big>(athena::io::IStreamReader& r);
template AudioGroupPool AudioGroupPool::_AudioGroupPool<athena::Little>(athena::io::IStreamReader& r);

AudioGroupPool AudioGroupPool::CreateAudioGroupPool(const AudioGroupData& data)
{
    athena::io::MemoryReader r(data.getPool(), data.getPoolSize());
    switch (data.getDataFormat())
    {
    case DataFormat::PC:
        return _AudioGroupPool<athena::Little>(r);
    default:
        return _AudioGroupPool<athena::Big>(r);
    }
}

template <class Tp>
static std::unique_ptr<SoundMacro::ICmd> MakeCmd(athena::io::MemoryReader& r)
{
    std::unique_ptr<SoundMacro::ICmd> ret = std::make_unique<Tp>();
    static_cast<Tp&>(*ret).read(r);
    return ret;
}

int SoundMacro::assertPC(int pc) const
{
    if (pc == -1)
        return -1;
    if (pc >= m_cmds.size())
    {
        fprintf(stderr, "SoundMacro PC bounds exceeded [%d/%d]\n", pc, int(m_cmds.size()));
        abort();
    }
    return pc;
}

template <athena::Endian DNAE>
void SoundMacro::readCmds(athena::io::IStreamReader& r, uint32_t size)
{
    uint32_t numCmds = size / 8;
    m_cmds.reserve(numCmds);
    for (int i = 0; i < numCmds; ++i)
    {
        uint32_t data[2];
        athena::io::Read<athena::io::PropType::None>::Do<decltype(data), DNAE>({}, data, r);
        athena::io::MemoryReader r(data, 8);
        std::unique_ptr<ICmd> cmd;
        switch (CmdOp(r.readUByte()))
        {
        case CmdOp::End:
            cmd = MakeCmd<CmdEnd>(r); break;
        case CmdOp::Stop:
            cmd = MakeCmd<CmdStop>(r); break;
        case CmdOp::SplitKey:
            cmd = MakeCmd<CmdSplitKey>(r); break;
        case CmdOp::SplitVel:
            cmd = MakeCmd<CmdSplitVel>(r); break;
        case CmdOp::WaitTicks:
            cmd = MakeCmd<CmdWaitTicks>(r); break;
        case CmdOp::Loop:
            cmd = MakeCmd<CmdLoop>(r); break;
        case CmdOp::Goto:
            cmd = MakeCmd<CmdGoto>(r); break;
        case CmdOp::WaitMs:
            cmd = MakeCmd<CmdWaitMs>(r); break;
        case CmdOp::PlayMacro:
            cmd = MakeCmd<CmdPlayMacro>(r); break;
        case CmdOp::SendKeyOff:
            cmd = MakeCmd<CmdSendKeyOff>(r); break;
        case CmdOp::SplitMod:
            cmd = MakeCmd<CmdSplitMod>(r); break;
        case CmdOp::PianoPan:
            cmd = MakeCmd<CmdPianoPan>(r); break;
        case CmdOp::SetAdsr:
            cmd = MakeCmd<CmdSetAdsr>(r); break;
        case CmdOp::ScaleVolume:
            cmd = MakeCmd<CmdScaleVolume>(r); break;
        case CmdOp::Panning:
            cmd = MakeCmd<CmdPanning>(r); break;
        case CmdOp::Envelope:
            cmd = MakeCmd<CmdEnvelope>(r); break;
        case CmdOp::StartSample:
            cmd = MakeCmd<CmdStartSample>(r); break;
        case CmdOp::StopSample:
            cmd = MakeCmd<CmdStopSample>(r); break;
        case CmdOp::KeyOff:
            cmd = MakeCmd<CmdKeyOff>(r); break;
        case CmdOp::SplitRnd:
            cmd = MakeCmd<CmdSplitRnd>(r); break;
        case CmdOp::FadeIn:
            cmd = MakeCmd<CmdFadeIn>(r); break;
        case CmdOp::Spanning:
            cmd = MakeCmd<CmdSpanning>(r); break;
        case CmdOp::SetAdsrCtrl:
            cmd = MakeCmd<CmdSetAdsrCtrl>(r); break;
        case CmdOp::RndNote:
            cmd = MakeCmd<CmdRndNote>(r); break;
        case CmdOp::AddNote:
            cmd = MakeCmd<CmdAddNote>(r); break;
        case CmdOp::SetNote:
            cmd = MakeCmd<CmdSetNote>(r); break;
        case CmdOp::LastNote:
            cmd = MakeCmd<CmdLastNote>(r); break;
        case CmdOp::Portamento:
            cmd = MakeCmd<CmdPortamento>(r); break;
        case CmdOp::Vibrato:
            cmd = MakeCmd<CmdVibrato>(r); break;
        case CmdOp::PitchSweep1:
            cmd = MakeCmd<CmdPitchSweep1>(r); break;
        case CmdOp::PitchSweep2:
            cmd = MakeCmd<CmdPitchSweep2>(r); break;
        case CmdOp::SetPitch:
            cmd = MakeCmd<CmdSetPitch>(r); break;
        case CmdOp::SetPitchAdsr:
            cmd = MakeCmd<CmdSetPitchAdsr>(r); break;
        case CmdOp::ScaleVolumeDLS:
            cmd = MakeCmd<CmdScaleVolumeDLS>(r); break;
        case CmdOp::Mod2Vibrange:
            cmd = MakeCmd<CmdMod2Vibrange>(r); break;
        case CmdOp::SetupTremolo:
            cmd = MakeCmd<CmdSetupTremolo>(r); break;
        case CmdOp::Return:
            cmd = MakeCmd<CmdReturn>(r); break;
        case CmdOp::GoSub:
            cmd = MakeCmd<CmdGoSub>(r); break;
        case CmdOp::TrapEvent:
            cmd = MakeCmd<CmdTrapEvent>(r); break;
        case CmdOp::UntrapEvent:
            cmd = MakeCmd<CmdUntrapEvent>(r); break;
        case CmdOp::SendMessage:
            cmd = MakeCmd<CmdSendMessage>(r); break;
        case CmdOp::GetMessage:
            cmd = MakeCmd<CmdGetMessage>(r); break;
        case CmdOp::GetVid:
            cmd = MakeCmd<CmdGetVid>(r); break;
        case CmdOp::AddAgeCount:
            cmd = MakeCmd<CmdAddAgeCount>(r); break;
        case CmdOp::SetAgeCount:
            cmd = MakeCmd<CmdSetAgeCount>(r); break;
        case CmdOp::SendFlag:
            cmd = MakeCmd<CmdSendFlag>(r); break;
        case CmdOp::PitchWheelR:
            cmd = MakeCmd<CmdPitchWheelR>(r); break;
        case CmdOp::SetPriority:
            cmd = MakeCmd<CmdSetPriority>(r); break;
        case CmdOp::AddPriority:
            cmd = MakeCmd<CmdAddPriority>(r); break;
        case CmdOp::AgeCntSpeed:
            cmd = MakeCmd<CmdAgeCntSpeed>(r); break;
        case CmdOp::AgeCntVel:
            cmd = MakeCmd<CmdAgeCntVel>(r); break;
        case CmdOp::VolSelect:
            cmd = MakeCmd<CmdVolSelect>(r); break;
        case CmdOp::PanSelect:
            cmd = MakeCmd<CmdPanSelect>(r); break;
        case CmdOp::PitchWheelSelect:
            cmd = MakeCmd<CmdPitchWheelSelect>(r); break;
        case CmdOp::ModWheelSelect:
            cmd = MakeCmd<CmdModWheelSelect>(r); break;
        case CmdOp::PedalSelect:
            cmd = MakeCmd<CmdPedalSelect>(r); break;
        case CmdOp::PortamentoSelect:
            cmd = MakeCmd<CmdPortamentoSelect>(r); break;
        case CmdOp::ReverbSelect:
            cmd = MakeCmd<CmdReverbSelect>(r); break;
        case CmdOp::SpanSelect:
            cmd = MakeCmd<CmdSpanSelect>(r); break;
        case CmdOp::DopplerSelect:
            cmd = MakeCmd<CmdDopplerSelect>(r); break;
        case CmdOp::TremoloSelect:
            cmd = MakeCmd<CmdTremoloSelect>(r); break;
        case CmdOp::PreASelect:
            cmd = MakeCmd<CmdPreASelect>(r); break;
        case CmdOp::PreBSelect:
            cmd = MakeCmd<CmdPreBSelect>(r); break;
        case CmdOp::PostBSelect:
            cmd = MakeCmd<CmdPostBSelect>(r); break;
        case CmdOp::AuxAFXSelect:
            cmd = MakeCmd<CmdAuxAFXSelect>(r); break;
        case CmdOp::AuxBFXSelect:
            cmd = MakeCmd<CmdAuxBFXSelect>(r); break;
        case CmdOp::SetupLFO:
            cmd = MakeCmd<CmdSetupLFO>(r); break;
        case CmdOp::ModeSelect:
            cmd = MakeCmd<CmdModeSelect>(r); break;
        case CmdOp::SetKeygroup:
            cmd = MakeCmd<CmdSetKeygroup>(r); break;
        case CmdOp::SRCmodeSelect:
            cmd = MakeCmd<CmdSRCmodeSelect>(r); break;
        case CmdOp::AddVars:
            cmd = MakeCmd<CmdAddVars>(r); break;
        case CmdOp::SubVars:
            cmd = MakeCmd<CmdSubVars>(r); break;
        case CmdOp::MulVars:
            cmd = MakeCmd<CmdMulVars>(r); break;
        case CmdOp::DivVars:
            cmd = MakeCmd<CmdDivVars>(r); break;
        case CmdOp::AddIVars:
            cmd = MakeCmd<CmdAddIVars>(r); break;
        case CmdOp::IfEqual:
            cmd = MakeCmd<CmdIfEqual>(r); break;
        case CmdOp::IfLess:
            cmd = MakeCmd<CmdIfLess>(r); break;
        default:
            break;
        }
        m_cmds.push_back(std::move(cmd));
    }
}
template void SoundMacro::readCmds<athena::Big>(athena::io::IStreamReader& r, uint32_t size);
template void SoundMacro::readCmds<athena::Little>(athena::io::IStreamReader& r, uint32_t size);

const SoundMacro* AudioGroupPool::soundMacro(ObjectId id) const
{
    auto search = m_soundMacros.find(id);
    if (search == m_soundMacros.cend())
        return nullptr;
    return &search->second;
}

const Keymap* AudioGroupPool::keymap(ObjectId id) const
{
    auto search = m_keymaps.find(id);
    if (search == m_keymaps.cend())
        return nullptr;
    return &search->second;
}

const std::vector<LayerMapping>* AudioGroupPool::layer(ObjectId id) const
{
    auto search = m_layers.find(id);
    if (search == m_layers.cend())
        return nullptr;
    return &search->second;
}

const ADSR* AudioGroupPool::tableAsAdsr(ObjectId id) const
{
    auto search = m_tables.find(id);
    if (search == m_tables.cend())
        return nullptr;
    return static_cast<const ADSR*>(search->second.get());
}

std::string_view SoundMacro::CmdOpToStr(CmdOp op)
{
    switch (op)
    {
    case CmdOp::End:
        return "End"sv;
    case CmdOp::Stop:
        return "Stop"sv;
    case CmdOp::SplitKey:
        return "SplitKey"sv;
    case CmdOp::SplitVel:
        return "SplitVel"sv;
    case CmdOp::WaitTicks:
        return "WaitTicks"sv;
    case CmdOp::Loop:
        return "Loop"sv;
    case CmdOp::Goto:
        return "Goto"sv;
    case CmdOp::WaitMs:
        return "WaitMs"sv;
    case CmdOp::PlayMacro:
        return "PlayMacro"sv;
    case CmdOp::SendKeyOff:
        return "SendKeyOff"sv;
    case CmdOp::SplitMod:
        return "SplitMod"sv;
    case CmdOp::PianoPan:
        return "PianoPan"sv;
    case CmdOp::SetAdsr:
        return "SetAdsr"sv;
    case CmdOp::ScaleVolume:
        return "ScaleVolume"sv;
    case CmdOp::Panning:
        return "Panning"sv;
    case CmdOp::Envelope:
        return "Envelope"sv;
    case CmdOp::StartSample:
        return "StartSample"sv;
    case CmdOp::StopSample:
        return "StopSample"sv;
    case CmdOp::KeyOff:
        return "KeyOff"sv;
    case CmdOp::SplitRnd:
        return "SplitRnd"sv;
    case CmdOp::FadeIn:
        return "FadeIn"sv;
    case CmdOp::Spanning:
        return "Spanning"sv;
    case CmdOp::SetAdsrCtrl:
        return "SetAdsrCtrl"sv;
    case CmdOp::RndNote:
        return "RndNote"sv;
    case CmdOp::AddNote:
        return "AddNote"sv;
    case CmdOp::SetNote:
        return "SetNote"sv;
    case CmdOp::LastNote:
        return "LastNote"sv;
    case CmdOp::Portamento:
        return "Portamento"sv;
    case CmdOp::Vibrato:
        return "Vibrato"sv;
    case CmdOp::PitchSweep1:
        return "PitchSweep1"sv;
    case CmdOp::PitchSweep2:
        return "PitchSweep2"sv;
    case CmdOp::SetPitch:
        return "SetPitch"sv;
    case CmdOp::SetPitchAdsr:
        return "SetPitchAdsr"sv;
    case CmdOp::ScaleVolumeDLS:
        return "ScaleVolumeDLS"sv;
    case CmdOp::Mod2Vibrange:
        return "Mod2Vibrange"sv;
    case CmdOp::SetupTremolo:
        return "SetupTremolo"sv;
    case CmdOp::Return:
        return "Return"sv;
    case CmdOp::GoSub:
        return "GoSub"sv;
    case CmdOp::TrapEvent:
        return "TrapEvent"sv;
    case CmdOp::UntrapEvent:
        return "UntrapEvent"sv;
    case CmdOp::SendMessage:
        return "SendMessage"sv;
    case CmdOp::GetMessage:
        return "GetMessage"sv;
    case CmdOp::GetVid:
        return "GetVid"sv;
    case CmdOp::AddAgeCount:
        return "AddAgeCount"sv;
    case CmdOp::SetAgeCount:
        return "SetAgeCount"sv;
    case CmdOp::SendFlag:
        return "SendFlag"sv;
    case CmdOp::PitchWheelR:
        return "PitchWheelR"sv;
    case CmdOp::SetPriority:
        return "SetPriority"sv;
    case CmdOp::AddPriority:
        return "AddPriority"sv;
    case CmdOp::AgeCntSpeed:
        return "AgeCntSpeed"sv;
    case CmdOp::AgeCntVel:
        return "AgeCntVel"sv;
    case CmdOp::VolSelect:
        return "VolSelect"sv;
    case CmdOp::PanSelect:
        return "PanSelect"sv;
    case CmdOp::PitchWheelSelect:
        return "PitchWheelSelect"sv;
    case CmdOp::ModWheelSelect:
        return "ModWheelSelect"sv;
    case CmdOp::PedalSelect:
        return "PedalSelect"sv;
    case CmdOp::PortamentoSelect:
        return "PortamentoSelect"sv;
    case CmdOp::ReverbSelect:
        return "ReverbSelect"sv;
    case CmdOp::SpanSelect:
        return "SpanSelect"sv;
    case CmdOp::DopplerSelect:
        return "DopplerSelect"sv;
    case CmdOp::TremoloSelect:
        return "TremoloSelect"sv;
    case CmdOp::PreASelect:
        return "PreASelect"sv;
    case CmdOp::PreBSelect:
        return "PreBSelect"sv;
    case CmdOp::PostBSelect:
        return "PostBSelect"sv;
    case CmdOp::AuxAFXSelect:
        return "AuxAFXSelect"sv;
    case CmdOp::AuxBFXSelect:
        return "AuxBFXSelect"sv;
    case CmdOp::SetupLFO:
        return "SetupLFO"sv;
    case CmdOp::ModeSelect:
        return "ModeSelect"sv;
    case CmdOp::SetKeygroup:
        return "SetKeygroup"sv;
    case CmdOp::SRCmodeSelect:
        return "SRCmodeSelect"sv;
    case CmdOp::AddVars:
        return "AddVars"sv;
    case CmdOp::SubVars:
        return "SubVars"sv;
    case CmdOp::MulVars:
        return "MulVars"sv;
    case CmdOp::DivVars:
        return "DivVars"sv;
    case CmdOp::AddIVars:
        return "AddIVars"sv;
    case CmdOp::IfEqual:
        return "IfEqual"sv;
    case CmdOp::IfLess:
        return "IfLess"sv;
    default:
        return ""sv;
    }
}

SoundMacro::CmdOp SoundMacro::CmdStrToOp(std::string_view op)
{
    if (!CompareCaseInsensitive(op.data(), "End"))
        return CmdOp::End;
    else if (!CompareCaseInsensitive(op.data(), "Stop"))
        return CmdOp::Stop;
    else if (!CompareCaseInsensitive(op.data(), "SplitKey"))
        return CmdOp::SplitKey;
    else if (!CompareCaseInsensitive(op.data(), "SplitVel"))
        return CmdOp::SplitVel;
    else if (!CompareCaseInsensitive(op.data(), "WaitTicks"))
        return CmdOp::WaitTicks;
    else if (!CompareCaseInsensitive(op.data(), "Loop"))
        return CmdOp::Loop;
    else if (!CompareCaseInsensitive(op.data(), "Goto"))
        return CmdOp::Goto;
    else if (!CompareCaseInsensitive(op.data(), "WaitMs"))
        return CmdOp::WaitMs;
    else if (!CompareCaseInsensitive(op.data(), "PlayMacro"))
        return CmdOp::PlayMacro;
    else if (!CompareCaseInsensitive(op.data(), "SendKeyOff"))
        return CmdOp::SendKeyOff;
    else if (!CompareCaseInsensitive(op.data(), "SplitMod"))
        return CmdOp::SplitMod;
    else if (!CompareCaseInsensitive(op.data(), "PianoPan"))
        return CmdOp::PianoPan;
    else if (!CompareCaseInsensitive(op.data(), "SetAdsr"))
        return CmdOp::SetAdsr;
    else if (!CompareCaseInsensitive(op.data(), "ScaleVolume"))
        return CmdOp::ScaleVolume;
    else if (!CompareCaseInsensitive(op.data(), "Panning"))
        return CmdOp::Panning;
    else if (!CompareCaseInsensitive(op.data(), "Envelope"))
        return CmdOp::Envelope;
    else if (!CompareCaseInsensitive(op.data(), "StartSample"))
        return CmdOp::StartSample;
    else if (!CompareCaseInsensitive(op.data(), "StopSample"))
        return CmdOp::StopSample;
    else if (!CompareCaseInsensitive(op.data(), "KeyOff"))
        return CmdOp::KeyOff;
    else if (!CompareCaseInsensitive(op.data(), "SplitRnd"))
        return CmdOp::SplitRnd;
    else if (!CompareCaseInsensitive(op.data(), "FadeIn"))
        return CmdOp::FadeIn;
    else if (!CompareCaseInsensitive(op.data(), "Spanning"))
        return CmdOp::Spanning;
    else if (!CompareCaseInsensitive(op.data(), "SetAdsrCtrl"))
        return CmdOp::SetAdsrCtrl;
    else if (!CompareCaseInsensitive(op.data(), "RndNote"))
        return CmdOp::RndNote;
    else if (!CompareCaseInsensitive(op.data(), "AddNote"))
        return CmdOp::AddNote;
    else if (!CompareCaseInsensitive(op.data(), "SetNote"))
        return CmdOp::SetNote;
    else if (!CompareCaseInsensitive(op.data(), "LastNote"))
        return CmdOp::LastNote;
    else if (!CompareCaseInsensitive(op.data(), "Portamento"))
        return CmdOp::Portamento;
    else if (!CompareCaseInsensitive(op.data(), "Vibrato"))
        return CmdOp::Vibrato;
    else if (!CompareCaseInsensitive(op.data(), "PitchSweep1"))
        return CmdOp::PitchSweep1;
    else if (!CompareCaseInsensitive(op.data(), "PitchSweep2"))
        return CmdOp::PitchSweep2;
    else if (!CompareCaseInsensitive(op.data(), "SetPitch"))
        return CmdOp::SetPitch;
    else if (!CompareCaseInsensitive(op.data(), "SetPitchAdsr"))
        return CmdOp::SetPitchAdsr;
    else if (!CompareCaseInsensitive(op.data(), "ScaleVolumeDLS"))
        return CmdOp::ScaleVolumeDLS;
    else if (!CompareCaseInsensitive(op.data(), "Mod2Vibrange"))
        return CmdOp::Mod2Vibrange;
    else if (!CompareCaseInsensitive(op.data(), "SetupTremolo"))
        return CmdOp::SetupTremolo;
    else if (!CompareCaseInsensitive(op.data(), "Return"))
        return CmdOp::Return;
    else if (!CompareCaseInsensitive(op.data(), "GoSub"))
        return CmdOp::GoSub;
    else if (!CompareCaseInsensitive(op.data(), "TrapEvent"))
        return CmdOp::TrapEvent;
    else if (!CompareCaseInsensitive(op.data(), "UntrapEvent"))
        return CmdOp::UntrapEvent;
    else if (!CompareCaseInsensitive(op.data(), "SendMessage"))
        return CmdOp::SendMessage;
    else if (!CompareCaseInsensitive(op.data(), "GetMessage"))
        return CmdOp::GetMessage;
    else if (!CompareCaseInsensitive(op.data(), "GetVid"))
        return CmdOp::GetVid;
    else if (!CompareCaseInsensitive(op.data(), "AddAgeCount"))
        return CmdOp::AddAgeCount;
    else if (!CompareCaseInsensitive(op.data(), "SetAgeCount"))
        return CmdOp::SetAgeCount;
    else if (!CompareCaseInsensitive(op.data(), "SendFlag"))
        return CmdOp::SendFlag;
    else if (!CompareCaseInsensitive(op.data(), "PitchWheelR"))
        return CmdOp::PitchWheelR;
    else if (!CompareCaseInsensitive(op.data(), "SetPriority"))
        return CmdOp::SetPriority;
    else if (!CompareCaseInsensitive(op.data(), "AddPriority"))
        return CmdOp::AddPriority;
    else if (!CompareCaseInsensitive(op.data(), "AgeCntSpeed"))
        return CmdOp::AgeCntSpeed;
    else if (!CompareCaseInsensitive(op.data(), "AgeCntVel"))
        return CmdOp::AgeCntVel;
    else if (!CompareCaseInsensitive(op.data(), "VolSelect"))
        return CmdOp::VolSelect;
    else if (!CompareCaseInsensitive(op.data(), "PanSelect"))
        return CmdOp::PanSelect;
    else if (!CompareCaseInsensitive(op.data(), "PitchWheelSelect"))
        return CmdOp::PitchWheelSelect;
    else if (!CompareCaseInsensitive(op.data(), "ModWheelSelect"))
        return CmdOp::ModWheelSelect;
    else if (!CompareCaseInsensitive(op.data(), "PedalSelect"))
        return CmdOp::PedalSelect;
    else if (!CompareCaseInsensitive(op.data(), "PortamentoSelect"))
        return CmdOp::PortamentoSelect;
    else if (!CompareCaseInsensitive(op.data(), "ReverbSelect"))
        return CmdOp::ReverbSelect;
    else if (!CompareCaseInsensitive(op.data(), "SpanSelect"))
        return CmdOp::SpanSelect;
    else if (!CompareCaseInsensitive(op.data(), "DopplerSelect"))
        return CmdOp::DopplerSelect;
    else if (!CompareCaseInsensitive(op.data(), "TremoloSelect"))
        return CmdOp::TremoloSelect;
    else if (!CompareCaseInsensitive(op.data(), "PreASelect"))
        return CmdOp::PreASelect;
    else if (!CompareCaseInsensitive(op.data(), "PreBSelect"))
        return CmdOp::PreBSelect;
    else if (!CompareCaseInsensitive(op.data(), "PostBSelect"))
        return CmdOp::PostBSelect;
    else if (!CompareCaseInsensitive(op.data(), "AuxAFXSelect"))
        return CmdOp::AuxAFXSelect;
    else if (!CompareCaseInsensitive(op.data(), "AuxBFXSelect"))
        return CmdOp::AuxBFXSelect;
    else if (!CompareCaseInsensitive(op.data(), "SetupLFO"))
        return CmdOp::SetupLFO;
    else if (!CompareCaseInsensitive(op.data(), "ModeSelect"))
        return CmdOp::ModeSelect;
    else if (!CompareCaseInsensitive(op.data(), "SetKeygroup"))
        return CmdOp::SetKeygroup;
    else if (!CompareCaseInsensitive(op.data(), "SRCmodeSelect"))
        return CmdOp::SRCmodeSelect;
    else if (!CompareCaseInsensitive(op.data(), "AddVars"))
        return CmdOp::AddVars;
    else if (!CompareCaseInsensitive(op.data(), "SubVars"))
        return CmdOp::SubVars;
    else if (!CompareCaseInsensitive(op.data(), "MulVars"))
        return CmdOp::MulVars;
    else if (!CompareCaseInsensitive(op.data(), "DivVars"))
        return CmdOp::DivVars;
    else if (!CompareCaseInsensitive(op.data(), "AddIVars"))
        return CmdOp::AddIVars;
    else if (!CompareCaseInsensitive(op.data(), "IfEqual"))
        return CmdOp::IfEqual;
    else if (!CompareCaseInsensitive(op.data(), "IfLess"))
        return CmdOp::IfLess;
    return CmdOp::Invalid;
}

bool AudioGroupPool::toYAML(athena::io::IStreamWriter& writer) const
{
    athena::io::YAMLDocWriter w("amuse::Pool");

    if (!m_soundMacros.empty())
    {
        if (auto __r = w.enterSubRecord("soundMacros"))
        {
            for (const auto& p : SortUnorderedMap(m_soundMacros))
            {
                if (auto __v = w.enterSubVector(SoundMacroId::CurNameDB->resolveNameFromId(p.first).data()))
                {
                    for (const auto& c : p.second.get().m_cmds)
                    {
                        if (auto __r2 = w.enterSubRecord(nullptr))
                        {
                            w.setStyle(athena::io::YAMLNodeStyle::Flow);
                            w.writeString("cmdOp", SoundMacro::CmdOpToStr(c->Isa()));
                            c->write(w);
                        }
                    }
                }
            }
        }
    }

    if (!m_tables.empty())
    {
        if (auto __r = w.enterSubRecord("tables"))
        {
            for (const auto& p : SortUnorderedMap(m_tables))
            {
                if (auto __v = w.enterSubRecord(TableId::CurNameDB->resolveNameFromId(p.first).data()))
                {
                    w.setStyle(athena::io::YAMLNodeStyle::Flow);
                    p.second.get()->write(w);
                }
            }
        }
    }

    if (!m_keymaps.empty())
    {
        if (auto __r = w.enterSubRecord("keymaps"))
        {
            for (const auto& p : SortUnorderedMap(m_keymaps))
            {
                if (auto __v = w.enterSubRecord(KeymapId::CurNameDB->resolveNameFromId(p.first).data()))
                {
                    w.setStyle(athena::io::YAMLNodeStyle::Flow);
                    p.second.get().write(w);
                }
            }
        }
    }

    if (!m_layers.empty())
    {
        if (auto __r = w.enterSubRecord("layers"))
        {
            for (const auto& p : SortUnorderedMap(m_layers))
            {
                if (auto __v = w.enterSubVector(LayersId::CurNameDB->resolveNameFromId(p.first).data()))
                {
                    for (const auto& lm : p.second.get())
                    {
                        if (auto __r2 = w.enterSubRecord(nullptr))
                        {
                            w.setStyle(athena::io::YAMLNodeStyle::Flow);
                            lm.write(w);
                        }
                    }
                }
            }
        }
    }

    return w.finish(&writer);
}

template <>
void amuse::Curve::Enumerate<LittleDNA::Read>(athena::io::IStreamReader& r)
{
    Log.report(logvisor::Fatal, "Curve binary DNA read not supported");
}

template <>
void amuse::Curve::Enumerate<LittleDNA::Write>(athena::io::IStreamWriter& w)
{
    Log.report(logvisor::Fatal, "Curve binary DNA write not supported");
}

template <>
void amuse::Curve::Enumerate<LittleDNA::BinarySize>(size_t& sz)
{
    Log.report(logvisor::Fatal, "Curve binary DNA size not supported");
}

template <>
void amuse::Curve::Enumerate<LittleDNA::ReadYaml>(athena::io::YAMLDocReader& r)
{
    r.enumerate("data", data);
}

template <>
void amuse::Curve::Enumerate<LittleDNA::WriteYaml>(athena::io::YAMLDocWriter& w)
{
    w.enumerate("data", data);
}

const char* amuse::Curve::DNAType()
{
    return "amuse::ADSR";
}

}

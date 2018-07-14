#include "amuse/AudioGroupPool.hpp"
#include "amuse/Common.hpp"
#include "amuse/Entity.hpp"
#include "amuse/AudioGroupData.hpp"
#include "athena/MemoryReader.hpp"
#include "logvisor/logvisor.hpp"

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
            Keymap& km = ret.m_keymaps[objHead.objectId.id];
            km.read(r);
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
                lm.emplace_back();
                lm.back().read(r);
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
    r.enumerate(nullptr, data);
}

template <>
void amuse::Curve::Enumerate<LittleDNA::WriteYaml>(athena::io::YAMLDocWriter& w)
{
    w.enumerate(nullptr, data);
}

const char* amuse::Curve::DNAType()
{
    return "amuse::ADSR";
}
}

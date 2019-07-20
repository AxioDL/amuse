#include <athena/MemoryWriter.hpp>
#include "amuse/AudioGroupPool.hpp"
#include "amuse/Common.hpp"
#include "amuse/Entity.hpp"
#include "amuse/AudioGroupData.hpp"
#include "logvisor/logvisor.hpp"
#include "athena/FileWriter.hpp"
#include "athena/FileReader.hpp"
#include "athena/VectorWriter.hpp"

using namespace std::literals;

namespace amuse {
static logvisor::Module Log("amuse::AudioGroupPool");

struct MakeCmdOp {
  template <class Tp, class R>
  static std::unique_ptr<SoundMacro::ICmd> Do(R& r) {
    std::unique_ptr<SoundMacro::ICmd> ret = std::make_unique<Tp>();
    static_cast<Tp&>(*ret).read(r);
    return ret;
  }
};

struct MakeCopyCmdOp {
  template <class Tp, class R>
  static std::unique_ptr<SoundMacro::ICmd> Do(R& r) {
    return std::make_unique<Tp>(static_cast<const Tp&>(r));
  }
};

struct MakeDefaultCmdOp {
  template <class Tp, class R>
  static std::unique_ptr<SoundMacro::ICmd> Do(R& r) {
    std::unique_ptr<SoundMacro::ICmd> ret = std::make_unique<Tp>();
    if (const SoundMacro::CmdIntrospection* introspection = SoundMacro::GetCmdIntrospection(r)) {
      for (int f = 0; f < 7; ++f) {
        const amuse::SoundMacro::CmdIntrospection::Field& field = introspection->m_fields[f];
        if (!field.m_name.empty()) {
          switch (field.m_tp) {
          case amuse::SoundMacro::CmdIntrospection::Field::Type::Bool:
            AccessField<bool>(ret.get(), field) = bool(field.m_default);
            break;
          case amuse::SoundMacro::CmdIntrospection::Field::Type::Int8:
          case amuse::SoundMacro::CmdIntrospection::Field::Type::Choice:
            AccessField<int8_t>(ret.get(), field) = int8_t(field.m_default);
            break;
          case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt8:
            AccessField<uint8_t>(ret.get(), field) = uint8_t(field.m_default);
            break;
          case amuse::SoundMacro::CmdIntrospection::Field::Type::Int16:
            AccessField<int16_t>(ret.get(), field) = int16_t(field.m_default);
            break;
          case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt16:
            AccessField<uint16_t>(ret.get(), field) = uint16_t(field.m_default);
            break;
          case amuse::SoundMacro::CmdIntrospection::Field::Type::Int32:
            AccessField<int32_t>(ret.get(), field) = int32_t(field.m_default);
            break;
          case amuse::SoundMacro::CmdIntrospection::Field::Type::UInt32:
            AccessField<uint32_t>(ret.get(), field) = uint32_t(field.m_default);
            break;
          case amuse::SoundMacro::CmdIntrospection::Field::Type::SoundMacroId:
          case amuse::SoundMacro::CmdIntrospection::Field::Type::SoundMacroStep:
          case amuse::SoundMacro::CmdIntrospection::Field::Type::TableId:
          case amuse::SoundMacro::CmdIntrospection::Field::Type::SampleId:
            AccessField<SoundMacroIdDNA<athena::Little>>(ret.get(), field).id = uint16_t(field.m_default);
            break;
          default:
            break;
          }
        }
      }
    }
    return ret;
  }
};

struct IntrospectCmdOp {
  template <class Tp>
  static const SoundMacro::CmdIntrospection* Do(SoundMacro::CmdOp) {
    return &Tp::Introspective;
  }
};

static bool AtEnd(athena::io::IStreamReader& r) {
  uint32_t v = r.readUint32Big();
  r.seek(-4, athena::Current);
  return v == 0xffffffff;
}

template <athena::Endian DNAE>
AudioGroupPool AudioGroupPool::_AudioGroupPool(athena::io::IStreamReader& r) {
  AudioGroupPool ret;

  PoolHeader<DNAE> head;
  head.read(r);

  if (head.soundMacrosOffset) {
    r.seek(head.soundMacrosOffset, athena::Begin);
    while (!AtEnd(r)) {
      ObjectHeader<DNAE> objHead;
      atInt64 startPos = r.position();
      objHead.read(r);
      if (SoundMacroId::CurNameDB)
        SoundMacroId::CurNameDB->registerPair(NameDB::generateName(objHead.objectId, NameDB::Type::SoundMacro),
                                              objHead.objectId);
      auto& macro = ret.m_soundMacros[objHead.objectId.id];
      macro = MakeObj<SoundMacro>();
      macro->template readCmds<DNAE>(r, objHead.size - 8);
      r.seek(startPos + objHead.size, athena::Begin);
    }
  }

  if (head.tablesOffset) {
    r.seek(head.tablesOffset, athena::Begin);
    while (!AtEnd(r)) {
      ObjectHeader<DNAE> objHead;
      atInt64 startPos = r.position();
      objHead.read(r);
      if (TableId::CurNameDB)
        TableId::CurNameDB->registerPair(NameDB::generateName(objHead.objectId, NameDB::Type::Table), objHead.objectId);
      auto& ptr = ret.m_tables[objHead.objectId.id];
      switch (objHead.size) {
      case 0x10:
        ptr = MakeObj<std::unique_ptr<ITable>>(std::make_unique<ADSR>());
        static_cast<ADSR&>(**ptr).read(r);
        break;
      case 0x1c:
        ptr = MakeObj<std::unique_ptr<ITable>>(std::make_unique<ADSRDLS>());
        static_cast<ADSRDLS&>(**ptr).read(r);
        break;
      default:
        ptr = MakeObj<std::unique_ptr<ITable>>(std::make_unique<Curve>());
        static_cast<Curve&>(**ptr).data.resize(objHead.size - 8);
        r.readUBytesToBuf(&static_cast<Curve&>(**ptr).data[0], objHead.size - 8);
        break;
      }
      r.seek(startPos + objHead.size, athena::Begin);
    }
  }

  if (head.keymapsOffset) {
    r.seek(head.keymapsOffset, athena::Begin);
    while (!AtEnd(r)) {
      ObjectHeader<DNAE> objHead;
      atInt64 startPos = r.position();
      objHead.read(r);
      if (KeymapId::CurNameDB)
        KeymapId::CurNameDB->registerPair(NameDB::generateName(objHead.objectId, NameDB::Type::Keymap),
                                          objHead.objectId);
      auto& km = ret.m_keymaps[objHead.objectId.id];
      km = MakeObj<std::array<Keymap, 128>>();
      for (int i = 0; i < 128; ++i) {
        KeymapDNA<DNAE> kmData;
        kmData.read(r);
        (*km)[i] = kmData;
      }
      r.seek(startPos + objHead.size, athena::Begin);
    }
  }

  if (head.layersOffset) {
    r.seek(head.layersOffset, athena::Begin);
    while (!AtEnd(r)) {
      ObjectHeader<DNAE> objHead;
      atInt64 startPos = r.position();
      objHead.read(r);
      if (LayersId::CurNameDB)
        LayersId::CurNameDB->registerPair(NameDB::generateName(objHead.objectId, NameDB::Type::Layer),
                                          objHead.objectId);
      auto& lm = ret.m_layers[objHead.objectId.id];
      lm = MakeObj<std::vector<LayerMapping>>();
      uint32_t count;
      athena::io::Read<athena::io::PropType::None>::Do<decltype(count), DNAE>({}, count, r);
      lm->reserve(count);
      for (uint32_t i = 0; i < count; ++i) {
        LayerMappingDNA<DNAE> lmData;
        lmData.read(r);
        lm->push_back(lmData);
      }
      r.seek(startPos + objHead.size, athena::Begin);
    }
  }

  return ret;
}
template AudioGroupPool AudioGroupPool::_AudioGroupPool<athena::Big>(athena::io::IStreamReader& r);
template AudioGroupPool AudioGroupPool::_AudioGroupPool<athena::Little>(athena::io::IStreamReader& r);

AudioGroupPool AudioGroupPool::CreateAudioGroupPool(const AudioGroupData& data) {
  if (data.getPoolSize() < 16)
    return {};
  athena::io::MemoryReader r(data.getPool(), data.getPoolSize());
  switch (data.getDataFormat()) {
  case DataFormat::PC:
    return _AudioGroupPool<athena::Little>(r);
  default:
    return _AudioGroupPool<athena::Big>(r);
  }
}

AudioGroupPool AudioGroupPool::CreateAudioGroupPool(SystemStringView groupPath) {
  AudioGroupPool ret;
  SystemString poolPath(groupPath);
  poolPath += _SYS_STR("/!pool.yaml");
  athena::io::FileReader fi(poolPath, 32 * 1024, false);

  if (!fi.hasError()) {
    athena::io::YAMLDocReader r;
    if (r.parse(&fi) && !r.readString("DNAType").compare("amuse::Pool")) {
      if (auto __r = r.enterSubRecord("soundMacros")) {
        for (const auto& sm : r.getCurNode()->m_mapChildren) {
          ObjectId macroId = SoundMacroId::CurNameDB->generateId(NameDB::Type::SoundMacro);
          SoundMacroId::CurNameDB->registerPair(sm.first, macroId);
        }
      }

      if (auto __r = r.enterSubRecord("tables")) {
        for (const auto& t : r.getCurNode()->m_mapChildren) {
          if (auto __v = r.enterSubRecord(t.first.c_str())) {
            ObjectId tableId = TableId::CurNameDB->generateId(NameDB::Type::Table);
            TableId::CurNameDB->registerPair(t.first, tableId);
          }
        }
      }

      if (auto __r = r.enterSubRecord("keymaps")) {
        for (const auto& k : r.getCurNode()->m_mapChildren)
          if (auto __v = r.enterSubRecord(k.first.c_str())) {
            ObjectId keymapId = KeymapId::CurNameDB->generateId(NameDB::Type::Keymap);
            KeymapId::CurNameDB->registerPair(k.first, keymapId);
          }
      }

      if (auto __r = r.enterSubRecord("layers")) {
        for (const auto& l : r.getCurNode()->m_mapChildren) {
          size_t mappingCount;
          if (auto __v = r.enterSubVector(l.first.c_str(), mappingCount)) {
            ObjectId layersId = LayersId::CurNameDB->generateId(NameDB::Type::Layer);
            LayersId::CurNameDB->registerPair(l.first, layersId);
          }
        }
      }

      if (auto __r = r.enterSubRecord("soundMacros")) {
        ret.m_soundMacros.reserve(r.getCurNode()->m_mapChildren.size());
        for (const auto& sm : r.getCurNode()->m_mapChildren) {
          auto& smOut = ret.m_soundMacros[SoundMacroId::CurNameDB->resolveIdFromName(sm.first)];
          smOut = MakeObj<SoundMacro>();
          size_t cmdCount;
          if (auto __v = r.enterSubVector(sm.first.c_str(), cmdCount))
            smOut->fromYAML(r, cmdCount);
        }
      }

      if (auto __r = r.enterSubRecord("tables")) {
        ret.m_tables.reserve(r.getCurNode()->m_mapChildren.size());
        for (const auto& t : r.getCurNode()->m_mapChildren) {
          if (auto __v = r.enterSubRecord(t.first.c_str())) {
            auto& tableOut = ret.m_tables[TableId::CurNameDB->resolveIdFromName(t.first)];
            if (auto __att = r.enterSubRecord("attack")) {
              __att.leave();
              if (auto __vta = r.enterSubRecord("velToAttack")) {
                __vta.leave();
                tableOut = MakeObj<std::unique_ptr<ITable>>(std::make_unique<ADSRDLS>());
                static_cast<ADSRDLS&>(**tableOut).read(r);
              } else {
                tableOut = MakeObj<std::unique_ptr<ITable>>(std::make_unique<ADSR>());
                static_cast<ADSR&>(**tableOut).read(r);
              }
            } else if (auto __dat = r.enterSubRecord("data")) {
              __dat.leave();
              tableOut = MakeObj<std::unique_ptr<ITable>>(std::make_unique<Curve>());
              static_cast<Curve&>(**tableOut).read(r);
            }
          }
        }
      }

      if (auto __r = r.enterSubRecord("keymaps")) {
        ret.m_keymaps.reserve(r.getCurNode()->m_mapChildren.size());
        for (const auto& k : r.getCurNode()->m_mapChildren) {
          size_t mappingCount;
          if (auto __v = r.enterSubVector(k.first.c_str(), mappingCount)) {
            auto& kmOut = ret.m_keymaps[KeymapId::CurNameDB->resolveIdFromName(k.first)];
            kmOut = MakeObj<std::array<Keymap, 128>>();
            for (size_t i = 0; i < mappingCount && i < 128; ++i)
              if (auto __r2 = r.enterSubRecord(nullptr))
                (*kmOut)[i].read(r);
          }
        }
      }

      if (auto __r = r.enterSubRecord("layers")) {
        ret.m_layers.reserve(r.getCurNode()->m_mapChildren.size());
        for (const auto& l : r.getCurNode()->m_mapChildren) {
          size_t mappingCount;
          if (auto __v = r.enterSubVector(l.first.c_str(), mappingCount)) {
            auto& layOut = ret.m_layers[LayersId::CurNameDB->resolveIdFromName(l.first)];
            layOut = MakeObj<std::vector<LayerMapping>>();
            layOut->reserve(mappingCount);
            for (size_t lm = 0; lm < mappingCount; ++lm) {
              if (auto __r2 = r.enterSubRecord(nullptr)) {
                layOut->emplace_back();
                layOut->back().read(r);
              }
            }
          }
        }
      }
    }
  }

  return ret;
}

int SoundMacro::assertPC(int pc) const {
  if (pc < 0)
    return -1;
  if (size_t(pc) >= m_cmds.size()) {
    fmt::print(stderr, fmt("SoundMacro PC bounds exceeded [{}/{}]\n"), pc, int(m_cmds.size()));
    abort();
  }
  return pc;
}

template <athena::Endian DNAE>
void SoundMacro::readCmds(athena::io::IStreamReader& r, uint32_t size) {
  uint32_t numCmds = size / 8;
  m_cmds.reserve(numCmds);
  for (uint32_t i = 0; i < numCmds; ++i) {
    uint32_t data[2];
    athena::io::Read<athena::io::PropType::None>::Do<decltype(data), DNAE>({}, data, r);
    athena::io::MemoryReader r(data, 8);
    m_cmds.push_back(CmdDo<MakeCmdOp, std::unique_ptr<SoundMacro::ICmd>>(r));
  }
}
template void SoundMacro::readCmds<athena::Big>(athena::io::IStreamReader& r, uint32_t size);
template void SoundMacro::readCmds<athena::Little>(athena::io::IStreamReader& r, uint32_t size);

template <athena::Endian DNAE>
void SoundMacro::writeCmds(athena::io::IStreamWriter& w) const {
  for (const auto& cmd : m_cmds) {
    uint32_t data[2];
    athena::io::MemoryWriter mw((uint8_t*)data, 8);
    mw.writeUByte(uint8_t(cmd->Isa()));
    cmd->write(mw);
    athena::io::Write<athena::io::PropType::None>::Do<decltype(data), DNAE>({}, data, w);
  }
}
template void SoundMacro::writeCmds<athena::Big>(athena::io::IStreamWriter& w) const;
template void SoundMacro::writeCmds<athena::Little>(athena::io::IStreamWriter& w) const;

void SoundMacro::buildFromPrototype(const SoundMacro& other) {
  m_cmds.reserve(other.m_cmds.size());
  for (auto& cmd : other.m_cmds)
    m_cmds.push_back(CmdDo<MakeCopyCmdOp, std::unique_ptr<SoundMacro::ICmd>>(*cmd));
}

void SoundMacro::toYAML(athena::io::YAMLDocWriter& w) const {
  for (const auto& c : m_cmds) {
    if (auto __r2 = w.enterSubRecord(nullptr)) {
      w.setStyle(athena::io::YAMLNodeStyle::Flow);
      w.writeString("cmdOp", SoundMacro::CmdOpToStr(c->Isa()));
      c->write(w);
    }
  }
}

void SoundMacro::fromYAML(athena::io::YAMLDocReader& r, size_t cmdCount) {
  m_cmds.reserve(cmdCount);
  for (size_t c = 0; c < cmdCount; ++c)
    if (auto __r2 = r.enterSubRecord(nullptr))
      m_cmds.push_back(SoundMacro::CmdDo<MakeCmdOp, std::unique_ptr<SoundMacro::ICmd>>(r));
}

const SoundMacro* AudioGroupPool::soundMacro(ObjectId id) const {
  auto search = m_soundMacros.find(id);
  if (search == m_soundMacros.cend())
    return nullptr;
  return search->second.get();
}

const Keymap* AudioGroupPool::keymap(ObjectId id) const {
  auto search = m_keymaps.find(id);
  if (search == m_keymaps.cend())
    return nullptr;
  return search->second.get()->data();
}

const std::vector<LayerMapping>* AudioGroupPool::layer(ObjectId id) const {
  auto search = m_layers.find(id);
  if (search == m_layers.cend())
    return nullptr;
  return search->second.get();
}

const ADSR* AudioGroupPool::tableAsAdsr(ObjectId id) const {
  auto search = m_tables.find(id);
  if (search == m_tables.cend() || (*search->second)->Isa() != ITable::Type::ADSR)
    return nullptr;
  return static_cast<const ADSR*>((*search->second).get());
}

const ADSRDLS* AudioGroupPool::tableAsAdsrDLS(ObjectId id) const {
  auto search = m_tables.find(id);
  if (search == m_tables.cend() || (*search->second)->Isa() != ITable::Type::ADSRDLS)
    return nullptr;
  return static_cast<const ADSRDLS*>((*search->second).get());
}

const Curve* AudioGroupPool::tableAsCurves(ObjectId id) const {
  auto search = m_tables.find(id);
  if (search == m_tables.cend() || (*search->second)->Isa() != ITable::Type::Curve)
    return nullptr;
  return static_cast<const Curve*>((*search->second).get());
}

static SoundMacro::CmdOp _ReadCmdOp(athena::io::MemoryReader& r) { return SoundMacro::CmdOp(r.readUByte()); }

static SoundMacro::CmdOp _ReadCmdOp(athena::io::YAMLDocReader& r) {
  return SoundMacro::CmdStrToOp(r.readString("cmdOp"));
}

static SoundMacro::CmdOp _ReadCmdOp(SoundMacro::CmdOp& op) { return op; }

static SoundMacro::CmdOp _ReadCmdOp(const SoundMacro::ICmd& op) { return op.Isa(); }

template <class Op, class O, class... _Args>
O SoundMacro::CmdDo(_Args&&... args) {
  SoundMacro::CmdOp op = _ReadCmdOp(std::forward<_Args>(args)...);
  switch (op) {
  case CmdOp::End:
    return Op::template Do<CmdEnd>(std::forward<_Args>(args)...);
  case CmdOp::Stop:
    return Op::template Do<CmdStop>(std::forward<_Args>(args)...);
  case CmdOp::SplitKey:
    return Op::template Do<CmdSplitKey>(std::forward<_Args>(args)...);
  case CmdOp::SplitVel:
    return Op::template Do<CmdSplitVel>(std::forward<_Args>(args)...);
  case CmdOp::WaitTicks:
    return Op::template Do<CmdWaitTicks>(std::forward<_Args>(args)...);
  case CmdOp::Loop:
    return Op::template Do<CmdLoop>(std::forward<_Args>(args)...);
  case CmdOp::Goto:
    return Op::template Do<CmdGoto>(std::forward<_Args>(args)...);
  case CmdOp::WaitMs:
    return Op::template Do<CmdWaitMs>(std::forward<_Args>(args)...);
  case CmdOp::PlayMacro:
    return Op::template Do<CmdPlayMacro>(std::forward<_Args>(args)...);
  case CmdOp::SendKeyOff:
    return Op::template Do<CmdSendKeyOff>(std::forward<_Args>(args)...);
  case CmdOp::SplitMod:
    return Op::template Do<CmdSplitMod>(std::forward<_Args>(args)...);
  case CmdOp::PianoPan:
    return Op::template Do<CmdPianoPan>(std::forward<_Args>(args)...);
  case CmdOp::SetAdsr:
    return Op::template Do<CmdSetAdsr>(std::forward<_Args>(args)...);
  case CmdOp::ScaleVolume:
    return Op::template Do<CmdScaleVolume>(std::forward<_Args>(args)...);
  case CmdOp::Panning:
    return Op::template Do<CmdPanning>(std::forward<_Args>(args)...);
  case CmdOp::Envelope:
    return Op::template Do<CmdEnvelope>(std::forward<_Args>(args)...);
  case CmdOp::StartSample:
    return Op::template Do<CmdStartSample>(std::forward<_Args>(args)...);
  case CmdOp::StopSample:
    return Op::template Do<CmdStopSample>(std::forward<_Args>(args)...);
  case CmdOp::KeyOff:
    return Op::template Do<CmdKeyOff>(std::forward<_Args>(args)...);
  case CmdOp::SplitRnd:
    return Op::template Do<CmdSplitRnd>(std::forward<_Args>(args)...);
  case CmdOp::FadeIn:
    return Op::template Do<CmdFadeIn>(std::forward<_Args>(args)...);
  case CmdOp::Spanning:
    return Op::template Do<CmdSpanning>(std::forward<_Args>(args)...);
  case CmdOp::SetAdsrCtrl:
    return Op::template Do<CmdSetAdsrCtrl>(std::forward<_Args>(args)...);
  case CmdOp::RndNote:
    return Op::template Do<CmdRndNote>(std::forward<_Args>(args)...);
  case CmdOp::AddNote:
    return Op::template Do<CmdAddNote>(std::forward<_Args>(args)...);
  case CmdOp::SetNote:
    return Op::template Do<CmdSetNote>(std::forward<_Args>(args)...);
  case CmdOp::LastNote:
    return Op::template Do<CmdLastNote>(std::forward<_Args>(args)...);
  case CmdOp::Portamento:
    return Op::template Do<CmdPortamento>(std::forward<_Args>(args)...);
  case CmdOp::Vibrato:
    return Op::template Do<CmdVibrato>(std::forward<_Args>(args)...);
  case CmdOp::PitchSweep1:
    return Op::template Do<CmdPitchSweep1>(std::forward<_Args>(args)...);
  case CmdOp::PitchSweep2:
    return Op::template Do<CmdPitchSweep2>(std::forward<_Args>(args)...);
  case CmdOp::SetPitch:
    return Op::template Do<CmdSetPitch>(std::forward<_Args>(args)...);
  case CmdOp::SetPitchAdsr:
    return Op::template Do<CmdSetPitchAdsr>(std::forward<_Args>(args)...);
  case CmdOp::ScaleVolumeDLS:
    return Op::template Do<CmdScaleVolumeDLS>(std::forward<_Args>(args)...);
  case CmdOp::Mod2Vibrange:
    return Op::template Do<CmdMod2Vibrange>(std::forward<_Args>(args)...);
  case CmdOp::SetupTremolo:
    return Op::template Do<CmdSetupTremolo>(std::forward<_Args>(args)...);
  case CmdOp::Return:
    return Op::template Do<CmdReturn>(std::forward<_Args>(args)...);
  case CmdOp::GoSub:
    return Op::template Do<CmdGoSub>(std::forward<_Args>(args)...);
  case CmdOp::TrapEvent:
    return Op::template Do<CmdTrapEvent>(std::forward<_Args>(args)...);
  case CmdOp::UntrapEvent:
    return Op::template Do<CmdUntrapEvent>(std::forward<_Args>(args)...);
  case CmdOp::SendMessage:
    return Op::template Do<CmdSendMessage>(std::forward<_Args>(args)...);
  case CmdOp::GetMessage:
    return Op::template Do<CmdGetMessage>(std::forward<_Args>(args)...);
  case CmdOp::GetVid:
    return Op::template Do<CmdGetVid>(std::forward<_Args>(args)...);
  case CmdOp::AddAgeCount:
    return Op::template Do<CmdAddAgeCount>(std::forward<_Args>(args)...);
  case CmdOp::SetAgeCount:
    return Op::template Do<CmdSetAgeCount>(std::forward<_Args>(args)...);
  case CmdOp::SendFlag:
    return Op::template Do<CmdSendFlag>(std::forward<_Args>(args)...);
  case CmdOp::PitchWheelR:
    return Op::template Do<CmdPitchWheelR>(std::forward<_Args>(args)...);
  case CmdOp::SetPriority:
    return Op::template Do<CmdSetPriority>(std::forward<_Args>(args)...);
  case CmdOp::AddPriority:
    return Op::template Do<CmdAddPriority>(std::forward<_Args>(args)...);
  case CmdOp::AgeCntSpeed:
    return Op::template Do<CmdAgeCntSpeed>(std::forward<_Args>(args)...);
  case CmdOp::AgeCntVel:
    return Op::template Do<CmdAgeCntVel>(std::forward<_Args>(args)...);
  case CmdOp::VolSelect:
    return Op::template Do<CmdVolSelect>(std::forward<_Args>(args)...);
  case CmdOp::PanSelect:
    return Op::template Do<CmdPanSelect>(std::forward<_Args>(args)...);
  case CmdOp::PitchWheelSelect:
    return Op::template Do<CmdPitchWheelSelect>(std::forward<_Args>(args)...);
  case CmdOp::ModWheelSelect:
    return Op::template Do<CmdModWheelSelect>(std::forward<_Args>(args)...);
  case CmdOp::PedalSelect:
    return Op::template Do<CmdPedalSelect>(std::forward<_Args>(args)...);
  case CmdOp::PortamentoSelect:
    return Op::template Do<CmdPortamentoSelect>(std::forward<_Args>(args)...);
  case CmdOp::ReverbSelect:
    return Op::template Do<CmdReverbSelect>(std::forward<_Args>(args)...);
  case CmdOp::SpanSelect:
    return Op::template Do<CmdSpanSelect>(std::forward<_Args>(args)...);
  case CmdOp::DopplerSelect:
    return Op::template Do<CmdDopplerSelect>(std::forward<_Args>(args)...);
  case CmdOp::TremoloSelect:
    return Op::template Do<CmdTremoloSelect>(std::forward<_Args>(args)...);
  case CmdOp::PreASelect:
    return Op::template Do<CmdPreASelect>(std::forward<_Args>(args)...);
  case CmdOp::PreBSelect:
    return Op::template Do<CmdPreBSelect>(std::forward<_Args>(args)...);
  case CmdOp::PostBSelect:
    return Op::template Do<CmdPostBSelect>(std::forward<_Args>(args)...);
  case CmdOp::AuxAFXSelect:
    return Op::template Do<CmdAuxAFXSelect>(std::forward<_Args>(args)...);
  case CmdOp::AuxBFXSelect:
    return Op::template Do<CmdAuxBFXSelect>(std::forward<_Args>(args)...);
  case CmdOp::SetupLFO:
    return Op::template Do<CmdSetupLFO>(std::forward<_Args>(args)...);
  case CmdOp::ModeSelect:
    return Op::template Do<CmdModeSelect>(std::forward<_Args>(args)...);
  case CmdOp::SetKeygroup:
    return Op::template Do<CmdSetKeygroup>(std::forward<_Args>(args)...);
  case CmdOp::SRCmodeSelect:
    return Op::template Do<CmdSRCmodeSelect>(std::forward<_Args>(args)...);
  case CmdOp::WiiUnknown:
    return Op::template Do<CmdWiiUnknown>(std::forward<_Args>(args)...);
  case CmdOp::WiiUnknown2:
    return Op::template Do<CmdWiiUnknown2>(std::forward<_Args>(args)...);
  case CmdOp::AddVars:
    return Op::template Do<CmdAddVars>(std::forward<_Args>(args)...);
  case CmdOp::SubVars:
    return Op::template Do<CmdSubVars>(std::forward<_Args>(args)...);
  case CmdOp::MulVars:
    return Op::template Do<CmdMulVars>(std::forward<_Args>(args)...);
  case CmdOp::DivVars:
    return Op::template Do<CmdDivVars>(std::forward<_Args>(args)...);
  case CmdOp::AddIVars:
    return Op::template Do<CmdAddIVars>(std::forward<_Args>(args)...);
  case CmdOp::SetVar:
    return Op::template Do<CmdSetVar>(std::forward<_Args>(args)...);
  case CmdOp::IfEqual:
    return Op::template Do<CmdIfEqual>(std::forward<_Args>(args)...);
  case CmdOp::IfLess:
    return Op::template Do<CmdIfLess>(std::forward<_Args>(args)...);
  default:
    return {};
  }
}
template std::unique_ptr<SoundMacro::ICmd> SoundMacro::CmdDo<MakeCmdOp>(athena::io::MemoryReader& r);
template std::unique_ptr<SoundMacro::ICmd> SoundMacro::CmdDo<MakeCmdOp>(athena::io::YAMLDocReader& r);
template std::unique_ptr<SoundMacro::ICmd> SoundMacro::CmdDo<MakeDefaultCmdOp>(SoundMacro::CmdOp& r);
template const SoundMacro::CmdIntrospection* SoundMacro::CmdDo<IntrospectCmdOp>(SoundMacro::CmdOp& op);

std::unique_ptr<SoundMacro::ICmd> SoundMacro::MakeCmd(CmdOp op) {
  return CmdDo<MakeDefaultCmdOp, std::unique_ptr<SoundMacro::ICmd>>(op);
}

const SoundMacro::CmdIntrospection* SoundMacro::GetCmdIntrospection(CmdOp op) {
  return CmdDo<IntrospectCmdOp, const SoundMacro::CmdIntrospection*>(op);
}

std::string_view SoundMacro::CmdOpToStr(CmdOp op) {
  switch (op) {
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
  case CmdOp::WiiUnknown:
    return "WiiUnknown"sv;
  case CmdOp::WiiUnknown2:
    return "WiiUnknown2"sv;
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
  case CmdOp::SetVar:
    return "SetVar"sv;
  case CmdOp::IfEqual:
    return "IfEqual"sv;
  case CmdOp::IfLess:
    return "IfLess"sv;
  default:
    return ""sv;
  }
}

SoundMacro::CmdOp SoundMacro::CmdStrToOp(std::string_view op) {
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
  else if (!CompareCaseInsensitive(op.data(), "WiiUnknown"))
    return CmdOp::WiiUnknown;
  else if (!CompareCaseInsensitive(op.data(), "WiiUnknown2"))
    return CmdOp::WiiUnknown2;
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
  else if (!CompareCaseInsensitive(op.data(), "SetVar"))
    return CmdOp::SetVar;
  else if (!CompareCaseInsensitive(op.data(), "IfEqual"))
    return CmdOp::IfEqual;
  else if (!CompareCaseInsensitive(op.data(), "IfLess"))
    return CmdOp::IfLess;
  return CmdOp::Invalid;
}

std::vector<uint8_t> AudioGroupPool::toYAML() const {
  athena::io::YAMLDocWriter w("amuse::Pool");

  if (!m_soundMacros.empty()) {
    if (auto __r = w.enterSubRecord("soundMacros")) {
      for (const auto& p : SortUnorderedMap(m_soundMacros)) {
        if (auto __v = w.enterSubVector(SoundMacroId::CurNameDB->resolveNameFromId(p.first).data())) {
          p.second.get()->toYAML(w);
        }
      }
    }
  }

  if (!m_tables.empty()) {
    if (auto __r = w.enterSubRecord("tables")) {
      for (const auto& p : SortUnorderedMap(m_tables)) {
        if (auto __v = w.enterSubRecord(TableId::CurNameDB->resolveNameFromId(p.first).data())) {
          w.setStyle(athena::io::YAMLNodeStyle::Flow);
          (*p.second.get())->write(w);
        }
      }
    }
  }

  if (!m_keymaps.empty()) {
    if (auto __r = w.enterSubRecord("keymaps")) {
      for (const auto& p : SortUnorderedMap(m_keymaps)) {
        if (auto __v = w.enterSubVector(KeymapId::CurNameDB->resolveNameFromId(p.first).data())) {
          for (const auto& km : *p.second.get()) {
            if (auto __r2 = w.enterSubRecord(nullptr)) {
              w.setStyle(athena::io::YAMLNodeStyle::Flow);
              km.write(w);
            }
          }
        }
      }
    }
  }

  if (!m_layers.empty()) {
    if (auto __r = w.enterSubRecord("layers")) {
      for (const auto& p : SortUnorderedMap(m_layers)) {
        if (auto __v = w.enterSubVector(LayersId::CurNameDB->resolveNameFromId(p.first).data())) {
          for (const auto& lm : *p.second.get()) {
            if (auto __r2 = w.enterSubRecord(nullptr)) {
              w.setStyle(athena::io::YAMLNodeStyle::Flow);
              lm.write(w);
            }
          }
        }
      }
    }
  }

  athena::io::VectorWriter fo;
  w.finish(&fo);
  return fo.data();
}

template <athena::Endian DNAE>
std::vector<uint8_t> AudioGroupPool::toData() const {
  athena::io::VectorWriter fo;

  PoolHeader<DNAE> head = {};
  head.write(fo);

  const uint32_t term = 0xffffffff;

  if (!m_soundMacros.empty()) {
    head.soundMacrosOffset = fo.position();
    for (const auto& p : m_soundMacros) {
      auto startPos = fo.position();
      ObjectHeader<DNAE> objHead = {};
      objHead.write(fo);
      p.second->template writeCmds<DNAE>(fo);
      objHead.size = fo.position() - startPos;
      objHead.objectId = p.first;
      fo.seek(startPos, athena::Begin);
      objHead.write(fo);
      fo.seek(startPos + objHead.size, athena::Begin);
    }
    athena::io::Write<athena::io::PropType::None>::Do<decltype(term), DNAE>({}, term, fo);
  }

  if (!m_tables.empty()) {
    head.tablesOffset = fo.position();
    for (const auto& p : m_tables) {
      auto startPos = fo.position();
      ObjectHeader<DNAE> objHead = {};
      objHead.write(fo);
      switch ((*p.second)->Isa()) {
      case ITable::Type::ADSR:
        static_cast<ADSR*>(p.second->get())->write(fo);
        break;
      case ITable::Type::ADSRDLS:
        static_cast<ADSRDLS*>(p.second->get())->write(fo);
        break;
      case ITable::Type::Curve: {
        const auto& data = static_cast<Curve*>(p.second->get())->data;
        fo.writeUBytes(data.data(), data.size());
        break;
      }
      default:
        break;
      }
      objHead.size = fo.position() - startPos;
      objHead.objectId = p.first;
      fo.seek(startPos, athena::Begin);
      objHead.write(fo);
      fo.seek(startPos + objHead.size, athena::Begin);
    }
    athena::io::Write<athena::io::PropType::None>::Do<decltype(term), DNAE>({}, term, fo);
  }

  if (!m_keymaps.empty()) {
    head.keymapsOffset = fo.position();
    for (const auto& p : m_keymaps) {
      auto startPos = fo.position();
      ObjectHeader<DNAE> objHead = {};
      objHead.write(fo);
      for (const auto& km : *p.second) {
        KeymapDNA<DNAE> kmData = km.toDNA<DNAE>();
        kmData.write(fo);
      }
      objHead.size = fo.position() - startPos;
      objHead.objectId = p.first;
      fo.seek(startPos, athena::Begin);
      objHead.write(fo);
      fo.seek(startPos + objHead.size, athena::Begin);
    }
    athena::io::Write<athena::io::PropType::None>::Do<decltype(term), DNAE>({}, term, fo);
  }

  if (!m_layers.empty()) {
    head.layersOffset = fo.position();
    for (const auto& p : m_layers) {
      auto startPos = fo.position();
      ObjectHeader<DNAE> objHead = {};
      objHead.write(fo);
      uint32_t count = p.second->size();
      athena::io::Write<athena::io::PropType::None>::Do<decltype(count), DNAE>({}, count, fo);
      for (const auto& lm : *p.second) {
        LayerMappingDNA<DNAE> lmData = lm.toDNA<DNAE>();
        lmData.write(fo);
      }
      objHead.size = fo.position() - startPos;
      objHead.objectId = p.first;
      fo.seek(startPos, athena::Begin);
      objHead.write(fo);
      fo.seek(startPos + objHead.size, athena::Begin);
    }
    athena::io::Write<athena::io::PropType::None>::Do<decltype(term), DNAE>({}, term, fo);
  }

  fo.seek(0, athena::Begin);
  head.write(fo);

  return fo.data();
}
template std::vector<uint8_t> AudioGroupPool::toData<athena::Big>() const;
template std::vector<uint8_t> AudioGroupPool::toData<athena::Little>() const;

template <>
void amuse::Curve::Enumerate<LittleDNA::Read>(athena::io::IStreamReader& r) {
  Log.report(logvisor::Fatal, fmt("Curve binary DNA read not supported"));
}

template <>
void amuse::Curve::Enumerate<LittleDNA::Write>(athena::io::IStreamWriter& w) {
  Log.report(logvisor::Fatal, fmt("Curve binary DNA write not supported"));
}

template <>
void amuse::Curve::Enumerate<LittleDNA::BinarySize>(size_t& sz) {
  Log.report(logvisor::Fatal, fmt("Curve binary DNA size not supported"));
}

template <>
void amuse::Curve::Enumerate<LittleDNA::ReadYaml>(athena::io::YAMLDocReader& r) {
  r.enumerate("data", data);
}

template <>
void amuse::Curve::Enumerate<LittleDNA::WriteYaml>(athena::io::YAMLDocWriter& w) {
  w.enumerate("data", data);
}

const char* amuse::Curve::DNAType() { return "amuse::Curve"; }

} // namespace amuse

#include "amuse/Common.hpp"

#ifndef _WIN32
#include <cstdio>
#include <memory>
#endif

#include <logvisor/logvisor.hpp>

using namespace std::literals;

namespace amuse {
static logvisor::Module Log("amuse");

bool Copy(const SystemChar* from, const SystemChar* to) {
#if _WIN32
  return CopyFileW(from, to, FALSE) != 0;
#else
  FILE* fi = fopen(from, "rb");
  if (!fi)
    return false;
  FILE* fo = fopen(to, "wb");
  if (!fo) {
    fclose(fi);
    return false;
  }
  std::unique_ptr<uint8_t[]> buf(new uint8_t[65536]);
  size_t readSz = 0;
  while ((readSz = fread(buf.get(), 1, 65536, fi)))
    fwrite(buf.get(), 1, readSz, fo);
  fclose(fi);
  fclose(fo);
  struct stat theStat;
  if (::stat(from, &theStat))
    return true;
#if __APPLE__
  struct timespec times[] = {theStat.st_atimespec, theStat.st_mtimespec};
#elif __SWITCH__
  struct timespec times[] = {theStat.st_atime, theStat.st_mtime};
#else
  struct timespec times[] = {theStat.st_atim, theStat.st_mtim};
#endif
  utimensat(AT_FDCWD, to, times, 0);
  return true;
#endif
}

#define DEFINE_ID_TYPE(type, typeName)                                                                                 \
  thread_local NameDB* type::CurNameDB = nullptr;                                                                      \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader & reader) {                        \
    id = reader.readUint16Little();                                                                                    \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter & writer) {                       \
    writer.writeUint16Little(id.id);                                                                                   \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Little>::Enumerate<BigDNA::BinarySize>(size_t & sz) {                                         \
    sz += 2;                                                                                                           \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader & reader) {                    \
    _read(reader);                                                                                                     \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter & writer) {                   \
    _write(writer);                                                                                                    \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader & reader) {                           \
    id = reader.readUint16Big();                                                                                       \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter & writer) {                          \
    writer.writeUint16Big(id.id);                                                                                      \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Big>::Enumerate<BigDNA::BinarySize>(size_t & sz) {                                            \
    sz += 2;                                                                                                           \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader & reader) {                       \
    _read(reader);                                                                                                     \
  }                                                                                                                    \
  template <>                                                                                                          \
  template <>                                                                                                          \
  void type##DNA<athena::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter & writer) {                      \
    _write(writer);                                                                                                    \
  }                                                                                                                    \
  template <athena::Endian DNAE>                                                                                       \
  void type##DNA<DNAE>::_read(athena::io::YAMLDocReader& r) {                                                          \
    std::string name = r.readString(nullptr);                                                                          \
    if (!type::CurNameDB)                                                                                              \
      Log.report(logvisor::Fatal, fmt("Unable to resolve " typeName " name {}, no database present"), name);           \
    if (name.empty()) {                                                                                                \
      id.id = 0xffff;                                                                                                  \
      return;                                                                                                          \
    }                                                                                                                  \
    id = type::CurNameDB->resolveIdFromName(name);                                                                     \
  }                                                                                                                    \
  template <athena::Endian DNAE>                                                                                       \
  void type##DNA<DNAE>::_write(athena::io::YAMLDocWriter& w) {                                                         \
    if (!type::CurNameDB)                                                                                              \
      Log.report(logvisor::Fatal, fmt("Unable to resolve " typeName " ID {}, no database present"), id);               \
    if (id.id == 0xffff)                                                                                               \
      return;                                                                                                          \
    std::string_view name = type::CurNameDB->resolveNameFromId(id);                                                    \
    if (!name.empty())                                                                                                 \
      w.writeString(nullptr, name);                                                                                    \
  }                                                                                                                    \
  template <athena::Endian DNAE>                                                                                       \
  const char* type##DNA<DNAE>::DNAType() {                                                                             \
    return "amuse::" #type "DNA";                                                                                      \
  }                                                                                                                    \
  template struct type##DNA<athena::Big>;                                                                              \
  template struct type##DNA<athena::Little>;

DEFINE_ID_TYPE(ObjectId, "object")
DEFINE_ID_TYPE(SoundMacroId, "SoundMacro")
DEFINE_ID_TYPE(SampleId, "sample")
DEFINE_ID_TYPE(TableId, "table")
DEFINE_ID_TYPE(KeymapId, "keymap")
DEFINE_ID_TYPE(LayersId, "layers")
DEFINE_ID_TYPE(SongId, "song")
DEFINE_ID_TYPE(SFXId, "sfx")
DEFINE_ID_TYPE(GroupId, "group")

template <>
template <>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  id = reader.readUint16Little();
}
template <>
template <>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  writer.writeUint16Little(id.id);
}
template <>
template <>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz) {
  sz += 2;
}
template <>
template <>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  _read(reader);
}
template <>
template <>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  _write(writer);
}
template <>
template <>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  id = reader.readUint16Big();
}
template <>
template <>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  writer.writeUint16Big(id.id);
}
template <>
template <>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz) {
  sz += 2;
}
template <>
template <>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  _read(reader);
}
template <>
template <>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  _write(writer);
}
template <athena::Endian DNAE>
void PageObjectIdDNA<DNAE>::_read(athena::io::YAMLDocReader& r) {
  std::string name = r.readString(nullptr);
  if (!KeymapId::CurNameDB || !LayersId::CurNameDB)
    Log.report(logvisor::Fatal, fmt("Unable to resolve keymap or layers name {}, no database present"), name);
  if (name.empty()) {
    id.id = 0xffff;
    return;
  }
  auto search = KeymapId::CurNameDB->m_stringToId.find(name);
  if (search == KeymapId::CurNameDB->m_stringToId.cend()) {
    search = LayersId::CurNameDB->m_stringToId.find(name);
    if (search == LayersId::CurNameDB->m_stringToId.cend()) {
      search = SoundMacroId::CurNameDB->m_stringToId.find(name);
      if (search == SoundMacroId::CurNameDB->m_stringToId.cend()) {
        Log.report(logvisor::Error, fmt("Unable to resolve name {}"), name);
        id.id = 0xffff;
        return;
      }
    }
  }
  id = search->second;
}
template <athena::Endian DNAE>
void PageObjectIdDNA<DNAE>::_write(athena::io::YAMLDocWriter& w) {
  if (!KeymapId::CurNameDB || !LayersId::CurNameDB)
    Log.report(logvisor::Fatal, fmt("Unable to resolve keymap or layers ID {}, no database present"), id);
  if (id.id == 0xffff)
    return;
  if (id.id & 0x8000) {
    std::string_view name = LayersId::CurNameDB->resolveNameFromId(id);
    if (!name.empty())
      w.writeString(nullptr, name);
  } else if (id.id & 0x4000) {
    std::string_view name = KeymapId::CurNameDB->resolveNameFromId(id);
    if (!name.empty())
      w.writeString(nullptr, name);
  } else {
    std::string_view name = SoundMacroId::CurNameDB->resolveNameFromId(id);
    if (!name.empty())
      w.writeString(nullptr, name);
  }
}
template <athena::Endian DNAE>
const char* PageObjectIdDNA<DNAE>::DNAType() {
  return "amuse::PageObjectIdDNA";
}
template struct PageObjectIdDNA<athena::Big>;
template struct PageObjectIdDNA<athena::Little>;

template <>
template <>
void SoundMacroStepDNA<athena::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  step = reader.readUint16Little();
}
template <>
template <>
void SoundMacroStepDNA<athena::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  writer.writeUint16Little(step);
}
template <>
template <>
void SoundMacroStepDNA<athena::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz) {
  sz += 2;
}
template <>
template <>
void SoundMacroStepDNA<athena::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  step = reader.readUint16(nullptr);
}
template <>
template <>
void SoundMacroStepDNA<athena::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  writer.writeUint16(nullptr, step);
}
template <>
template <>
void SoundMacroStepDNA<athena::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) {
  step = reader.readUint16Big();
}
template <>
template <>
void SoundMacroStepDNA<athena::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) {
  writer.writeUint16Big(step);
}
template <>
template <>
void SoundMacroStepDNA<athena::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz) {
  sz += 2;
}
template <>
template <>
void SoundMacroStepDNA<athena::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  step = reader.readUint16(nullptr);
}
template <>
template <>
void SoundMacroStepDNA<athena::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  writer.writeUint16(nullptr, step);
}
template <athena::Endian DNAE>
const char* SoundMacroStepDNA<DNAE>::DNAType() {
  return "amuse::SoundMacroStepDNA";
}
template struct SoundMacroStepDNA<athena::Big>;
template struct SoundMacroStepDNA<athena::Little>;

ObjectId NameDB::generateId(Type tp) const {
  uint16_t maxMatch = 0;
  if (tp == Type::Layer)
    maxMatch = 0x8000;
  else if (tp == Type::Keymap)
    maxMatch = 0x4000;
  for (const auto& p : m_idToString)
    if (p.first.id >= maxMatch)
      maxMatch = p.first.id + 1;
  return maxMatch;
}

std::string NameDB::generateName(ObjectId id, Type tp) {
  switch (tp) {
  case Type::SoundMacro:
    return fmt::format(fmt("macro{}"), id);
  case Type::Table:
    return fmt::format(fmt("table{}"), id);
  case Type::Keymap:
    return fmt::format(fmt("keymap{}"), id);
  case Type::Layer:
    return fmt::format(fmt("layers{}"), id);
  case Type::Song:
    return fmt::format(fmt("song{}"), id);
  case Type::SFX:
    return fmt::format(fmt("sfx{}"), id);
  case Type::Group:
    return fmt::format(fmt("group{}"), id);
  case Type::Sample:
    return fmt::format(fmt("sample{}"), id);
  default:
    return fmt::format(fmt("obj{}"), id);
  }
}

std::string NameDB::generateDefaultName(Type tp) const { return generateName(generateId(tp), tp); }

std::string_view NameDB::registerPair(std::string_view str, ObjectId id) {
  m_stringToId[std::string(str)] = id;
  return m_idToString.insert(std::make_pair(id, str)).first->second;
}

std::string_view NameDB::resolveNameFromId(ObjectId id) const {
  auto search = m_idToString.find(id);
  if (search == m_idToString.cend()) {
    Log.report(logvisor::Error, fmt("Unable to resolve ID {}"), id);
    return ""sv;
  }
  return search->second;
}

ObjectId NameDB::resolveIdFromName(std::string_view str) const {
  auto search = m_stringToId.find(std::string(str));
  if (search == m_stringToId.cend()) {
    Log.report(logvisor::Error, fmt("Unable to resolve name {}"), str);
    return {};
  }
  return search->second;
}

void NameDB::remove(ObjectId id) {
  auto search = m_idToString.find(id);
  if (search == m_idToString.cend())
    return;
  auto search2 = m_stringToId.find(search->second);
  if (search2 == m_stringToId.cend())
    return;
  m_idToString.erase(search);
  m_stringToId.erase(search2);
}

void NameDB::rename(ObjectId id, std::string_view str) {
  auto search = m_idToString.find(id);
  if (search == m_idToString.cend())
    return;
  if (search->second == str)
    return;
  auto search2 = m_stringToId.find(search->second);
  if (search2 == m_stringToId.cend())
    return;
#if __APPLE__
  std::swap(m_stringToId[std::string(str)], search2->second);
  m_stringToId.erase(search2);
#else
  auto nh = m_stringToId.extract(search2);
  nh.key() = str;
  m_stringToId.insert(std::move(nh));
#endif
  search->second = str;
}

template <>
void LittleUInt24::Enumerate<LittleDNA::Read>(athena::io::IStreamReader& reader) {
  union {
    atUint32 val;
    char bytes[4];
  } data = {};
  reader.readBytesToBuf(data.bytes, 3);
  val = SLittle(data.val);
}

template <>
void LittleUInt24::Enumerate<LittleDNA::Write>(athena::io::IStreamWriter& writer) {
  union {
    atUint32 val;
    char bytes[4];
  } data;
  data.val = SLittle(val);
  writer.writeBytes(data.bytes, 3);
}

template <>
void LittleUInt24::Enumerate<LittleDNA::BinarySize>(size_t& sz) {
  sz += 3;
}

template <>
void LittleUInt24::Enumerate<LittleDNA::ReadYaml>(athena::io::YAMLDocReader& reader) {
  val = reader.readUint32(nullptr);
}

template <>
void LittleUInt24::Enumerate<LittleDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) {
  writer.writeUint32(nullptr, val);
}

const char* LittleUInt24::DNAType() { return "amuse::LittleUInt24"; }

} // namespace amuse

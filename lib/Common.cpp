#include "amuse/Common.hpp"
#include "logvisor/logvisor.hpp"

namespace amuse
{
static logvisor::Module Log("amuse");

#define DEFINE_ID_TYPE(type, typeName) \
thread_local NameDB* type::CurNameDB = nullptr; \
template<> template<> \
void type##DNA<athena::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) \
{ \
    id = reader.readUint16Little(); \
} \
template<> template<> \
void type##DNA<athena::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) \
{ \
    writer.writeUint16Little(id); \
} \
template<> template<> \
void type##DNA<athena::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz) \
{ \
    sz += 2; \
} \
template<> template<> \
void type##DNA<athena::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) \
{ \
    _read(reader); \
} \
template<> template<> \
void type##DNA<athena::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) \
{ \
    _write(writer); \
} \
template<> template<> \
void type##DNA<athena::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader) \
{ \
    id = reader.readUint16Big(); \
} \
template<> template<> \
void type##DNA<athena::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer) \
{ \
    writer.writeUint16Big(id); \
} \
template<> template<> \
void type##DNA<athena::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz) \
{ \
    sz += 2; \
} \
template<> template<> \
void type##DNA<athena::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader) \
{ \
    _read(reader); \
} \
template<> template<> \
void type##DNA<athena::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer) \
{ \
    _write(writer); \
} \
template <athena::Endian DNAE> \
void type##DNA<DNAE>::_read(athena::io::YAMLDocReader& r) \
{ \
    std::string name = r.readString(nullptr); \
    if (!type::CurNameDB) \
        Log.report(logvisor::Fatal, "Unable to resolve " typeName " name %s, no database present", name.c_str()); \
    if (name.empty()) \
    { \
        id.id = 0xffff; \
        return; \
    } \
    id = type::CurNameDB->resolveIdFromName(name); \
} \
template <athena::Endian DNAE> \
void type##DNA<DNAE>::_write(athena::io::YAMLDocWriter& w) \
{ \
    if (!type::CurNameDB) \
        Log.report(logvisor::Fatal, "Unable to resolve " typeName " ID %d, no database present", id.id); \
    if (id.id == 0xffff) \
        return; \
    std::string_view name = type::CurNameDB->resolveNameFromId(id); \
    w.writeString(nullptr, name); \
} \
template <athena::Endian DNAE> \
const char* type##DNA<DNAE>::DNAType() \
{ \
    return "amuse::" #type "DNA"; \
} \
template struct type##DNA<athena::Big>; \
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

template<> template<>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Little();
}
template<> template<>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Little(id);
}
template<> template<>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}
template<> template<>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}
template<> template<>
void PageObjectIdDNA<athena::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}
template<> template<>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Big();
}
template<> template<>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Big(id);
}
template<> template<>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}
template<> template<>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}
template<> template<>
void PageObjectIdDNA<athena::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}
template <athena::Endian DNAE>
void PageObjectIdDNA<DNAE>::_read(athena::io::YAMLDocReader& r)
{
    std::string name = r.readString(nullptr);
    if (!KeymapId::CurNameDB || !LayersId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve keymap or layers name %s, no database present", name.c_str());
    if (name.empty())
    {
        id.id = 0xffff;
        return;
    }
    auto search = KeymapId::CurNameDB->m_stringToId.find(name);
    if (search == KeymapId::CurNameDB->m_stringToId.cend())
    {
        search = LayersId::CurNameDB->m_stringToId.find(name);
        if (search == LayersId::CurNameDB->m_stringToId.cend())
            Log.report(logvisor::Fatal, "Unable to resolve name %s", name.c_str());
    }
    id = search->second;
}
template <athena::Endian DNAE>
void PageObjectIdDNA<DNAE>::_write(athena::io::YAMLDocWriter& w)
{
    if (!KeymapId::CurNameDB || !LayersId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve keymap or layers ID %d, no database present", id.id);
    if (id.id == 0xffff)
        return;
    if (id.id & 0x8000)
    {
        std::string_view name = LayersId::CurNameDB->resolveNameFromId(id);
        w.writeString(nullptr, name);
    }
    else
    {
        std::string_view name = KeymapId::CurNameDB->resolveNameFromId(id);
        w.writeString(nullptr, name);
    }
}
template <athena::Endian DNAE>
const char* PageObjectIdDNA<DNAE>::DNAType()
{
    return "amuse::PageObjectIdDNA";
}
template struct PageObjectIdDNA<athena::Big>;
template struct PageObjectIdDNA<athena::Little>;

ObjectId NameDB::generateId(Type tp) const
{
    uint16_t maxMatch = uint16_t(tp == Type::Layer ? 0x8000 : 0);
    for (const auto& p : m_idToString)
        if (p.first >= maxMatch)
            maxMatch = p.first + 1;
    return maxMatch;
}

std::string NameDB::generateName(ObjectId id, Type tp)
{
    char name[32];
    switch (tp)
    {
    case Type::SoundMacro:
        snprintf(name, 32, "macro%04X", id.id);
        break;
    case Type::Table:
        snprintf(name, 32, "table%04X", id.id);
        break;
    case Type::Keymap:
        snprintf(name, 32, "keymap%04X", id.id);
        break;
    case Type::Layer:
        snprintf(name, 32, "layers%04X", id.id);
        break;
    case Type::Song:
        snprintf(name, 32, "song%04X", id.id);
        break;
    case Type::SFX:
        snprintf(name, 32, "sfx%04X", id.id);
        break;
    case Type::Group:
        snprintf(name, 32, "group%04X", id.id);
        break;
    case Type::Sample:
        snprintf(name, 32, "sample%04X", id.id);
        break;
    default:
        snprintf(name, 32, "obj%04X", id.id);
        break;
    }
    return name;
}

std::string_view NameDB::registerPair(std::string_view str, ObjectId id)
{
    m_stringToId[std::string(str)] = id;
    return m_idToString.insert(std::make_pair(id, str)).first->second;
}

std::string_view NameDB::resolveNameFromId(ObjectId id) const
{
    auto search = m_idToString.find(id);
    if (search == m_idToString.cend())
        Log.report(logvisor::Fatal, "Unable to resolve ID 0x%04X", id.id);
    return search->second;
}

ObjectId NameDB::resolveIdFromName(std::string_view str) const
{
    auto search = m_stringToId.find(std::string(str));
    if (search == m_stringToId.cend())
        Log.report(logvisor::Fatal, "Unable to resolve name %s", str.data());
    return search->second;
}

template<>
void LittleUInt24::Enumerate<LittleDNA::Read>(athena::io::IStreamReader& reader)
{
    union
    {
        atUint32 val;
        char bytes[4];
    } data = {};
    reader.readBytesToBuf(data.bytes, 3);
    val = SLittle(data.val);
}

template<>
void LittleUInt24::Enumerate<LittleDNA::Write>(athena::io::IStreamWriter& writer)
{
    union
    {
        atUint32 val;
        char bytes[4];
    } data;
    data.val = SLittle(val);
    writer.writeBytes(data.bytes, 3);
}

template<>
void LittleUInt24::Enumerate<LittleDNA::BinarySize>(size_t& sz)
{
    sz += 3;
}

template<>
void LittleUInt24::Enumerate<LittleDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    val = reader.readUint32(nullptr);
}

template<>
void LittleUInt24::Enumerate<LittleDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    writer.writeUint32(nullptr, val);
}

const char* LittleUInt24::DNAType()
{
    return "amuse::LittleUInt24";
}

}

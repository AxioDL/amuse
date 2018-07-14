#include "amuse/Common.hpp"
#include "logvisor/logvisor.hpp"

namespace amuse
{
static logvisor::Module Log("amuse");

thread_local NameDB* ObjectId::CurNameDB = nullptr;
thread_local NameDB* SampleId::CurNameDB = nullptr;
thread_local NameDB* SongId::CurNameDB = nullptr;
thread_local NameDB* SFXId::CurNameDB = nullptr;

template<> template<>
void ObjectIdDNA<athena::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Little();
}

template<> template<>
void ObjectIdDNA<athena::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Little(id);
}

template<> template<>
void ObjectIdDNA<athena::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}

template<> template<>
void ObjectIdDNA<athena::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}

template<> template<>
void ObjectIdDNA<athena::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}

template<> template<>
void ObjectIdDNA<athena::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Big();
}

template<> template<>
void ObjectIdDNA<athena::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Big(id);
}

template<> template<>
void ObjectIdDNA<athena::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}

template<> template<>
void ObjectIdDNA<athena::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}

template<> template<>
void ObjectIdDNA<athena::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}

template <athena::Endian DNAE>
void ObjectIdDNA<DNAE>::_read(athena::io::YAMLDocReader& r)
{
    std::string name = r.readString(nullptr);
    if (!ObjectId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve object name %s, no database present", name.c_str());
    id = ObjectId::CurNameDB->resolveIdFromName(name);
}

template <athena::Endian DNAE>
void ObjectIdDNA<DNAE>::_write(athena::io::YAMLDocWriter& w)
{
    if (!ObjectId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve object ID %d, no database present", id.id);
    std::string_view name = ObjectId::CurNameDB->resolveNameFromId(id);
    w.writeString(nullptr, name);
}

template <athena::Endian DNAE>
const char* ObjectIdDNA<DNAE>::DNAType()
{
    return "amuse::ObjectId";
}

template<> template<>
void SampleIdDNA<athena::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Little();
}

template<> template<>
void SampleIdDNA<athena::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Little(id);
}

template<> template<>
void SampleIdDNA<athena::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}

template<> template<>
void SampleIdDNA<athena::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}

template<> template<>
void SampleIdDNA<athena::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}

template<> template<>
void SampleIdDNA<athena::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Big();
}

template<> template<>
void SampleIdDNA<athena::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Big(id);
}

template<> template<>
void SampleIdDNA<athena::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}

template<> template<>
void SampleIdDNA<athena::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}

template<> template<>
void SampleIdDNA<athena::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}

template <athena::Endian DNAE>
void SampleIdDNA<DNAE>::_read(athena::io::YAMLDocReader& r)
{
    std::string name = r.readString(nullptr);
    if (!SampleId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve sample name %s, no database present", name.c_str());
    id = SampleId::CurNameDB->resolveIdFromName(name);
}

template <athena::Endian DNAE>
void SampleIdDNA<DNAE>::_write(athena::io::YAMLDocWriter& w)
{
    if (!SampleId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve sample ID %d, no database present", id.id);
    std::string_view name = SampleId::CurNameDB->resolveNameFromId(id);
    w.writeString(nullptr, name);
}

template <athena::Endian DNAE>
const char* SampleIdDNA<DNAE>::DNAType()
{
    return "amuse::SampleId";
}

template<> template<>
void SongIdDNA<athena::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Little();
}

template<> template<>
void SongIdDNA<athena::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Little(id);
}

template<> template<>
void SongIdDNA<athena::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}

template<> template<>
void SongIdDNA<athena::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}

template<> template<>
void SongIdDNA<athena::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}

template<> template<>
void SongIdDNA<athena::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Big();
}

template<> template<>
void SongIdDNA<athena::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Big(id);
}

template<> template<>
void SongIdDNA<athena::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}

template<> template<>
void SongIdDNA<athena::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}

template<> template<>
void SongIdDNA<athena::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}

template <athena::Endian DNAE>
void SongIdDNA<DNAE>::_read(athena::io::YAMLDocReader& r)
{
    std::string name = r.readString(nullptr);
    if (!SongId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve song name %s, no database present", name.c_str());
    id = SongId::CurNameDB->resolveIdFromName(name);
}

template <athena::Endian DNAE>
void SongIdDNA<DNAE>::_write(athena::io::YAMLDocWriter& w)
{
    if (!SongId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve song ID %d, no database present", id.id);
    std::string_view name = SongId::CurNameDB->resolveNameFromId(id);
    w.writeString(nullptr, name);
}

template <athena::Endian DNAE>
const char* SongIdDNA<DNAE>::DNAType()
{
    return "amuse::SongId";
}

template<> template<>
void SFXIdDNA<athena::Little>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Little();
}

template<> template<>
void SFXIdDNA<athena::Little>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Little(id);
}

template<> template<>
void SFXIdDNA<athena::Little>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}

template<> template<>
void SFXIdDNA<athena::Little>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}

template<> template<>
void SFXIdDNA<athena::Little>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}

template<> template<>
void SFXIdDNA<athena::Big>::Enumerate<BigDNA::Read>(athena::io::IStreamReader& reader)
{
    id = reader.readUint16Big();
}

template<> template<>
void SFXIdDNA<athena::Big>::Enumerate<BigDNA::Write>(athena::io::IStreamWriter& writer)
{
    writer.writeUint16Big(id);
}

template<> template<>
void SFXIdDNA<athena::Big>::Enumerate<BigDNA::BinarySize>(size_t& sz)
{
    sz += 2;
}

template<> template<>
void SFXIdDNA<athena::Big>::Enumerate<BigDNA::ReadYaml>(athena::io::YAMLDocReader& reader)
{
    _read(reader);
}

template<> template<>
void SFXIdDNA<athena::Big>::Enumerate<BigDNA::WriteYaml>(athena::io::YAMLDocWriter& writer)
{
    _write(writer);
}

template <athena::Endian DNAE>
void SFXIdDNA<DNAE>::_read(athena::io::YAMLDocReader& r)
{
    std::string name = r.readString(nullptr);
    if (!SFXId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve song name %s, no database present", name.c_str());
    id = SFXId::CurNameDB->resolveIdFromName(name);
}

template <athena::Endian DNAE>
void SFXIdDNA<DNAE>::_write(athena::io::YAMLDocWriter& w)
{
    if (!SFXId::CurNameDB)
        Log.report(logvisor::Fatal, "Unable to resolve song ID %d, no database present", id.id);
    std::string_view name = SFXId::CurNameDB->resolveNameFromId(id);
    w.writeString(nullptr, name);
}

template <athena::Endian DNAE>
const char* SFXIdDNA<DNAE>::DNAType()
{
    return "amuse::SFXId";
}

ObjectId NameDB::generateId(Type tp)
{
    uint16_t upperMatch = uint16_t(tp) << 8;
    uint16_t maxMatch = 0;
    for (const auto& p : m_idToString)
        if ((p.first & 0xff00) == upperMatch && (p.first & 0xff) >= maxMatch)
            maxMatch = (p.first & 0xff) + 1;
    return upperMatch | maxMatch;
}

std::string NameDB::generateName(ObjectId id)
{
    Type tp = Type(id.id >> 8);
    char name[32];
    switch (tp)
    {
    case Type::SoundMacro:
        snprintf(name, 32, "macro%d", id.id & 0xff);
        break;
    case Type::Table:
        snprintf(name, 32, "table%d", id.id & 0xff);
        break;
    case Type::Keymap:
        snprintf(name, 32, "keymap%d", id.id & 0xff);
        break;
    case Type::Layer:
        snprintf(name, 32, "layers%d", id.id & 0xff);
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
        Log.report(logvisor::Fatal, "Unable to resolve ID %d", id.id);
    return search->second;
}

ObjectId NameDB::resolveIdFromName(std::string_view str) const
{
    auto search = m_stringToId.find(std::string(str));
    if (search == m_stringToId.cend())
        Log.report(logvisor::Fatal, "Unable to resolve name %s", str.data());
    return search->second;
}

template struct ObjectIdDNA<athena::Big>;
template struct ObjectIdDNA<athena::Little>;

template struct SampleIdDNA<athena::Big>;
template struct SampleIdDNA<athena::Little>;

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

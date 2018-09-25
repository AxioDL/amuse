#ifndef __AMUSE_COMMON_HPP__
#define __AMUSE_COMMON_HPP__

#include <algorithm>
#include <limits>
#include <cstdio>
#include <cstdint>
#include <cstdarg>
#include <string>
#include <string_view>
#include <cstring>
#include <atomic>
#include <unordered_map>
#include "athena/DNA.hpp"

#ifndef _WIN32
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <Windows.h>
#endif

#if __SWITCH__
#undef _S

#include "switch_math.hpp"
#endif

#undef min
#undef max

constexpr float NativeSampleRate = 32000.0f;

namespace amuse
{
struct NameDB;

using BigDNA = athena::io::DNA<athena::Big>;
using LittleDNA = athena::io::DNA<athena::Little>;
using BigDNAV = athena::io::DNAVYaml<athena::Big>;
using LittleDNAV = athena::io::DNAVYaml<athena::Little>;

/** Common ID structure statically tagging
 *  SoundMacros, Tables, Keymaps, Layers, Samples, SFX, Songs */
struct ObjectId
{
    uint16_t id = 0xffff;
    operator uint16_t() const { return id; }
    ObjectId() = default;
    ObjectId(uint16_t idIn) : id(idIn) {}
    ObjectId& operator=(uint16_t idIn) { id = idIn; return *this; }
    static thread_local NameDB* CurNameDB;
};
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
ObjectIdDNA : BigDNA
{
    AT_DECL_EXPLICIT_DNA_YAML
    void _read(athena::io::YAMLDocReader& r);
    void _write(athena::io::YAMLDocWriter& w);
    ObjectId id;
    ObjectIdDNA() = default;
    ObjectIdDNA(ObjectId idIn) : id(idIn) {}
    operator ObjectId() const { return id; }
};

#define DECL_ID_TYPE(type) \
struct type : ObjectId \
{ \
    using ObjectId::ObjectId; \
    type() = default; \
    type(const ObjectId& id) : ObjectId(id) {} \
    static thread_local NameDB* CurNameDB; \
}; \
template <athena::Endian DNAEn> \
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little) \
type##DNA : BigDNA \
{ \
    AT_DECL_EXPLICIT_DNA_YAML \
    void _read(athena::io::YAMLDocReader& r); \
    void _write(athena::io::YAMLDocWriter& w); \
    type id; \
    type##DNA() = default; \
    type##DNA(type idIn) : id(idIn) {} \
    operator type() const { return id; } \
};
DECL_ID_TYPE(SoundMacroId)
DECL_ID_TYPE(SampleId)
DECL_ID_TYPE(TableId)
DECL_ID_TYPE(KeymapId)
DECL_ID_TYPE(LayersId)
DECL_ID_TYPE(SongId)
DECL_ID_TYPE(SFXId)
DECL_ID_TYPE(GroupId)

/* MusyX has object polymorphism between Keymaps and Layers when
 * referenced by a song group's page object. When the upper bit is set,
 * this indicates a layer type. */
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
PageObjectIdDNA : BigDNA
{
    AT_DECL_EXPLICIT_DNA_YAML
    void _read(athena::io::YAMLDocReader& r);
    void _write(athena::io::YAMLDocWriter& w);
    ObjectId id;
    PageObjectIdDNA() = default;
    PageObjectIdDNA(ObjectId idIn) : id(idIn) {}
    operator ObjectId() const { return id; }
};

struct SoundMacroStep
{
    uint16_t step = 0;
    operator uint16_t() const { return step; }
    SoundMacroStep() = default;
    SoundMacroStep(uint16_t idIn) : step(idIn) {}
    SoundMacroStep& operator=(uint16_t idIn) { step = idIn; return *this; }
};

template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
SoundMacroStepDNA : BigDNA
{
    AT_DECL_EXPLICIT_DNA_YAML
    SoundMacroStep step;
    SoundMacroStepDNA() = default;
    SoundMacroStepDNA(SoundMacroStep idIn) : step(idIn) {}
    operator SoundMacroStep() const { return step; }
};

struct LittleUInt24 : LittleDNA
{
    AT_DECL_EXPLICIT_DNA_YAML
    atUint32 val;
    operator uint32_t() const { return val; }
    LittleUInt24() = default;
    LittleUInt24(uint32_t valIn) : val(valIn) {}
    LittleUInt24& operator=(uint32_t valIn) { val = valIn; return *this; }
};

class IObj
{
    std::atomic_int m_refCount = {0};
protected:
    virtual ~IObj() = default;
public:
    void increment() { m_refCount++; }
    void decrement()
    {
        if (m_refCount.fetch_sub(1) == 1)
            delete this;
    }
};

template<class SubCls>
class ObjWrapper : public IObj
{
    SubCls m_obj;
public:
    template <class... _Args>
    ObjWrapper(_Args&&... args) : m_obj(std::forward<_Args>(args)...) {}
    SubCls* get() { return &m_obj; }
    const SubCls* get() const { return &m_obj; }
};

template<class SubCls>
class ObjTokenBase
{
protected:
    IObj* m_obj = nullptr;
    ObjTokenBase(IObj* obj) : m_obj(obj) { if (m_obj) m_obj->increment(); }
public:
    ObjTokenBase() = default;
    ObjTokenBase(const ObjTokenBase& other) : m_obj(other.m_obj) { if (m_obj) m_obj->increment(); }
    ObjTokenBase(ObjTokenBase&& other) : m_obj(other.m_obj) { other.m_obj = nullptr; }
    ObjTokenBase& operator=(const ObjTokenBase& other)
    { if (m_obj) m_obj->decrement(); m_obj = other.m_obj; if (m_obj) m_obj->increment(); return *this; }
    ObjTokenBase& operator=(ObjTokenBase&& other)
    { if (m_obj) m_obj->decrement(); m_obj = other.m_obj; other.m_obj = nullptr; return *this; }
    ~ObjTokenBase() { if (m_obj) m_obj->decrement(); }
    bool operator==(const ObjTokenBase& other) const { return m_obj == other.m_obj; }
    bool operator!=(const ObjTokenBase& other) const { return m_obj != other.m_obj; }
    bool operator<(const ObjTokenBase& other) const { return m_obj < other.m_obj; }
    bool operator>(const ObjTokenBase& other) const { return m_obj > other.m_obj; }
    operator bool() const { return m_obj != nullptr; }
    void reset() { if (m_obj) m_obj->decrement(); m_obj = nullptr; }
};

template<class SubCls, class Enable = void>
class ObjToken : public ObjTokenBase<SubCls>
{
    IObj*& _obj() { return ObjTokenBase<SubCls>::m_obj; }
    IObj*const& _obj() const { return ObjTokenBase<SubCls>::m_obj; }
public:
    using ObjTokenBase<SubCls>::ObjTokenBase;
    ObjToken() = default;
    ObjToken(ObjWrapper<SubCls>* obj) : ObjTokenBase<SubCls>(obj) {}
    ObjToken& operator=(ObjWrapper<SubCls>* obj)
    { if (_obj()) _obj()->decrement(); _obj() = obj; if (_obj()) _obj()->increment(); return *this; }
    SubCls* get() const { return static_cast<ObjWrapper<SubCls>*>(_obj())->get(); }
    SubCls* operator->() const { return get(); }
    SubCls& operator*() const { return *get(); }
};

template<class SubCls>
class ObjToken<SubCls, typename std::enable_if_t<std::is_base_of_v<IObj, SubCls>>> : public ObjTokenBase<SubCls>
{
    IObj*& _obj() { return ObjTokenBase<SubCls>::m_obj; }
    IObj*const& _obj() const { return ObjTokenBase<SubCls>::m_obj; }
public:
    using ObjTokenBase<SubCls>::ObjTokenBase;
    ObjToken() = default;
    ObjToken(IObj* obj) : ObjTokenBase<SubCls>(obj) {}
    ObjToken& operator=(IObj* obj)
    { if (_obj()) _obj()->decrement(); _obj() = obj; if (_obj()) _obj()->increment(); return *this; }
    SubCls* get() const { return static_cast<SubCls*>(_obj()); }
    SubCls* operator->() const { return get(); }
    SubCls& operator*() const { return *get(); }
    template <class T>
    T* cast() const { return static_cast<T*>(_obj()); }
};

/* ONLY USE WITH CLASSES DERIVED FROM IOBJ!
 * Bypasses type_traits tests for incomplete type definitions. */
template<class SubCls>
class IObjToken : public ObjTokenBase<SubCls>
{
    IObj*& _obj() { return ObjTokenBase<SubCls>::m_obj; }
    IObj*const& _obj() const { return ObjTokenBase<SubCls>::m_obj; }
public:
    using ObjTokenBase<SubCls>::ObjTokenBase;
    IObjToken() = default;
    IObjToken(IObj* obj) : ObjTokenBase<SubCls>(obj) {}
    IObjToken& operator=(IObj* obj)
    { if (_obj()) _obj()->decrement(); _obj() = obj; if (_obj()) _obj()->increment(); return *this; }
    SubCls* get() const { return static_cast<SubCls*>(_obj()); }
    SubCls* operator->() const { return get(); }
    SubCls& operator*() const { return *get(); }
    template <class T>
    T* cast() const { return static_cast<T*>(_obj()); }
};

template <class Tp, class... _Args>
inline typename std::enable_if_t<std::is_base_of_v<IObj, Tp>, ObjToken<Tp>> MakeObj(_Args&&... args)
{
    return new Tp(std::forward<_Args>(args)...);
}

template <class Tp, class... _Args>
inline typename std::enable_if_t<!std::is_base_of_v<IObj, Tp>, ObjToken<Tp>> MakeObj(_Args&&... args)
{
    return new ObjWrapper<Tp>(std::forward<_Args>(args)...);
}

#ifndef PRISize
#ifdef _MSC_VER
#define PRISize "Iu"
#else
#define PRISize "zu"
#endif
#endif

#ifdef _WIN32
using SystemString = std::wstring;
using SystemStringView = std::wstring_view;
using SystemChar = wchar_t;
#ifndef _S
#define _S(val) L##val
#endif
typedef struct _stat Sstat;
static inline int Mkdir(const wchar_t* path, int) { return _wmkdir(path); }
static inline int Stat(const wchar_t* path, Sstat* statout) { return _wstat(path, statout); }
#else
using SystemString = std::string;
using SystemStringView = std::string_view;
using SystemChar = char;
#ifndef _S
#define _S(val) val
#endif
typedef struct stat Sstat;
static inline int Mkdir(const char* path, mode_t mode) { return mkdir(path, mode); }
static inline int Stat(const char* path, Sstat* statout) { return stat(path, statout); }
#endif

static inline int Rename(const SystemChar* oldpath, const SystemChar* newpath)
{
#if _WIN32
    //return _wrename(oldpath, newpath);
    return MoveFileExW(oldpath, newpath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0;
#else
    return rename(oldpath, newpath);
#endif
}

#if _WIN32
static inline int CompareCaseInsensitive(const char* a, const char* b) { return _stricmp(a, b); }
#endif

static inline int CompareCaseInsensitive(const SystemChar* a, const SystemChar* b)
{
#if _WIN32
    return _wcsicmp(a, b);
#else
    return strcasecmp(a, b);
#endif
}

template <typename T>
static inline T clamp(T a, T val, T b)
{
    return std::max<T>(a, std::min<T>(b, val));
}

template <typename T>
inline T ClampFull(float in)
{    
    if (std::is_floating_point<T>())
    {
        return in;
    }
    else
    {
        constexpr T MAX = std::numeric_limits<T>::max();
        constexpr T MIN = std::numeric_limits<T>::min();
        
        if (in < MIN)
            return MIN;
        else if (in > MAX)
            return MAX;
        else
            return in;
    }
}

#ifndef M_PIF
#define M_PIF 3.14159265358979323846f /* pi */
#endif

#if __GNUC__
__attribute__((__format__(__printf__, 1, 2)))
#endif
static inline void
Printf(const SystemChar* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
#if _WIN32
    vwprintf(fmt, args);
#else
    vprintf(fmt, args);
#endif
    va_end(args);
}

#if __GNUC__
__attribute__((__format__(__printf__, 3, 4)))
#endif
static inline void
SNPrintf(SystemChar* str, size_t maxlen, const SystemChar* format, ...)
{
    va_list va;
    va_start(va, format);
#if _WIN32
    _vsnwprintf(str, maxlen, format, va);
#else
    vsnprintf(str, maxlen, format, va);
#endif
    va_end(va);
}

static inline const SystemChar* StrRChr(const SystemChar* str, SystemChar ch)
{
#if _WIN32
    return wcsrchr(str, ch);
#else
    return strrchr(str, ch);
#endif
}

static inline SystemChar* StrRChr(SystemChar* str, SystemChar ch)
{
#if _WIN32
    return wcsrchr(str, ch);
#else
    return strrchr(str, ch);
#endif
}

static inline int FSeek(FILE* fp, int64_t offset, int whence)
{
#if _WIN32
    return _fseeki64(fp, offset, whence);
#elif __APPLE__ || __FreeBSD__
    return fseeko(fp, offset, whence);
#else
    return fseeko64(fp, offset, whence);
#endif
}

static inline int64_t FTell(FILE* fp)
{
#if _WIN32
    return _ftelli64(fp);
#elif __APPLE__ || __FreeBSD__
    return ftello(fp);
#else
    return ftello64(fp);
#endif
}

static inline FILE* FOpen(const SystemChar* path, const SystemChar* mode)
{
#if _WIN32
    FILE* fp = _wfopen(path, mode);
    if (!fp)
        return nullptr;
#else
    FILE* fp = fopen(path, mode);
    if (!fp)
        return nullptr;
#endif
    return fp;
}

static inline void Unlink(const SystemChar* file)
{
#if _WIN32
    _wunlink(file);
#else
    unlink(file);
#endif
}

bool Copy(const SystemChar* from, const SystemChar* to);

#undef bswap16
#undef bswap32
#undef bswap64

/* Type-sensitive byte swappers */
template <typename T>
static inline T bswap16(T val)
{
#if __GNUC__
    return __builtin_bswap16(val);
#elif _WIN32
    return _byteswap_ushort(val);
#else
    return (val = (val << 8) | ((val >> 8) & 0xFF));
#endif
}

template <typename T>
static inline T bswap32(T val)
{
#if __GNUC__
    return __builtin_bswap32(val);
#elif _WIN32
    return _byteswap_ulong(val);
#else
    val = (val & 0x0000FFFF) << 16 | (val & 0xFFFF0000) >> 16;
    val = (val & 0x00FF00FF) << 8 | (val & 0xFF00FF00) >> 8;
    return val;
#endif
}

template <typename T>
static inline T bswap64(T val)
{
#if __GNUC__
    return __builtin_bswap64(val);
#elif _WIN32
    return _byteswap_uint64(val);
#else
    return ((val & 0xFF00000000000000ULL) >> 56) | ((val & 0x00FF000000000000ULL) >> 40) |
           ((val & 0x0000FF0000000000ULL) >> 24) | ((val & 0x000000FF00000000ULL) >> 8) |
           ((val & 0x00000000FF000000ULL) << 8) | ((val & 0x0000000000FF0000ULL) << 24) |
           ((val & 0x000000000000FF00ULL) << 40) | ((val & 0x00000000000000FFULL) << 56);
#endif
}

#if __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
static inline int16_t SBig(int16_t val) { return bswap16(val); }
static inline uint16_t SBig(uint16_t val) { return bswap16(val); }
static inline int32_t SBig(int32_t val) { return bswap32(val); }
static inline uint32_t SBig(uint32_t val) { return bswap32(val); }
static inline int64_t SBig(int64_t val) { return bswap64(val); }
static inline uint64_t SBig(uint64_t val) { return bswap64(val); }
static inline float SBig(float val)
{
    int32_t ival = bswap32(*((int32_t*)(&val)));
    return *((float*)(&ival));
}
static inline double SBig(double val)
{
    int64_t ival = bswap64(*((int64_t*)(&val)));
    return *((double*)(&ival));
}
#ifndef SBIG
#define SBIG(q) (((q)&0x000000FF) << 24 | ((q)&0x0000FF00) << 8 | ((q)&0x00FF0000) >> 8 | ((q)&0xFF000000) >> 24)
#endif

static inline int16_t SLittle(int16_t val) { return val; }
static inline uint16_t SLittle(uint16_t val) { return val; }
static inline int32_t SLittle(int32_t val) { return val; }
static inline uint32_t SLittle(uint32_t val) { return val; }
static inline int64_t SLittle(int64_t val) { return val; }
static inline uint64_t SLittle(uint64_t val) { return val; }
static inline float SLittle(float val) { return val; }
static inline double SLittle(double val) { return val; }
#ifndef SLITTLE
#define SLITTLE(q) (q)
#endif
#else
static inline int16_t SLittle(int16_t val) { return bswap16(val); }
static inline uint16_t SLittle(uint16_t val) { return bswap16(val); }
static inline int32_t SLittle(int32_t val) { return bswap32(val); }
static inline uint32_t SLittle(uint32_t val) { return bswap32(val); }
static inline int64_t SLittle(int64_t val) { return bswap64(val); }
static inline uint64_t SLittle(uint64_t val) { return bswap64(val); }
static inline float SLittle(float val)
{
    int32_t ival = bswap32(*((int32_t*)(&val)));
    return *((float*)(&ival));
}
static inline double SLittle(double val)
{
    int64_t ival = bswap64(*((int64_t*)(&val)));
    return *((double*)(&ival));
}
#ifndef SLITTLE
#define SLITTLE(q) (((q)&0x000000FF) << 24 | ((q)&0x0000FF00) << 8 | ((q)&0x00FF0000) >> 8 | ((q)&0xFF000000) >> 24)
#endif

static inline int16_t SBig(int16_t val) { return val; }
static inline uint16_t SBig(uint16_t val) { return val; }
static inline int32_t SBig(int32_t val) { return val; }
static inline uint32_t SBig(uint32_t val) { return val; }
static inline int64_t SBig(int64_t val) { return val; }
static inline uint64_t SBig(uint64_t val) { return val; }
static inline float SBig(float val) { return val; }
static inline double SBig(double val) { return val; }
#ifndef SBIG
#define SBIG(q) (q)
#endif
#endif

/** Versioned data format to interpret */
enum class DataFormat
{
    GCN,
    N64,
    PC
};

/** Meta-type for selecting GameCube (MusyX 2.0) data formats */
struct GCNDataTag
{
};

/** Meta-type for selecting N64 (MusyX 1.0) data formats */
struct N64DataTag
{
};

/** Meta-type for selecting PC (MusyX 1.0) data formats */
struct PCDataTag
{
};

template <class T>
static std::vector<std::pair<typename T::key_type,
    std::reference_wrapper<typename T::mapped_type>>> SortUnorderedMap(T& um)
{
    std::vector<std::pair<typename T::key_type, std::reference_wrapper<typename T::mapped_type>>> ret;
    ret.reserve(um.size());
    for (auto& p : um)
        ret.emplace_back(p.first, p.second);
    std::sort(ret.begin(), ret.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return ret;
}

template <class T>
static std::vector<std::pair<typename T::key_type,
    std::reference_wrapper<const typename T::mapped_type>>> SortUnorderedMap(const T& um)
{
    std::vector<std::pair<typename T::key_type, std::reference_wrapper<const typename T::mapped_type>>> ret;
    ret.reserve(um.size());
    for (const auto& p : um)
        ret.emplace_back(p.first, p.second);
    std::sort(ret.begin(), ret.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    return ret;
}

template <class T>
static std::vector<typename T::key_type> SortUnorderedSet(T& us)
{
    std::vector<typename T::key_type> ret;
    ret.reserve(us.size());
    for (auto& p : us)
        ret.emplace_back(p);
    std::sort(ret.begin(), ret.end());
    return ret;
}

template <class T>
static std::vector<typename T::key_type> SortUnorderedSet(const T& us)
{
    std::vector<typename T::key_type> ret;
    ret.reserve(us.size());
    for (const auto& p : us)
        ret.emplace_back(p);
    std::sort(ret.begin(), ret.end());
    return ret;
}
}

namespace std
{
#define DECL_ID_HASH(type) \
template<> \
struct hash<amuse::type> \
{ \
    size_t operator()(const amuse::type& val) const noexcept { return val.id; } \
};
DECL_ID_HASH(ObjectId)
DECL_ID_HASH(SoundMacroId)
DECL_ID_HASH(SampleId)
DECL_ID_HASH(TableId)
DECL_ID_HASH(KeymapId)
DECL_ID_HASH(LayersId)
DECL_ID_HASH(SongId)
DECL_ID_HASH(SFXId)
DECL_ID_HASH(GroupId)

template<class T>
struct hash<amuse::ObjToken<T>>
{
    size_t operator()(const amuse::ObjToken<T>& val) const noexcept { return hash<T*>()(val.get()); }
};
}

namespace amuse
{
struct NameDB
{
    enum class Type
    {
        SoundMacro,
        Table,
        Keymap,
        Layer,
        Song,
        SFX,
        Group,
        Sample
    };

    std::unordered_map<std::string, ObjectId> m_stringToId;
    std::unordered_map<ObjectId, std::string> m_idToString;

    ObjectId generateId(Type tp) const;
    static std::string generateName(ObjectId id, Type tp);
    std::string generateDefaultName(Type tp) const;
    std::string_view registerPair(std::string_view str, ObjectId id);
    std::string_view resolveNameFromId(ObjectId id) const;
    ObjectId resolveIdFromName(std::string_view str) const;
    void remove(ObjectId id);
    void rename(ObjectId id, std::string_view str);
};
}

#endif // __AMUSE_COMMON_HPP__

#pragma once

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdio>
#include <limits>
#include <string>
#include <string_view>
#include <unordered_map>

#include <athena/DNA.hpp>
#include <logvisor/logvisor.hpp>

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

namespace amuse {
struct NameDB;

using BigDNA = athena::io::DNA<athena::Big>;
using LittleDNA = athena::io::DNA<athena::Little>;
using BigDNAV = athena::io::DNAVYaml<athena::Big>;
using LittleDNAV = athena::io::DNAVYaml<athena::Little>;

/** Common ID structure statically tagging
 *  SoundMacros, Tables, Keymaps, Layers, Samples, SFX, Songs */
struct ObjectId {
  uint16_t id = 0xffff;
  constexpr ObjectId() noexcept = default;
  constexpr ObjectId(uint16_t idIn) noexcept : id(idIn) {}
  constexpr ObjectId& operator=(uint16_t idIn) noexcept {
    id = idIn;
    return *this;
  }
  constexpr bool operator==(const ObjectId& other) const noexcept { return id == other.id; }
  constexpr bool operator!=(const ObjectId& other) const noexcept { return !operator==(other); }
  constexpr bool operator<(const ObjectId& other) const noexcept { return id < other.id; }
  constexpr bool operator>(const ObjectId& other) const noexcept { return id > other.id; }
  static thread_local NameDB* CurNameDB;
};
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little) ObjectIdDNA : BigDNA {
  AT_DECL_EXPLICIT_DNA_YAML
  void _read(athena::io::YAMLDocReader& r);
  void _write(athena::io::YAMLDocWriter& w);
  ObjectId id;
  constexpr ObjectIdDNA() noexcept = default;
  constexpr ObjectIdDNA(ObjectId idIn) noexcept : id(idIn) {}
  constexpr operator ObjectId() const noexcept { return id; }
};

#define DECL_ID_TYPE(type)                                                                                             \
  struct type : ObjectId {                                                                                             \
    using ObjectId::ObjectId;                                                                                          \
    constexpr type() noexcept = default;                                                                               \
    constexpr type(const ObjectId& idIn) noexcept : ObjectId(idIn) {}                                                  \
    static thread_local NameDB* CurNameDB;                                                                             \
  };                                                                                                                   \
  template <athena::Endian DNAEn>                                                                                      \
  struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little) type##DNA : BigDNA {                         \
    AT_DECL_EXPLICIT_DNA_YAML                                                                                          \
    void _read(athena::io::YAMLDocReader& r);                                                                          \
    void _write(athena::io::YAMLDocWriter& w);                                                                         \
    type id;                                                                                                           \
    constexpr type##DNA() noexcept = default;                                                                          \
    constexpr type##DNA(type idIn) noexcept : id(idIn) {}                                                              \
    constexpr operator type() const noexcept { return id; }                                                            \
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
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little) PageObjectIdDNA : BigDNA {
  AT_DECL_EXPLICIT_DNA_YAML
  void _read(athena::io::YAMLDocReader& r);
  void _write(athena::io::YAMLDocWriter& w);
  ObjectId id;
  constexpr PageObjectIdDNA() noexcept = default;
  constexpr PageObjectIdDNA(ObjectId idIn) noexcept : id(idIn) {}
  constexpr operator ObjectId() const noexcept { return id; }
};

struct SoundMacroStep {
  uint16_t step = 0;
  constexpr operator uint16_t() const noexcept { return step; }
  constexpr SoundMacroStep() noexcept = default;
  constexpr SoundMacroStep(uint16_t idIn) noexcept : step(idIn) {}
  constexpr SoundMacroStep& operator=(uint16_t idIn) noexcept {
    step = idIn;
    return *this;
  }
};

template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little) SoundMacroStepDNA : BigDNA {
  AT_DECL_EXPLICIT_DNA_YAML
  SoundMacroStep step;
  constexpr SoundMacroStepDNA() noexcept = default;
  constexpr SoundMacroStepDNA(SoundMacroStep idIn) noexcept : step(idIn) {}
  constexpr operator SoundMacroStep() const noexcept { return step; }
};

struct LittleUInt24 : LittleDNA {
  AT_DECL_EXPLICIT_DNA_YAML
  atUint32 val{};
  constexpr operator uint32_t() const noexcept { return val; }
  constexpr LittleUInt24() noexcept = default;
  constexpr LittleUInt24(uint32_t valIn) noexcept : val(valIn) {}
  constexpr LittleUInt24& operator=(uint32_t valIn) noexcept {
    val = valIn;
    return *this;
  }
};

class IObj {
  std::atomic_int m_refCount = {0};

protected:
  virtual ~IObj() = default;

public:
  void increment() noexcept { m_refCount.fetch_add(std::memory_order_relaxed); }
  void decrement() noexcept {
    if (m_refCount.fetch_sub(1, std::memory_order_release) == 1) {
      std::atomic_thread_fence(std::memory_order_acquire);
      delete this;
    }
  }
};

template <class SubCls>
class ObjWrapper : public IObj {
  SubCls m_obj;

public:
  template <class... _Args>
  ObjWrapper(_Args&&... args) noexcept : m_obj(std::forward<_Args>(args)...) {}
  SubCls* get() noexcept { return &m_obj; }
  const SubCls* get() const noexcept { return &m_obj; }
};

template <class SubCls>
class ObjTokenBase {
protected:
  IObj* m_obj = nullptr;
  ObjTokenBase(IObj* obj) noexcept : m_obj(obj) {
    if (m_obj) {
      m_obj->increment();
    }
  }

public:
  ObjTokenBase() noexcept = default;
  ObjTokenBase(const ObjTokenBase& other) noexcept : m_obj(other.m_obj) {
    if (m_obj) {
      m_obj->increment();
    }
  }
  ObjTokenBase(ObjTokenBase&& other) noexcept : m_obj(other.m_obj) { other.m_obj = nullptr; }
  ObjTokenBase& operator=(const ObjTokenBase& other) noexcept {
    if (m_obj) {
      m_obj->decrement();
    }
    m_obj = other.m_obj;
    if (m_obj) {
      m_obj->increment();
    }
    return *this;
  }
  ObjTokenBase& operator=(ObjTokenBase&& other) noexcept {
    if (m_obj) {
      m_obj->decrement();
    }
    m_obj = other.m_obj;
    other.m_obj = nullptr;
    return *this;
  }
  ~ObjTokenBase() noexcept {
    if (!m_obj) {
      return;
    }

    m_obj->decrement();
  }
  bool operator==(const ObjTokenBase& other) const noexcept { return m_obj == other.m_obj; }
  bool operator!=(const ObjTokenBase& other) const noexcept { return !operator==(other); }
  bool operator<(const ObjTokenBase& other) const noexcept { return m_obj < other.m_obj; }
  bool operator>(const ObjTokenBase& other) const noexcept { return m_obj > other.m_obj; }
  explicit operator bool() const noexcept { return m_obj != nullptr; }
  void reset() noexcept {
    if (m_obj) {
      m_obj->decrement();
    }
    m_obj = nullptr;
  }
};

template <class SubCls, class Enable = void>
class ObjToken : public ObjTokenBase<SubCls> {
  IObj*& _obj() noexcept { return ObjTokenBase<SubCls>::m_obj; }
  IObj* const& _obj() const noexcept { return ObjTokenBase<SubCls>::m_obj; }

public:
  using ObjTokenBase<SubCls>::ObjTokenBase;
  ObjToken() noexcept = default;
  ObjToken(ObjWrapper<SubCls>* obj) noexcept : ObjTokenBase<SubCls>(obj) {}
  ObjToken& operator=(ObjWrapper<SubCls>* obj) noexcept {
    if (_obj()) {
      _obj()->decrement();
    }
    _obj() = obj;
    if (_obj()) {
      _obj()->increment();
    }
    return *this;
  }
  SubCls* get() const noexcept { return static_cast<ObjWrapper<SubCls>*>(_obj())->get(); }
  SubCls* operator->() const noexcept { return get(); }
  SubCls& operator*() const noexcept { return *get(); }
};

template <class SubCls>
class ObjToken<SubCls, typename std::enable_if_t<std::is_base_of_v<IObj, SubCls>>> : public ObjTokenBase<SubCls> {
  IObj*& _obj() noexcept { return ObjTokenBase<SubCls>::m_obj; }
  IObj* const& _obj() const noexcept { return ObjTokenBase<SubCls>::m_obj; }

public:
  using ObjTokenBase<SubCls>::ObjTokenBase;
  ObjToken() noexcept = default;
  ObjToken(IObj* obj) noexcept : ObjTokenBase<SubCls>(obj) {}
  ObjToken& operator=(IObj* obj) noexcept {
    if (_obj()) {
      _obj()->decrement();
    }
    _obj() = obj;
    if (_obj()) {
      _obj()->increment();
    }
    return *this;
  }
  SubCls* get() const noexcept { return static_cast<SubCls*>(_obj()); }
  SubCls* operator->() const noexcept { return get(); }
  SubCls& operator*() const noexcept { return *get(); }
  template <class T>
  T* cast() const noexcept {
    return static_cast<T*>(_obj());
  }
};

/* ONLY USE WITH CLASSES DERIVED FROM IOBJ!
 * Bypasses type_traits tests for incomplete type definitions. */
template <class SubCls>
class IObjToken : public ObjTokenBase<SubCls> {
  IObj*& _obj() noexcept { return ObjTokenBase<SubCls>::m_obj; }
  IObj* const& _obj() const noexcept { return ObjTokenBase<SubCls>::m_obj; }

public:
  using ObjTokenBase<SubCls>::ObjTokenBase;
  IObjToken() noexcept = default;
  IObjToken(IObj* obj) noexcept : ObjTokenBase<SubCls>(obj) {}
  IObjToken& operator=(IObj* obj) noexcept {
    if (_obj()) {
      _obj()->decrement();
    }
    _obj() = obj;
    if (_obj()) {
      _obj()->increment();
    }
    return *this;
  }
  SubCls* get() const noexcept { return static_cast<SubCls*>(_obj()); }
  SubCls* operator->() const noexcept { return get(); }
  SubCls& operator*() const noexcept { return *get(); }
  template <class T>
  T* cast() const noexcept {
    return static_cast<T*>(_obj());
  }
};

template <class Tp, class... _Args>
inline typename std::enable_if_t<std::is_base_of_v<IObj, Tp>, ObjToken<Tp>> MakeObj(_Args&&... args) {
  return new Tp(std::forward<_Args>(args)...);
}

template <class Tp, class... _Args>
inline typename std::enable_if_t<!std::is_base_of_v<IObj, Tp>, ObjToken<Tp>> MakeObj(_Args&&... args) {
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
#ifndef _SYS_STR
#define _SYS_STR(val) L##val
#endif
typedef struct _stat Sstat;
inline int Mkdir(const wchar_t* path, int) { return _wmkdir(path); }
inline int Stat(const wchar_t* path, Sstat* statout) { return _wstat(path, statout); }
#else
using SystemString = std::string;
using SystemStringView = std::string_view;
using SystemChar = char;
#ifndef _SYS_STR
#define _SYS_STR(val) val
#endif
typedef struct stat Sstat;
inline int Mkdir(const char* path, mode_t mode) { return mkdir(path, mode); }
inline int Stat(const char* path, Sstat* statout) { return stat(path, statout); }
#endif

inline int Rename(const SystemChar* oldpath, const SystemChar* newpath) {
#if _WIN32
  // return _wrename(oldpath, newpath);
  return MoveFileExW(oldpath, newpath, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) == 0;
#else
  return rename(oldpath, newpath);
#endif
}

#if _WIN32
inline int CompareCaseInsensitive(const char* a, const char* b) { return _stricmp(a, b); }
#endif

inline int CompareCaseInsensitive(const SystemChar* a, const SystemChar* b) {
#if _WIN32
  return _wcsicmp(a, b);
#else
  return strcasecmp(a, b);
#endif
}

template <typename T>
constexpr T ClampFull(float in) noexcept {
  if (std::is_floating_point<T>()) {
    return in;
  } else {
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

inline const SystemChar* StrRChr(const SystemChar* str, SystemChar ch) {
#if _WIN32
  return wcsrchr(str, ch);
#else
  return strrchr(str, ch);
#endif
}

inline SystemChar* StrRChr(SystemChar* str, SystemChar ch) {
#if _WIN32
  return wcsrchr(str, ch);
#else
  return strrchr(str, ch);
#endif
}

inline int FSeek(FILE* fp, int64_t offset, int whence) {
#if _WIN32
  return _fseeki64(fp, offset, whence);
#elif __APPLE__ || __FreeBSD__
  return fseeko(fp, offset, whence);
#else
  return fseeko64(fp, offset, whence);
#endif
}

inline int64_t FTell(FILE* fp) {
#if _WIN32
  return _ftelli64(fp);
#elif __APPLE__ || __FreeBSD__
  return ftello(fp);
#else
  return ftello64(fp);
#endif
}

inline FILE* FOpen(const SystemChar* path, const SystemChar* mode) {
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

inline void Unlink(const SystemChar* file) {
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
constexpr T bswap16(T val) noexcept {
#if __GNUC__
  return __builtin_bswap16(val);
#elif _WIN32
  return _byteswap_ushort(val);
#else
  return (val = (val << 8) | ((val >> 8) & 0xFF));
#endif
}

template <typename T>
constexpr T bswap32(T val) noexcept {
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
constexpr T bswap64(T val) noexcept {
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
constexpr int16_t SBig(int16_t val) noexcept { return bswap16(val); }
constexpr uint16_t SBig(uint16_t val) noexcept { return bswap16(val); }
constexpr int32_t SBig(int32_t val) noexcept { return bswap32(val); }
constexpr uint32_t SBig(uint32_t val) noexcept { return bswap32(val); }
constexpr int64_t SBig(int64_t val) noexcept { return bswap64(val); }
constexpr uint64_t SBig(uint64_t val) noexcept { return bswap64(val); }
constexpr float SBig(float val) noexcept {
  union { float f; atInt32 i; } uval1 = {val};
  union { atInt32 i; float f; } uval2 = {bswap32(uval1.i)};
  return uval2.f;
}
constexpr double SBig(double val) noexcept {
  union { double f; atInt64 i; } uval1 = {val};
  union { atInt64 i; double f; } uval2 = {bswap64(uval1.i)};
  return uval2.f;
}
#ifndef SBIG
#define SBIG(q) (((q)&0x000000FF) << 24 | ((q)&0x0000FF00) << 8 | ((q)&0x00FF0000) >> 8 | ((q)&0xFF000000) >> 24)
#endif

constexpr int16_t SLittle(int16_t val) noexcept { return val; }
constexpr uint16_t SLittle(uint16_t val) noexcept { return val; }
constexpr int32_t SLittle(int32_t val) noexcept { return val; }
constexpr uint32_t SLittle(uint32_t val) noexcept { return val; }
constexpr int64_t SLittle(int64_t val) noexcept { return val; }
constexpr uint64_t SLittle(uint64_t val) noexcept { return val; }
constexpr float SLittle(float val) noexcept { return val; }
constexpr double SLittle(double val) noexcept { return val; }
#ifndef SLITTLE
#define SLITTLE(q) (q)
#endif
#else
constexpr int16_t SLittle(int16_t val) noexcept { return bswap16(val); }
constexpr uint16_t SLittle(uint16_t val) noexcept { return bswap16(val); }
constexpr int32_t SLittle(int32_t val) noexcept { return bswap32(val); }
constexpr uint32_t SLittle(uint32_t val) noexcept { return bswap32(val); }
constexpr int64_t SLittle(int64_t val) noexcept { return bswap64(val); }
constexpr uint64_t SLittle(uint64_t val) noexcept { return bswap64(val); }
constexpr float SLittle(float val) noexcept {
  int32_t ival = bswap32(*((int32_t*)(&val)));
  return *((float*)(&ival));
}
constexpr double SLittle(double val) noexcept {
  int64_t ival = bswap64(*((int64_t*)(&val)));
  return *((double*)(&ival));
}
#ifndef SLITTLE
#define SLITTLE(q) (((q)&0x000000FF) << 24 | ((q)&0x0000FF00) << 8 | ((q)&0x00FF0000) >> 8 | ((q)&0xFF000000) >> 24)
#endif

constexpr int16_t SBig(int16_t val) noexcept { return val; }
constexpr uint16_t SBig(uint16_t val) noexcept { return val; }
constexpr int32_t SBig(int32_t val) noexcept { return val; }
constexpr uint32_t SBig(uint32_t val) noexcept { return val; }
constexpr int64_t SBig(int64_t val) noexcept { return val; }
constexpr uint64_t SBig(uint64_t val) noexcept { return val; }
constexpr float SBig(float val) noexcept { return val; }
constexpr double SBig(double val) noexcept { return val; }
#ifndef SBIG
#define SBIG(q) (q)
#endif
#endif

/** Versioned data format to interpret */
enum class DataFormat { GCN, N64, PC };

/** Meta-type for selecting GameCube (MusyX 2.0) data formats */
struct GCNDataTag {};

/** Meta-type for selecting N64 (MusyX 1.0) data formats */
struct N64DataTag {};

/** Meta-type for selecting PC (MusyX 1.0) data formats */
struct PCDataTag {};

template <class T>
inline std::vector<std::pair<typename T::key_type, std::reference_wrapper<typename T::mapped_type>>>
SortUnorderedMap(T& um) {
  std::vector<std::pair<typename T::key_type, std::reference_wrapper<typename T::mapped_type>>> ret;
  ret.reserve(um.size());
  for (auto& p : um)
    ret.emplace_back(p.first, p.second);
  std::sort(ret.begin(), ret.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  return ret;
}

template <class T>
inline std::vector<std::pair<typename T::key_type, std::reference_wrapper<const typename T::mapped_type>>>
SortUnorderedMap(const T& um) {
  std::vector<std::pair<typename T::key_type, std::reference_wrapper<const typename T::mapped_type>>> ret;
  ret.reserve(um.size());
  for (const auto& p : um)
    ret.emplace_back(p.first, p.second);
  std::sort(ret.begin(), ret.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
  return ret;
}

template <class T>
inline std::vector<typename T::key_type> SortUnorderedSet(T& us) {
  std::vector<typename T::key_type> ret;
  ret.reserve(us.size());
  for (auto& p : us)
    ret.emplace_back(p);
  std::sort(ret.begin(), ret.end());
  return ret;
}

template <class T>
inline std::vector<typename T::key_type> SortUnorderedSet(const T& us) {
  std::vector<typename T::key_type> ret;
  ret.reserve(us.size());
  for (const auto& p : us)
    ret.emplace_back(p);
  std::sort(ret.begin(), ret.end());
  return ret;
}
} // namespace amuse

namespace std {
#define DECL_ID_HASH(type)                                                                                             \
  template <>                                                                                                          \
  struct hash<amuse::type> {                                                                                           \
    size_t operator()(const amuse::type& val) const noexcept { return val.id; }                                        \
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

template <class T>
struct hash<amuse::ObjToken<T>> {
  size_t operator()(const amuse::ObjToken<T>& val) const noexcept { return hash<T*>()(val.get()); }
};
} // namespace std

namespace amuse {
struct NameDB {
  enum class Type { SoundMacro, Table, Keymap, Layer, Song, SFX, Group, Sample };

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
} // namespace amuse

FMT_CUSTOM_FORMATTER(amuse::ObjectId, "{:04X}", obj.id)
FMT_CUSTOM_FORMATTER(amuse::SoundMacroId, "{:04X}", obj.id)
FMT_CUSTOM_FORMATTER(amuse::SampleId, "{:04X}", obj.id)
FMT_CUSTOM_FORMATTER(amuse::TableId, "{:04X}", obj.id)
FMT_CUSTOM_FORMATTER(amuse::KeymapId, "{:04X}", obj.id)
FMT_CUSTOM_FORMATTER(amuse::LayersId, "{:04X}", obj.id)
FMT_CUSTOM_FORMATTER(amuse::SongId, "{:04X}", obj.id)
FMT_CUSTOM_FORMATTER(amuse::SFXId, "{:04X}", obj.id)
FMT_CUSTOM_FORMATTER(amuse::GroupId, "{:04X}", obj.id)

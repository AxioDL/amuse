#ifndef __AMUSE_COMMON_HPP__
#define __AMUSE_COMMON_HPP__

#include <algorithm>
#include <limits>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string>
#include <string_view>
#include <cstring>

#ifndef _WIN32
#include <strings.h>
#include <sys/stat.h>
#include <unistd.h>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#include <Windows.h>
#endif

#undef min
#undef max

constexpr float NativeSampleRate = 32000.0f;

namespace amuse
{

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
    if(std::is_floating_point<T>())
    {
        return std::min<T>(std::max<T>(in, -1.0), 1.0);
    }
    else
    {
        constexpr T MAX = std::numeric_limits<T>::max();
        constexpr T MIN = std::numeric_limits<T>::min();
        
        if(in < MIN)
            return MIN;
        else if(in > MAX)
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
}

#endif // __AMUSE_COMMON_HPP__

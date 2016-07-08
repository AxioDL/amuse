#ifndef __AMUSE_AUDIOGROUPDATA_HPP__
#define __AMUSE_AUDIOGROUPDATA_HPP__

#include "Common.hpp"

namespace amuse
{

/** Simple pointer-container of the four Audio Group chunks */
class AudioGroupData
{
    friend class Engine;

protected:
    unsigned char* m_proj;
    size_t m_projSz;
    unsigned char* m_pool;
    size_t m_poolSz;
    unsigned char* m_sdir;
    size_t m_sdirSz;
    unsigned char* m_samp;
    size_t m_sampSz;

    DataFormat m_fmt;
    bool m_absOffs;

    AudioGroupData(unsigned char* proj, size_t projSz, unsigned char* pool, size_t poolSz, unsigned char* sdir, size_t sdirSz,
                   unsigned char* samp, size_t sampSz, DataFormat fmt, bool absOffs)
    : m_proj(proj)
    , m_projSz(projSz)
    , m_pool(pool)
    , m_poolSz(poolSz)
    , m_sdir(sdir)
    , m_sdirSz(sdirSz)
    , m_samp(samp)
    , m_sampSz(sampSz)
    , m_fmt(fmt)
    , m_absOffs(absOffs)
    {
    }

public:
    AudioGroupData(unsigned char* proj, size_t projSz, unsigned char* pool, size_t poolSz, unsigned char* sdir, size_t sdirSz,
                   unsigned char* samp, size_t sampSz, GCNDataTag)
    : m_proj(proj)
    , m_projSz(projSz)
    , m_pool(pool)
    , m_poolSz(poolSz)
    , m_sdir(sdir)
    , m_sdirSz(sdirSz)
    , m_samp(samp)
    , m_sampSz(sampSz)
    , m_fmt(DataFormat::GCN)
    , m_absOffs(true)
    {
    }
    AudioGroupData(unsigned char* proj, size_t projSz, unsigned char* pool, size_t poolSz, unsigned char* sdir, size_t sdirSz,
                   unsigned char* samp, size_t sampSz, bool absOffs, N64DataTag)
    : m_proj(proj)
    , m_projSz(projSz)
    , m_pool(pool)
    , m_poolSz(poolSz)
    , m_sdir(sdir)
    , m_sdirSz(sdirSz)
    , m_samp(samp)
    , m_sampSz(sampSz)
    , m_fmt(DataFormat::N64)
    , m_absOffs(absOffs)
    {
    }
    AudioGroupData(unsigned char* proj, size_t projSz, unsigned char* pool, size_t poolSz, unsigned char* sdir, size_t sdirSz,
                   unsigned char* samp, size_t sampSz, bool absOffs, PCDataTag)
    : m_proj(proj)
    , m_projSz(projSz)
    , m_pool(pool)
    , m_poolSz(poolSz)
    , m_sdir(sdir)
    , m_sdirSz(sdirSz)
    , m_samp(samp)
    , m_sampSz(sampSz)
    , m_fmt(DataFormat::PC)
    , m_absOffs(absOffs)
    {
    }

    const unsigned char* getProj() const { return m_proj; }
    const unsigned char* getPool() const { return m_pool; }
    const unsigned char* getSdir() const { return m_sdir; }
    const unsigned char* getSamp() const { return m_samp; }

    unsigned char* getProj() { return m_proj; }
    unsigned char* getPool() { return m_pool; }
    unsigned char* getSdir() { return m_sdir; }
    unsigned char* getSamp() { return m_samp; }

    size_t getProjSize() const { return m_projSz; }
    size_t getPoolSize() const { return m_poolSz; }
    size_t getSdirSize() const { return m_sdirSz; }
    size_t getSampSize() const { return m_sampSz; }

    operator bool() const { return m_proj != nullptr && m_pool != nullptr && m_sdir != nullptr && m_samp != nullptr; }

    DataFormat getDataFormat() const { return m_fmt; }
    bool getAbsoluteProjOffsets() const { return m_absOffs; }
};

/** A buffer-owning version of AudioGroupData */
class IntrusiveAudioGroupData : public AudioGroupData
{
    bool m_owns = true;

public:
    using AudioGroupData::AudioGroupData;
    ~IntrusiveAudioGroupData();

    IntrusiveAudioGroupData(const IntrusiveAudioGroupData&) = delete;
    IntrusiveAudioGroupData& operator=(const IntrusiveAudioGroupData&) = delete;

    IntrusiveAudioGroupData(IntrusiveAudioGroupData&& other);
    IntrusiveAudioGroupData& operator=(IntrusiveAudioGroupData&& other);

    void dangleOwnership() { m_owns = false; }
};
}

#endif // __AMUSE_AUDIOGROUPDATA_HPP__

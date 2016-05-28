#ifndef __AMUSE_AUDIOGROUPDATA_HPP__
#define __AMUSE_AUDIOGROUPDATA_HPP__

#include "Common.hpp"

namespace amuse
{

/** Simple pointer-container of the four Audio Group chunks */
class AudioGroupData
{
    friend class Engine;
    friend class AudioGroupProject;
protected:
    unsigned char* m_proj;
    unsigned char* m_pool;
    unsigned char* m_sdir;
    unsigned char* m_samp;
    DataFormat m_fmt;

    AudioGroupData(unsigned char* proj, unsigned char* pool,
                   unsigned char* sdir, unsigned char* samp, DataFormat fmt)
    : m_proj(proj), m_pool(pool), m_sdir(sdir), m_samp(samp), m_fmt(fmt) {}
public:
    AudioGroupData(unsigned char* proj, unsigned char* pool,
                   unsigned char* sdir, unsigned char* samp, GCNDataTag)
    : m_proj(proj), m_pool(pool), m_sdir(sdir), m_samp(samp), m_fmt(DataFormat::GCN) {}
    AudioGroupData(unsigned char* proj, unsigned char* pool,
                   unsigned char* sdir, unsigned char* samp, N64DataTag)
    : m_proj(proj), m_pool(pool), m_sdir(sdir), m_samp(samp), m_fmt(DataFormat::N64) {}
    AudioGroupData(unsigned char* proj, unsigned char* pool,
                   unsigned char* sdir, unsigned char* samp, PCDataTag)
    : m_proj(proj), m_pool(pool), m_sdir(sdir), m_samp(samp), m_fmt(DataFormat::PC) {}

    const unsigned char* getProj() const {return m_proj;}
    const unsigned char* getPool() const {return m_pool;}
    const unsigned char* getSdir() const {return m_sdir;}
    const unsigned char* getSamp() const {return m_samp;}

    operator bool() const
    {
        return m_proj != nullptr && m_pool != nullptr && m_sdir != nullptr && m_samp != nullptr;
    }
};

/** A buffer-owning version of AudioGroupData */
class IntrusiveAudioGroupData : public AudioGroupData
{
    bool m_owns = true;
public:
    using AudioGroupData::AudioGroupData;
    ~IntrusiveAudioGroupData();

    IntrusiveAudioGroupData(const IntrusiveAudioGroupData&)=delete;
    IntrusiveAudioGroupData& operator=(const IntrusiveAudioGroupData&)=delete;

    IntrusiveAudioGroupData(IntrusiveAudioGroupData&& other);
    IntrusiveAudioGroupData& operator=(IntrusiveAudioGroupData&& other);
};

}

#endif // __AMUSE_AUDIOGROUPDATA_HPP__

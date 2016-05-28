#ifndef __AMUSE_AUDIOGROUP_HPP__
#define __AMUSE_AUDIOGROUP_HPP__

#include "AudioGroupPool.hpp"
#include "AudioGroupProject.hpp"
#include "AudioGroupSampleDirectory.hpp"

namespace amuse
{
class AudioGroupData;

using Sample = std::pair<AudioGroupSampleDirectory::Entry,
                         AudioGroupSampleDirectory::ADPCMParms>;

/** Runtime audio group index container */
class AudioGroup
{
    AudioGroupProject m_proj;
    AudioGroupPool m_pool;
    AudioGroupSampleDirectory m_sdir;
    const unsigned char* m_samp;
    DataFormat m_fmt;
    bool m_valid;
public:
    operator bool() const {return m_valid;}
    AudioGroup(const AudioGroupData& data, GCNDataTag);
    AudioGroup(const AudioGroupData& data, N64DataTag);
    AudioGroup(const AudioGroupData& data, PCDataTag);

    const Sample* getSample(int sfxId) const;
    const unsigned char* getSampleData(uint32_t offset) const;
    const AudioGroupProject& getProj() const {return m_proj;}
    const AudioGroupPool& getPool() const {return m_pool;}
    DataFormat getDataFormat() const {return m_fmt;}
};

}

#endif // __AMUSE_AUDIOGROUP_HPP__

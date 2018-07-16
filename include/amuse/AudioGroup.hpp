#ifndef __AMUSE_AUDIOGROUP_HPP__
#define __AMUSE_AUDIOGROUP_HPP__

#include "AudioGroupPool.hpp"
#include "AudioGroupProject.hpp"
#include "AudioGroupSampleDirectory.hpp"

namespace amuse
{
class AudioGroupData;

/** Runtime audio group index container */
class AudioGroup
{
    AudioGroupProject m_proj;
    AudioGroupPool m_pool;
    AudioGroupSampleDirectory m_sdir;
    const unsigned char* m_samp = nullptr;
    SystemString m_groupPath;
    bool m_valid;

public:
    operator bool() const { return m_valid; }
    explicit AudioGroup(const AudioGroupData& data);
    explicit AudioGroup(SystemStringView groupPath);

    const AudioGroupSampleDirectory::Entry* getSample(SampleId sfxId) const;
    const unsigned char* getSampleData(SampleId sfxId, const AudioGroupSampleDirectory::Entry* sample) const;
    const AudioGroupProject& getProj() const { return m_proj; }
    const AudioGroupPool& getPool() const { return m_pool; }
    const AudioGroupSampleDirectory& getSdir() const { return m_sdir; }
};
}

#endif // __AMUSE_AUDIOGROUP_HPP__

#ifndef __AMUSE_AUDIOGROUP_HPP__
#define __AMUSE_AUDIOGROUP_HPP__

#include "AudioGroupPool.hpp"
#include "AudioGroupProject.hpp"
#include "AudioGroupSampleDirectory.hpp"

namespace amuse
{
class AudioGroupData;

class AudioGroup
{
    int m_groupId;
    AudioGroupPool m_pool;
    AudioGroupProject m_proj;
    AudioGroupSampleDirectory m_sdir;
    const unsigned char* m_samp;
    bool m_valid;
public:
    operator bool() const {return m_valid;}
    AudioGroup(int groupId, const AudioGroupData& data);

    int groupId() const {return m_groupId;}
    bool sfxInGroup(int sfxId) const;
    bool songInGroup(int songId) const;

    const AudioGroupPool& getPool() const;
    const AudioGroupSampleDirectory::Entry* getSfxEntry(int sfxId) const;
};

}

#endif // __AMUSE_AUDIOGROUP_HPP__

#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"

namespace amuse
{

void AudioGroup::assign(const AudioGroupData& data)
{
    m_proj = AudioGroupProject::CreateAudioGroupProject(data);
    m_pool = AudioGroupPool::CreateAudioGroupPool(data);
    m_sdir = AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(data);
    m_samp = data.getSamp();
}
void AudioGroup::assign(SystemStringView groupPath)
{
    /* Reverse order when loading intermediates */
    m_groupPath = groupPath;
    m_sdir = AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(groupPath);
    m_pool = AudioGroupPool::CreateAudioGroupPool(groupPath);
    m_proj = AudioGroupProject::CreateAudioGroupProject(groupPath);
    m_samp = nullptr;
}

const AudioGroupSampleDirectory::Entry* AudioGroup::getSample(SampleId sfxId) const
{
    auto search = m_sdir.m_entries.find(sfxId);
    if (search == m_sdir.m_entries.cend())
        return nullptr;
    return &search->second;
}

const unsigned char* AudioGroup::getSampleData(SampleId sfxId, const AudioGroupSampleDirectory::Entry* sample) const
{
    if (sample->m_looseData)
    {
        setIdDatabases();
#if _WIN32
        SystemString basePath = m_groupPath + _S('/') +
            athena::utility::utf8ToWide(SampleId::CurNameDB->resolveNameFromId(sfxId));
#else
        SystemString basePath = m_groupPath + _S('/') +
            SampleId::CurNameDB->resolveNameFromId(sfxId).data();
#endif
        const_cast<AudioGroupSampleDirectory::Entry*>(sample)->loadLooseData(basePath);
        return sample->m_looseData.get();
    }
    return m_samp + sample->m_sampleOff;
}
}

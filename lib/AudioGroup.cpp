#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"

namespace amuse
{

AudioGroup::AudioGroup(const AudioGroupData& data, GCNDataTag)
: m_proj(AudioGroupProject::CreateAudioGroupProject(data))
, m_pool(AudioGroupPool::CreateAudioGroupPool(data))
, m_sdir(data.getSdir(), GCNDataTag{})
, m_samp(data.getSamp())
, m_fmt(DataFormat::GCN)
{
}

AudioGroup::AudioGroup(const AudioGroupData& data, bool absOffs, N64DataTag)
: m_proj(AudioGroupProject::CreateAudioGroupProject(data))
, m_pool(AudioGroupPool::CreateAudioGroupPool(data))
, m_sdir(data.getSdir(), data.getSamp(), absOffs, N64DataTag{})
, m_samp(data.getSamp())
, m_fmt(DataFormat::N64)
{
}

AudioGroup::AudioGroup(const AudioGroupData& data, bool absOffs, PCDataTag)
: m_proj(AudioGroupProject::CreateAudioGroupProject(data))
, m_pool(AudioGroupPool::CreateAudioGroupPool(data))
, m_sdir(data.getSdir(), absOffs, PCDataTag{})
, m_samp(data.getSamp())
, m_fmt(DataFormat::PC)
{
}

const Sample* AudioGroup::getSample(int sfxId) const
{
    auto search = m_sdir.m_entries.find(sfxId);
    if (search == m_sdir.m_entries.cend())
        return nullptr;
    return &search->second;
}

const unsigned char* AudioGroup::getSampleData(uint32_t offset) const { return m_samp + offset; }
}

#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"

namespace amuse
{

AudioGroup::AudioGroup(const AudioGroupData& data)
: m_proj(data.getProj()),
  m_pool(data.getPool()),
  m_sdir(data.getSdir()),
  m_samp(data.getSamp())
{}

const Sample* AudioGroup::getSample(int sfxId) const
{
    auto search = m_sdir.m_entries.find(sfxId);
    if (search == m_sdir.m_entries.cend())
        return nullptr;
    return &search->second;
}

const unsigned char* AudioGroup::getSampleData(uint32_t offset) const
{
    return m_samp + offset;
}

}

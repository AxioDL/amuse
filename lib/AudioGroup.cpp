#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"

namespace amuse
{

AudioGroup::AudioGroup(const AudioGroupData& data, GCNDataTag)
: m_proj(data.getProj(), GCNDataTag{}),
  m_pool(data.getPool()),
  m_sdir(data.getSdir(), GCNDataTag{}),
  m_samp(data.getSamp()),
  m_fmt(DataFormat::GCN)
{}

AudioGroup::AudioGroup(const AudioGroupData& data, N64DataTag)
: m_proj(data.getProj(), N64DataTag{}),
  m_pool(data.getPool()),
  m_sdir(data.getSdir(), data.getSamp(), N64DataTag{}),
  m_samp(data.getSamp()),
  m_fmt(DataFormat::N64)
{}

AudioGroup::AudioGroup(const AudioGroupData& data, PCDataTag)
: m_proj(data.getProj(), PCDataTag{}),
  m_pool(data.getPool(), PCDataTag{}),
  m_sdir(data.getSdir(), PCDataTag{}),
  m_samp(data.getSamp()),
  m_fmt(DataFormat::PC)
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

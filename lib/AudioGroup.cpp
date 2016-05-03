#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"

namespace amuse
{

AudioGroup::AudioGroup(int groupId, const AudioGroupData& data)
: m_groupId(groupId),
  m_pool(data.getPool()),
  m_proj(data.getProj()),
  m_sdir(data.getSdir()),
  m_samp(data.getSamp())
{}

bool AudioGroup::sfxInGroup(int sfxId) const
{
}

bool AudioGroup::songInGroup(int songId) const
{
}

const AudioGroupSampleDirectory::Entry* AudioGroup::getSfxEntry(int sfxId) const
{
    for (const auto& ent : m_sdir.m_entries)
        if (ent.first.m_sfxId == sfxId)
            return &ent.first;
    return nullptr;
}

}

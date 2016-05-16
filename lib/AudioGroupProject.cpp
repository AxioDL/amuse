#include "amuse/AudioGroupProject.hpp"
#include "amuse/Common.hpp"
#include <stdint.h>

namespace amuse
{

enum class GroupType : uint16_t
{
    Song,
    SFX
};

struct GroupHeader
{
    uint32_t groupEndOff;
    uint16_t groupId;
    GroupType type;
    uint32_t soundMacroIdsOff;
    uint32_t samplIdsOff;
    uint32_t tableIdsOff;
    uint32_t keymapIdsOff;
    uint32_t layerIdsOff;
    uint32_t pageTableOff;
    uint32_t drumTableOff;
    uint32_t midiSetupsOff;

    void swapBig()
    {
        groupEndOff = SBig(groupEndOff);
        groupId = SBig(groupId);
        type = GroupType(SBig(uint16_t(type)));
        soundMacroIdsOff = SBig(soundMacroIdsOff);
        samplIdsOff = SBig(samplIdsOff);
        tableIdsOff = SBig(tableIdsOff);
        keymapIdsOff = SBig(keymapIdsOff);
        layerIdsOff = SBig(layerIdsOff);
        pageTableOff = SBig(pageTableOff);
        drumTableOff = SBig(drumTableOff);
        midiSetupsOff = SBig(midiSetupsOff);
    }
};

AudioGroupProject::AudioGroupProject(const unsigned char* data)
{
    const GroupHeader* group = reinterpret_cast<const GroupHeader*>(data);
    while (group->groupEndOff != 0xffffffff)
    {
        GroupHeader header = *group;
        header.swapBig();

        AudioGroupIndex* bIdx = nullptr;

        if (header.type == GroupType::Song)
        {
            SongGroupIndex& idx = m_songGroups[header.groupId];
            bIdx = &idx;

            /* Normal pages */
            const SongGroupIndex::PageEntry* normEntries =
                reinterpret_cast<const SongGroupIndex::PageEntry*>(data + header.pageTableOff);
            while (normEntries->objId != 0xffff)
            {
                idx.m_normPages[normEntries->programNo] = normEntries;
                ++normEntries;
            }

            /* Drum pages */
            const SongGroupIndex::PageEntry* drumEntries =
                reinterpret_cast<const SongGroupIndex::PageEntry*>(data + header.drumTableOff);
            while (drumEntries->objId != 0xffff)
            {
                idx.m_drumPages[drumEntries->programNo] = drumEntries;
                ++drumEntries;
            }

            /* MIDI setups */
            const uint8_t* setupData = data + header.midiSetupsOff;
            const uint8_t* setupEnd = data + header.groupEndOff;
            while (setupData < setupEnd)
            {
                uint16_t songId = SBig(*reinterpret_cast<const uint16_t*>(setupData));
                idx.m_midiSetups[songId] =
                    reinterpret_cast<const std::array<SongGroupIndex::MIDISetup, 16>*>(setupData + 4);
                setupData += 5 * 16 + 4;
            }
        }
        else if (header.type == GroupType::SFX)
        {
            SFXGroupIndex& idx = m_sfxGroups[header.groupId];
            bIdx = &idx;

            /* SFX entries */
            uint16_t count = SBig(*reinterpret_cast<const uint16_t*>(data + header.pageTableOff));
            idx.m_sfxEntries.reserve(count);
            const SFXGroupIndex::SFXEntry* entries =
                reinterpret_cast<const SFXGroupIndex::SFXEntry*>(data + header.pageTableOff + 4);
            for (int i=0 ; i<count ; ++i)
            {
                idx.m_sfxEntries[SBig(entries->defineId)] = entries;
                ++entries;
            }
        }

        if (bIdx)
        {
            bIdx->m_soundMacroIndex = reinterpret_cast<const uint16_t*>(data + header.soundMacroIdsOff);
            bIdx->m_tablesIndex = reinterpret_cast<const uint16_t*>(data + header.tableIdsOff);
            bIdx->m_keymapsIndex = reinterpret_cast<const uint16_t*>(data + header.keymapIdsOff);
            bIdx->m_layersIndex = reinterpret_cast<const uint16_t*>(data + header.layerIdsOff);
        }

        group = reinterpret_cast<const GroupHeader*>(data + header.groupEndOff);
    }
}

const SongGroupIndex* AudioGroupProject::getSongGroupIndex(int groupId) const
{
    auto search = m_songGroups.find(groupId);
    if (search == m_songGroups.cend())
        return nullptr;
    return &search->second;
}

const SFXGroupIndex* AudioGroupProject::getSFXGroupIndex(int groupId) const
{
    auto search = m_sfxGroups.find(groupId);
    if (search == m_sfxGroups.cend())
        return nullptr;
    return &search->second;
}

}

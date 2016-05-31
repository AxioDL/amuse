#include "amuse/AudioGroupProject.hpp"
#include "amuse/AudioGroupData.hpp"
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

AudioGroupProject::AudioGroupProject(const unsigned char* data, GCNDataTag)
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

struct MusyX1PageEntry
{
    ObjectId objId;
    uint8_t priority;
    uint8_t maxVoices;
    uint8_t unk;
    uint8_t programNo;
    uint8_t pad[2];

    void setIntoMusyX2(SongGroupIndex::PageEntry& ent) const
    {
        ent.objId = objId;
        ent.priority = priority;
        ent.maxVoices = maxVoices;
        ent.programNo = programNo;
    }
};

struct MusyX1MIDISetup
{
    uint8_t programNo;
    uint8_t volume;
    uint8_t panning;
    uint8_t reverb;
    uint8_t chorus;
    uint8_t pad[3];

    void setIntoMusyX2(SongGroupIndex::MIDISetup& ent) const
    {
        ent.programNo = programNo;
        ent.volume = volume;
        ent.panning = panning;
        ent.reverb = reverb;
        ent.chorus = chorus;
    }
};

void AudioGroupProject::_allocateConvBuffers(const unsigned char* data, bool absOffs, N64DataTag)
{
    size_t normPageCount = 0;
    size_t drumPageCount = 0;
    size_t midiSetupCount = 0;

    const GroupHeader* group = reinterpret_cast<const GroupHeader*>(data);
    while (group->groupEndOff != 0xffffffff)
    {
        const unsigned char* subData = absOffs ? data : data + 8;
        GroupHeader header = *group;
        header.swapBig();

        if (header.type == GroupType::Song)
        {
            /* Normal pages */
            const MusyX1PageEntry* normEntries =
                reinterpret_cast<const MusyX1PageEntry*>(subData + header.pageTableOff);
            while (normEntries->objId != 0xffff)
            {
                ++normPageCount;
                ++normEntries;
            }

            /* Drum pages */
            const MusyX1PageEntry* drumEntries =
                reinterpret_cast<const MusyX1PageEntry*>(subData + header.drumTableOff);
            while (drumEntries->objId != 0xffff)
            {
                ++drumPageCount;
                ++drumEntries;
            }

            /* MIDI setups */
            const uint8_t* setupData = subData + header.midiSetupsOff;
            while (*reinterpret_cast<const uint32_t*>(setupData) != 0xffffffff)
            {
                ++midiSetupCount;
                setupData += 8 * 16 + 4;
            }
        }

        if (absOffs)
            group = reinterpret_cast<const GroupHeader*>(data + header.groupEndOff);
        else
        {
            data += header.groupEndOff;
            group = reinterpret_cast<const GroupHeader*>(data);
        }
    }

    if (normPageCount)
        m_convNormalPages.reset(new SongGroupIndex::PageEntry[normPageCount]);
    if (drumPageCount)
        m_convDrumPages.reset(new SongGroupIndex::PageEntry[drumPageCount]);
    if (midiSetupCount)
        m_convMidiSetups.reset(new std::array<SongGroupIndex::MIDISetup, 16>[midiSetupCount]);
}

AudioGroupProject::AudioGroupProject(const unsigned char* data, bool absOffs, N64DataTag)
{
    _allocateConvBuffers(data, absOffs, N64DataTag{});
    SongGroupIndex::PageEntry* normPagesBuf = m_convNormalPages.get();
    SongGroupIndex::PageEntry* drumPagesBuf = m_convDrumPages.get();
    std::array<SongGroupIndex::MIDISetup, 16>* midiSetupsBuf = m_convMidiSetups.get();

    const GroupHeader* group = reinterpret_cast<const GroupHeader*>(data);
    while (group->groupEndOff != 0xffffffff)
    {
        const unsigned char* subData = absOffs ? data : data + 8;
        GroupHeader header = *group;
        header.swapBig();

        AudioGroupIndex* bIdx = nullptr;

        if (header.type == GroupType::Song)
        {
            SongGroupIndex& idx = m_songGroups[header.groupId];
            bIdx = &idx;

            /* Normal pages */
            const MusyX1PageEntry* normEntries =
                reinterpret_cast<const MusyX1PageEntry*>(subData + header.pageTableOff);
            while (normEntries->objId != 0xffff)
            {
                normEntries->setIntoMusyX2(*normPagesBuf);
                idx.m_normPages[normEntries->programNo] = normPagesBuf;
                ++normEntries;
                ++normPagesBuf;
            }

            /* Drum pages */
            const MusyX1PageEntry* drumEntries =
                reinterpret_cast<const MusyX1PageEntry*>(subData + header.drumTableOff);
            while (drumEntries->objId != 0xffff)
            {
                drumEntries->setIntoMusyX2(*drumPagesBuf);
                idx.m_drumPages[drumEntries->programNo] = drumPagesBuf;
                ++drumEntries;
                ++drumPagesBuf;
            }

            /* MIDI setups */
            const uint8_t* setupData = subData + header.midiSetupsOff;
            while (*reinterpret_cast<const uint32_t*>(setupData) != 0xffffffff)
            {
                uint16_t songId = SBig(*reinterpret_cast<const uint16_t*>(setupData));
                const std::array<MusyX1MIDISetup, 16>* midiSetups =
                    reinterpret_cast<const std::array<MusyX1MIDISetup, 16>*>(setupData + 4);

                for (int i=0 ; i<16 ; ++i)
                    (*midiSetups)[i].setIntoMusyX2((*midiSetupsBuf)[i]);

                idx.m_midiSetups[songId] = midiSetupsBuf;
                setupData += 8 * 16 + 4;
                ++midiSetupsBuf;
            }
        }
        else if (header.type == GroupType::SFX)
        {
            SFXGroupIndex& idx = m_sfxGroups[header.groupId];
            bIdx = &idx;

            /* SFX entries */
            uint16_t count = SBig(*reinterpret_cast<const uint16_t*>(subData + header.pageTableOff));
            idx.m_sfxEntries.reserve(count);
            for (int i=0 ; i<count ; ++i)
            {
                const SFXGroupIndex::SFXEntry* entries =
                    reinterpret_cast<const SFXGroupIndex::SFXEntry*>(subData + header.pageTableOff + 4 + i * 12);
                idx.m_sfxEntries[SBig(entries->defineId)] = entries;
            }
        }

        if (bIdx)
        {
            bIdx->m_soundMacroIndex = reinterpret_cast<const uint16_t*>(subData + header.soundMacroIdsOff);
            bIdx->m_tablesIndex = reinterpret_cast<const uint16_t*>(subData + header.tableIdsOff);
            bIdx->m_keymapsIndex = reinterpret_cast<const uint16_t*>(subData + header.keymapIdsOff);
            bIdx->m_layersIndex = reinterpret_cast<const uint16_t*>(subData + header.layerIdsOff);
        }

        if (absOffs)
            group = reinterpret_cast<const GroupHeader*>(data + header.groupEndOff);
        else
        {
            data += header.groupEndOff;
            group = reinterpret_cast<const GroupHeader*>(data);
        }
    }
}

void AudioGroupProject::_allocateConvBuffers(const unsigned char* data, bool absOffs, PCDataTag)
{
    size_t normPageCount = 0;
    size_t drumPageCount = 0;
    size_t midiSetupCount = 0;

    const GroupHeader* group = reinterpret_cast<const GroupHeader*>(data);
    while (group->groupEndOff != 0xffffffff)
    {
        const unsigned char* subData = absOffs ? data : data + 8;

        if (group->type == GroupType::Song)
        {
            /* Normal pages */
            const MusyX1PageEntry* normEntries =
            reinterpret_cast<const MusyX1PageEntry*>(subData + group->pageTableOff);
            while (normEntries->objId != 0xffff)
            {
                ++normPageCount;
                ++normEntries;
            }

            /* Drum pages */
            const MusyX1PageEntry* drumEntries =
            reinterpret_cast<const MusyX1PageEntry*>(subData + group->drumTableOff);
            while (drumEntries->objId != 0xffff)
            {
                ++drumPageCount;
                ++drumEntries;
            }

            /* MIDI setups */
            const uint8_t* setupData = subData + group->midiSetupsOff;
            while (*reinterpret_cast<const uint32_t*>(setupData) != 0xffffffff)
            {
                ++midiSetupCount;
                setupData += 8 * 16 + 4;
            }
        }

        if (absOffs)
            group = reinterpret_cast<const GroupHeader*>(data + group->groupEndOff);
        else
        {
            data += group->groupEndOff;
            group = reinterpret_cast<const GroupHeader*>(data);
        }
    }

    if (normPageCount)
        m_convNormalPages.reset(new SongGroupIndex::PageEntry[normPageCount]);
    if (drumPageCount)
        m_convDrumPages.reset(new SongGroupIndex::PageEntry[drumPageCount]);
    if (midiSetupCount)
        m_convMidiSetups.reset(new std::array<SongGroupIndex::MIDISetup, 16>[midiSetupCount]);
}

AudioGroupProject::AudioGroupProject(const unsigned char* data, bool absOffs, PCDataTag)
{
    _allocateConvBuffers(data, absOffs, PCDataTag{});
    SongGroupIndex::PageEntry* normPagesBuf = m_convNormalPages.get();
    SongGroupIndex::PageEntry* drumPagesBuf = m_convDrumPages.get();
    std::array<SongGroupIndex::MIDISetup, 16>* midiSetupsBuf = m_convMidiSetups.get();

    const GroupHeader* group = reinterpret_cast<const GroupHeader*>(data);
    while (group->groupEndOff != 0xffffffff)
    {
        const unsigned char* subData = absOffs ? data : data + 8;

        AudioGroupIndex* bIdx = nullptr;

        if (group->type == GroupType::Song)
        {
            SongGroupIndex& idx = m_songGroups[group->groupId];
            bIdx = &idx;

            /* Normal pages */
            const MusyX1PageEntry* normEntries =
                reinterpret_cast<const MusyX1PageEntry*>(subData + group->pageTableOff);
            while (normEntries->objId != 0xffff)
            {
                normEntries->setIntoMusyX2(*normPagesBuf);
                idx.m_normPages[normEntries->programNo] = normPagesBuf;
                ++normEntries;
                ++normPagesBuf;
            }

            /* Drum pages */
            const MusyX1PageEntry* drumEntries =
                reinterpret_cast<const MusyX1PageEntry*>(subData + group->drumTableOff);
            while (drumEntries->objId != 0xffff)
            {
                drumEntries->setIntoMusyX2(*drumPagesBuf);
                idx.m_drumPages[drumEntries->programNo] = drumPagesBuf;
                ++drumEntries;
                ++drumPagesBuf;
            }

            /* MIDI setups */
            const uint8_t* setupData = subData + group->midiSetupsOff;
            while (*reinterpret_cast<const uint32_t*>(setupData) != 0xffffffff)
            {
                uint16_t songId = *reinterpret_cast<const uint16_t*>(setupData);
                const std::array<MusyX1MIDISetup, 16>* midiSetups =
                    reinterpret_cast<const std::array<MusyX1MIDISetup, 16>*>(setupData + 4);

                for (int i=0 ; i<16 ; ++i)
                    (*midiSetups)[i].setIntoMusyX2((*midiSetupsBuf)[i]);

                idx.m_midiSetups[songId] = midiSetupsBuf;
                setupData += 8 * 16 + 4;
                ++midiSetupsBuf;
            }
        }
        else if (group->type == GroupType::SFX)
        {
            SFXGroupIndex& idx = m_sfxGroups[group->groupId];
            bIdx = &idx;

            /* SFX entries */
            uint16_t count = *reinterpret_cast<const uint16_t*>(subData + group->pageTableOff);
            idx.m_sfxEntries.reserve(count);
            for (int i=0 ; i<count ; ++i)
            {
                const SFXGroupIndex::SFXEntry* entries =
                reinterpret_cast<const SFXGroupIndex::SFXEntry*>(subData + group->pageTableOff + 4 + i * 12);
                idx.m_sfxEntries[entries->defineId] = entries;
            }
        }

        if (bIdx)
        {
            bIdx->m_soundMacroIndex = reinterpret_cast<const uint16_t*>(subData + group->soundMacroIdsOff);
            bIdx->m_tablesIndex = reinterpret_cast<const uint16_t*>(subData + group->tableIdsOff);
            bIdx->m_keymapsIndex = reinterpret_cast<const uint16_t*>(subData + group->keymapIdsOff);
            bIdx->m_layersIndex = reinterpret_cast<const uint16_t*>(subData + group->layerIdsOff);
        }

        if (absOffs)
            group = reinterpret_cast<const GroupHeader*>(data + group->groupEndOff);
        else
        {
            data += group->groupEndOff;
            group = reinterpret_cast<const GroupHeader*>(data);
        }
    }
}

AudioGroupProject AudioGroupProject::CreateAudioGroupProject(const AudioGroupData& data)
{
    switch (data.getDataFormat())
    {
    case DataFormat::GCN:
    default:
        return AudioGroupProject(data.getProj(), GCNDataTag{});
    case DataFormat::N64:
        return AudioGroupProject(data.getProj(), data.getAbsoluteProjOffsets(), N64DataTag{});
    case DataFormat::PC:
        return AudioGroupProject(data.getProj(), data.getAbsoluteProjOffsets(), PCDataTag{});
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

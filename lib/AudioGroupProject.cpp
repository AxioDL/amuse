#include "amuse/AudioGroupProject.hpp"
#include "amuse/AudioGroupData.hpp"
#include "athena/MemoryReader.hpp"

namespace amuse
{

static bool AtEnd32(athena::io::IStreamReader& r)
{
    uint32_t v = r.readUint32Big();
    r.seek(-4, athena::Current);
    return v == 0xffffffff;
}

static bool AtEnd16(athena::io::IStreamReader& r)
{
    uint16_t v = r.readUint16Big();
    r.seek(-2, athena::Current);
    return v == 0xffff;
}

AudioGroupProject::AudioGroupProject(athena::io::IStreamReader& r, GCNDataTag)
{
    while (!AtEnd32(r))
    {
        GroupHeader<athena::Big> header;
        header.read(r);

        if (header.type == GroupType::Song)
        {
            SongGroupIndex& idx = m_songGroups[header.groupId];

            /* Normal pages */
            r.seek(header.pageTableOff, athena::Begin);
            while (!AtEnd16(r))
            {
                SongGroupIndex::PageEntryDNA<athena::Big> entry;
                entry.read(r);
                idx.m_normPages[entry.programNo] = entry;
            }

            /* Drum pages */
            r.seek(header.drumTableOff, athena::Begin);
            while (!AtEnd16(r))
            {
                SongGroupIndex::PageEntryDNA<athena::Big> entry;
                entry.read(r);
                idx.m_drumPages[entry.programNo] = entry;
            }

            /* MIDI setups */
            r.seek(header.midiSetupsOff, athena::Begin);
            while (r.position() < header.groupEndOff)
            {
                uint16_t songId = r.readUint16Big();
                r.seek(2, athena::Current);
                std::array<SongGroupIndex::MIDISetup, 16>& setup = idx.m_midiSetups[songId];
                for (int i = 0; i < 16 ; ++i)
                    setup[i].read(r);
            }
        }
        else if (header.type == GroupType::SFX)
        {
            SFXGroupIndex& idx = m_sfxGroups[header.groupId];

            /* SFX entries */
            r.seek(header.pageTableOff, athena::Begin);
            uint16_t count = r.readUint16Big();
            r.seek(2, athena::Current);
            idx.m_sfxEntries.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                SFXGroupIndex::SFXEntryDNA<athena::Big> entry;
                entry.read(r);
                idx.m_sfxEntries[entry.defineId.id] = entry;
            }
        }

        r.seek(header.groupEndOff, athena::Begin);
    }
}

template <athena::Endian DNAE>
AudioGroupProject AudioGroupProject::_AudioGroupProject(athena::io::IStreamReader& r, bool absOffs)
{
    AudioGroupProject ret;

    while (!AtEnd32(r))
    {
        atInt64 groupBegin = r.position();
        atInt64 subDataOff = absOffs ? 0 : groupBegin + 8;
        GroupHeader<DNAE> header;
        header.read(r);

        if (header.type == GroupType::Song)
        {
            SongGroupIndex& idx = ret.m_songGroups[header.groupId];

            if (absOffs)
            {
                /* Normal pages */
                r.seek(header.pageTableOff, athena::Begin);
                while (!AtEnd16(r))
                {
                    SongGroupIndex::PageEntryDNA<DNAE> entry;
                    entry.read(r);
                    idx.m_normPages[entry.programNo] = entry;
                }

                /* Drum pages */
                r.seek(header.drumTableOff, athena::Begin);
                while (!AtEnd16(r))
                {
                    SongGroupIndex::PageEntryDNA<DNAE> entry;
                    entry.read(r);
                    idx.m_drumPages[entry.programNo] = entry;
                }

                /* MIDI setups */
                r.seek(header.midiSetupsOff, athena::Begin);
                while (r.position() < header.groupEndOff)
                {
                    uint16_t songId = r.readUint16Big();
                    r.seek(2, athena::Current);
                    std::array<SongGroupIndex::MIDISetup, 16>& setup = idx.m_midiSetups[songId];
                    for (int i = 0; i < 16 ; ++i)
                        setup[i].read(r);
                }
            }
            else
            {
                /* Normal pages */
                r.seek(subDataOff + header.pageTableOff, athena::Begin);
                while (!AtEnd16(r))
                {
                    SongGroupIndex::MusyX1PageEntryDNA<DNAE> entry;
                    entry.read(r);
                    idx.m_normPages[entry.programNo] = entry;
                }

                /* Drum pages */
                r.seek(subDataOff + header.drumTableOff, athena::Begin);
                while (!AtEnd16(r))
                {
                    SongGroupIndex::MusyX1PageEntryDNA<DNAE> entry;
                    entry.read(r);
                    idx.m_drumPages[entry.programNo] = entry;
                }

                /* MIDI setups */
                r.seek(subDataOff + header.midiSetupsOff, athena::Begin);
                while (r.position() < groupBegin + header.groupEndOff)
                {
                    uint16_t songId = r.readUint16Big();
                    r.seek(2, athena::Current);
                    std::array<SongGroupIndex::MIDISetup, 16>& setup = idx.m_midiSetups[songId];
                    for (int i = 0; i < 16 ; ++i)
                    {
                        SongGroupIndex::MusyX1MIDISetup ent;
                        ent.read(r);
                        setup[i] = ent;
                    }
                }
            }
        }
        else if (header.type == GroupType::SFX)
        {
            SFXGroupIndex& idx = ret.m_sfxGroups[header.groupId];

            /* SFX entries */
            r.seek(subDataOff + header.pageTableOff, athena::Begin);
            uint16_t count = r.readUint16Big();
            r.seek(2, athena::Current);
            idx.m_sfxEntries.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                SFXGroupIndex::SFXEntryDNA<DNAE> entry;
                entry.read(r);
                r.seek(2, athena::Current);
                idx.m_sfxEntries[entry.defineId.id] = entry;
            }
        }

        if (absOffs)
            r.seek(header.groupEndOff, athena::Begin);
        else
            r.seek(groupBegin + header.groupEndOff, athena::Begin);
    }

    return ret;
}

AudioGroupProject AudioGroupProject::CreateAudioGroupProject(const AudioGroupData& data)
{
    athena::io::MemoryReader r(data.getProj(), data.getProjSize());
    switch (data.getDataFormat())
    {
    case DataFormat::GCN:
    default:
        return AudioGroupProject(r, GCNDataTag{});
    case DataFormat::N64:
        return _AudioGroupProject<athena::Big>(r, data.getAbsoluteProjOffsets());
    case DataFormat::PC:
        return _AudioGroupProject<athena::Little>(r, data.getAbsoluteProjOffsets());
    }
}

const SongGroupIndex* AudioGroupProject::getSongGroupIndex(int groupId) const
{
    auto search = m_songGroups.find(groupId);
    if (search != m_songGroups.cend())
        return &search->second;
    return nullptr;
}

const SFXGroupIndex* AudioGroupProject::getSFXGroupIndex(int groupId) const
{
    auto search = m_sfxGroups.find(groupId);
    if (search != m_sfxGroups.cend())
        return &search->second;
    return nullptr;
}

}

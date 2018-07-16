#include "amuse/AudioGroupProject.hpp"
#include "amuse/AudioGroupData.hpp"
#include "athena/MemoryReader.hpp"
#include "athena/FileWriter.hpp"
#include "athena/FileReader.hpp"

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
                idx.m_sfxEntries[entry.sfxId.id] = entry;
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
                    uint16_t songId;
                    athena::io::Read<athena::io::PropType::None>::Do<decltype(songId), DNAE>({}, songId, r);
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
                    uint16_t songId;
                    athena::io::Read<athena::io::PropType::None>::Do<decltype(songId), DNAE>({}, songId, r);
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
            uint16_t count;
            athena::io::Read<athena::io::PropType::None>::Do<decltype(count), DNAE>({}, count, r);
            r.seek(2, athena::Current);
            idx.m_sfxEntries.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                SFXGroupIndex::SFXEntryDNA<DNAE> entry;
                entry.read(r);
                r.seek(2, athena::Current);
                idx.m_sfxEntries[entry.sfxId.id] = entry;
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

AudioGroupProject AudioGroupProject::CreateAudioGroupProject(SystemStringView groupPath)
{
    AudioGroupProject ret;
    SystemString projPath(groupPath);
    projPath += _S("/!project.yaml");
    athena::io::FileReader fi(projPath);

    if (!fi.hasError())
    {
        athena::io::YAMLDocReader r;
        if (r.parse(&fi) && r.ValidateClassType("amuse::Project"))
        {
            size_t songGroupCount;
            if (auto __v = r.enterSubVector("songGroups", songGroupCount))
            {
                ret.m_songGroups.reserve(songGroupCount);
                for (int g = 0; g < songGroupCount; ++g)
                {
                    if (auto __r = r.enterSubRecord(nullptr))
                    {
                        SongGroupIndex& idx = ret.m_songGroups[g];
                        if (auto __v2 = r.enterSubRecord("normPages"))
                        {
                            idx.m_normPages.reserve(r.getCurNode()->m_mapChildren.size());
                            for (const auto& pg : r.getCurNode()->m_mapChildren)
                                if (auto __r2 = r.enterSubRecord(pg.first.c_str()))
                                    idx.m_normPages[strtoul(pg.first.c_str(), nullptr, 0)].read(r);
                        }
                        if (auto __v2 = r.enterSubRecord("drumPages"))
                        {
                            idx.m_drumPages.reserve(r.getCurNode()->m_mapChildren.size());
                            for (const auto& pg : r.getCurNode()->m_mapChildren)
                                if (auto __r2 = r.enterSubRecord(pg.first.c_str()))
                                    idx.m_drumPages[strtoul(pg.first.c_str(), nullptr, 0)].read(r);
                        }
                        if (auto __v2 = r.enterSubRecord("songs"))
                        {
                            idx.m_midiSetups.reserve(r.getCurNode()->m_mapChildren.size());
                            for (const auto& song : r.getCurNode()->m_mapChildren)
                            {
                                size_t chanCount;
                                if (auto __v3 = r.enterSubVector(song.first.c_str(), chanCount))
                                {
                                    ObjectId songId = SongId::CurNameDB->generateId(NameDB::Type::Song);
                                    SongId::CurNameDB->registerPair(song.first, songId);
                                    std::array<SongGroupIndex::MIDISetup, 16>& setup = idx.m_midiSetups[songId];
                                    for (int i = 0; i < 16 && i < chanCount; ++i)
                                        if (auto __r2 = r.enterSubRecord(nullptr))
                                            setup[i].read(r);
                                }
                            }
                        }
                    }
                }
            }

            size_t sfxGroupCount;
            if (auto __v = r.enterSubVector("sfxGroups", sfxGroupCount))
            {
                ret.m_sfxGroups.reserve(sfxGroupCount);
                for (int g = 0; g < sfxGroupCount; ++g)
                {
                    if (auto __r = r.enterSubRecord(nullptr))
                    {
                        SFXGroupIndex& idx = ret.m_sfxGroups[g];
                        for (const auto& sfx : r.getCurNode()->m_mapChildren)
                            if (auto __r2 = r.enterSubRecord(sfx.first.c_str()))
                            {
                                ObjectId sfxId = SFXId::CurNameDB->generateId(NameDB::Type::SFX);
                                SFXId::CurNameDB->registerPair(sfx.first, sfxId);
                                idx.m_sfxEntries[sfxId].read(r);
                            }
                    }
                }
            }
        }
    }

    return ret;
}

template <athena::Endian DNAE>
static void ReadRangedObjectIds(NameDB* db, athena::io::IStreamReader& r, NameDB::Type tp)
{
    uint16_t id;
    athena::io::Read<athena::io::PropType::None>::Do<decltype(id), DNAE>({}, id, r);
    if ((id & 0x8000) == 0x8000)
    {
        uint16_t endId;
        athena::io::Read<athena::io::PropType::None>::Do<decltype(endId), DNAE>({}, endId, r);
        for (uint16_t i = uint16_t(id & 0x7fff); i <= uint16_t(endId & 0x7fff); ++i)
        {
            ObjectId useId = i;
            if (tp == NameDB::Type::Layer)
                useId.id |= 0x8000;
            db->registerPair(NameDB::generateName(useId, tp), useId);
        }
    }
    else
    {
        db->registerPair(NameDB::generateName(id, tp), id);
    }
}

void AudioGroupProject::BootstrapObjectIDs(athena::io::IStreamReader& r, GCNDataTag)
{
    while (!AtEnd32(r))
    {
        GroupHeader<athena::Big> header;
        header.read(r);

        /* Sound Macros */
        r.seek(header.soundMacroIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<athena::Big>(SoundMacroId::CurNameDB, r, NameDB::Type::SoundMacro);

        /* Samples */
        r.seek(header.samplIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<athena::Big>(SampleId::CurNameDB, r, NameDB::Type::Sample);

        /* Tables */
        r.seek(header.tableIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<athena::Big>(TableId::CurNameDB, r, NameDB::Type::Table);

        /* Keymaps */
        r.seek(header.keymapIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<athena::Big>(KeymapId::CurNameDB, r, NameDB::Type::Keymap);

        /* Layers */
        r.seek(header.layerIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<athena::Big>(LayersId::CurNameDB, r, NameDB::Type::Layer);

        if (header.type == GroupType::Song)
        {
            /* MIDI setups */
            r.seek(header.midiSetupsOff, athena::Begin);
            while (r.position() < header.groupEndOff)
            {
                uint16_t id = r.readUint16Big();
                SongId::CurNameDB->registerPair(NameDB::generateName(id, NameDB::Type::Song), id);
                r.seek(2 + 5 * 16, athena::Current);
            }
        }
        else if (header.type == GroupType::SFX)
        {
            /* SFX entries */
            r.seek(header.pageTableOff, athena::Begin);
            uint16_t count = r.readUint16Big();
            r.seek(2, athena::Current);
            for (int i = 0; i < count; ++i)
            {
                SFXGroupIndex::SFXEntryDNA<athena::Big> entry;
                entry.read(r);
                SFXId::CurNameDB->registerPair(
                    NameDB::generateName(entry.sfxId.id, NameDB::Type::SFX), entry.sfxId.id);
            }
        }

        r.seek(header.groupEndOff, athena::Begin);
    }
}

template <athena::Endian DNAE>
void AudioGroupProject::BootstrapObjectIDs(athena::io::IStreamReader& r, bool absOffs)
{
    while (!AtEnd32(r))
    {
        atInt64 groupBegin = r.position();
        atInt64 subDataOff = absOffs ? 0 : groupBegin + 8;
        GroupHeader<DNAE> header;
        header.read(r);

        /* Sound Macros */
        r.seek(subDataOff + header.soundMacroIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<DNAE>(SoundMacroId::CurNameDB, r, NameDB::Type::SoundMacro);

        /* Samples */
        r.seek(subDataOff + header.samplIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<DNAE>(SampleId::CurNameDB, r, NameDB::Type::Sample);

        /* Tables */
        r.seek(subDataOff + header.tableIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<DNAE>(TableId::CurNameDB, r, NameDB::Type::Table);

        /* Keymaps */
        r.seek(subDataOff + header.keymapIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<DNAE>(KeymapId::CurNameDB, r, NameDB::Type::Keymap);

        /* Layers */
        r.seek(subDataOff + header.layerIdsOff, athena::Begin);
        while (!AtEnd16(r))
            ReadRangedObjectIds<DNAE>(LayersId::CurNameDB, r, NameDB::Type::Layer);

        if (header.type == GroupType::Song)
        {
            /* MIDI setups */
            if (absOffs)
            {
                r.seek(header.midiSetupsOff, athena::Begin);
                while (r.position() < header.groupEndOff)
                {
                    uint16_t id;
                    athena::io::Read<athena::io::PropType::None>::Do<decltype(id), DNAE>({}, id, r);
                    SongId::CurNameDB->registerPair(NameDB::generateName(id, NameDB::Type::Song), id);
                    r.seek(2 + 5 * 16, athena::Current);
                }
            }
            else
            {
                r.seek(subDataOff + header.midiSetupsOff, athena::Begin);
                while (r.position() < groupBegin + header.groupEndOff)
                {
                    uint16_t id;
                    athena::io::Read<athena::io::PropType::None>::Do<decltype(id), DNAE>({}, id, r);
                    SongId::CurNameDB->registerPair(NameDB::generateName(id, NameDB::Type::Song), id);
                    r.seek(2 + 8 * 16, athena::Current);
                }
            }
        }
        else if (header.type == GroupType::SFX)
        {
            /* SFX entries */
            r.seek(subDataOff + header.pageTableOff, athena::Begin);
            uint16_t count;
            athena::io::Read<athena::io::PropType::None>::Do<decltype(count), DNAE>({}, count, r);
            r.seek(2, athena::Current);
            for (int i = 0; i < count; ++i)
            {
                SFXGroupIndex::SFXEntryDNA<DNAE> entry;
                entry.read(r);
                r.seek(2, athena::Current);
                SFXId::CurNameDB->registerPair(
                    NameDB::generateName(entry.sfxId.id, NameDB::Type::SFX), entry.sfxId.id);
            }
        }

        if (absOffs)
            r.seek(header.groupEndOff, athena::Begin);
        else
            r.seek(groupBegin + header.groupEndOff, athena::Begin);
    }
}

void AudioGroupProject::BootstrapObjectIDs(const AudioGroupData& data)
{
    athena::io::MemoryReader r(data.getProj(), data.getProjSize());
    switch (data.getDataFormat())
    {
    case DataFormat::GCN:
    default:
        BootstrapObjectIDs(r, GCNDataTag{});
        break;
    case DataFormat::N64:
        BootstrapObjectIDs<athena::Big>(r, data.getAbsoluteProjOffsets());
        break;
    case DataFormat::PC:
        BootstrapObjectIDs<athena::Little>(r, data.getAbsoluteProjOffsets());
        break;
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

bool AudioGroupProject::toYAML(SystemStringView groupPath) const
{
    athena::io::YAMLDocWriter w("amuse::Project");

    if (!m_songGroups.empty())
    {
        if (auto __v = w.enterSubVector("songGroups"))
        {
            for (const auto& p : SortUnorderedMap(m_songGroups))
            {
                if (auto __r = w.enterSubRecord(nullptr))
                {
                    if (!p.second.get().m_normPages.empty())
                    {
                        if (auto __v2 = w.enterSubRecord("normPages"))
                        {
                            for (const auto& pg : SortUnorderedMap(p.second.get().m_normPages))
                            {
                                char name[16];
                                snprintf(name, 16, "%d", pg.first);
                                if (auto __r2 = w.enterSubRecord(name))
                                {
                                    w.setStyle(athena::io::YAMLNodeStyle::Flow);
                                    pg.second.get().write(w);
                                }
                            }
                        }
                    }
                    if (!p.second.get().m_drumPages.empty())
                    {
                        if (auto __v2 = w.enterSubRecord("drumPages"))
                        {
                            for (const auto& pg : SortUnorderedMap(p.second.get().m_drumPages))
                            {
                                char name[16];
                                snprintf(name, 16, "%d", pg.first);
                                if (auto __r2 = w.enterSubRecord(name))
                                {
                                    w.setStyle(athena::io::YAMLNodeStyle::Flow);
                                    pg.second.get().write(w);
                                }
                            }
                        }
                    }
                    if (!p.second.get().m_midiSetups.empty())
                    {
                        if (auto __v2 = w.enterSubRecord("songs"))
                        {
                            for (const auto& song : SortUnorderedMap(p.second.get().m_midiSetups))
                            {
                                if (auto __v3 = w.enterSubVector(SongId::CurNameDB->resolveNameFromId(song.first).data()))
                                    for (int i = 0; i < 16; ++i)
                                        if (auto __r2 = w.enterSubRecord(nullptr))
                                        {
                                            w.setStyle(athena::io::YAMLNodeStyle::Flow);
                                            song.second.get()[i].write(w);
                                        }
                            }
                        }
                    }
                }
            }
        }
    }

    if (!m_sfxGroups.empty())
    {
        if (auto __v = w.enterSubVector("sfxGroups"))
        {
            for (const auto& p : SortUnorderedMap(m_sfxGroups))
            {
                if (auto __r = w.enterSubRecord(nullptr))
                {
                    for (const auto& sfx : SortUnorderedMap(p.second.get().m_sfxEntries))
                    {
                        if (auto __r2 = w.enterSubRecord(SFXId::CurNameDB->resolveNameFromId(sfx.first).data()))
                        {
                            w.setStyle(athena::io::YAMLNodeStyle::Flow);
                            sfx.second.get().write(w);
                        }
                    }
                }
            }
        }
    }

    SystemString projPath(groupPath);
    projPath += _S("/!project.yaml");
    athena::io::FileWriter fo(projPath);
    return w.finish(&fo);
}

}

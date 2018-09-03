#include <athena/VectorWriter.hpp>
#include "amuse/AudioGroupProject.hpp"
#include "amuse/AudioGroupPool.hpp"
#include "amuse/AudioGroupSampleDirectory.hpp"
#include "amuse/AudioGroupData.hpp"
#include "athena/MemoryReader.hpp"
#include "athena/FileWriter.hpp"
#include "athena/FileReader.hpp"

namespace amuse
{

static bool AtEnd64(athena::io::IStreamReader& r)
{
    uint64_t v = r.readUint64Big();
    r.seek(-8, athena::Current);
    return v == 0xffffffffffffffff;
}

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
            else if (tp == NameDB::Type::Keymap)
                useId.id |= 0x4000;
            if (db)
                db->registerPair(NameDB::generateName(useId, tp), useId);
        }
    }
    else
    {
        if (tp == NameDB::Type::Layer)
            id |= 0x8000;
        else if (tp == NameDB::Type::Keymap)
            id |= 0x4000;
        if (db)
            db->registerPair(NameDB::generateName(id, tp), id);
    }
}

template <athena::Endian DNAE, class T>
static void WriteRangedObjectIds(athena::io::IStreamWriter& w, const T& list)
{
    if (list.cbegin() == list.cend())
        return;
    bool inRange = false;
    uint16_t lastId = list.cbegin()->first & 0x3fff;
    for (auto it = list.cbegin() + 1; it != list.cend(); ++it)
    {
        uint16_t thisId = it->first & 0x3fff;
        if (thisId == lastId + 1)
        {
            if (!inRange)
            {
                inRange = true;
                lastId |= 0x8000;
                athena::io::Write<athena::io::PropType::None>::Do<decltype(lastId), DNAE>({}, lastId, w);
            }
        }
        else
        {
            inRange = false;
            athena::io::Write<athena::io::PropType::None>::Do<decltype(lastId), DNAE>({}, lastId, w);
        }
        lastId = thisId;
    }
    athena::io::Write<athena::io::PropType::None>::Do<decltype(lastId), DNAE>({}, lastId, w);
    uint16_t term = 0xffff;
    athena::io::Write<athena::io::PropType::None>::Do<decltype(term), DNAE>({}, term, w);
}

AudioGroupProject::AudioGroupProject(athena::io::IStreamReader& r, GCNDataTag)
{
    while (!AtEnd32(r))
    {
        GroupHeader<athena::Big> header;
        header.read(r);

        if (GroupId::CurNameDB)
            GroupId::CurNameDB->registerPair(NameDB::generateName(header.groupId, NameDB::Type::Group), header.groupId);

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
            auto& idx = m_songGroups[header.groupId];
            idx = MakeObj<SongGroupIndex>();

            /* Normal pages */
            r.seek(header.pageTableOff, athena::Begin);
            while (!AtEnd64(r))
            {
                SongGroupIndex::PageEntryDNA<athena::Big> entry;
                entry.read(r);
                idx->m_normPages[entry.programNo] = entry;
            }

            /* Drum pages */
            r.seek(header.drumTableOff, athena::Begin);
            while (!AtEnd64(r))
            {
                SongGroupIndex::PageEntryDNA<athena::Big> entry;
                entry.read(r);
                idx->m_drumPages[entry.programNo] = entry;
            }

            /* MIDI setups */
            r.seek(header.midiSetupsOff, athena::Begin);
            while (r.position() < header.groupEndOff)
            {
                uint16_t songId = r.readUint16Big();
                r.seek(2, athena::Current);
                std::array<SongGroupIndex::MIDISetup, 16>& setup = idx->m_midiSetups[songId];
                for (int i = 0; i < 16 ; ++i)
                    setup[i].read(r);
                if (SongId::CurNameDB)
                    SongId::CurNameDB->registerPair(NameDB::generateName(songId, NameDB::Type::Song), songId);
            }
        }
        else if (header.type == GroupType::SFX)
        {
            auto& idx = m_sfxGroups[header.groupId];
            idx = MakeObj<SFXGroupIndex>();

            /* SFX entries */
            r.seek(header.pageTableOff, athena::Begin);
            uint16_t count = r.readUint16Big();
            r.seek(2, athena::Current);
            idx->m_sfxEntries.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                SFXGroupIndex::SFXEntryDNA<athena::Big> entry;
                entry.read(r);
                idx->m_sfxEntries[entry.sfxId.id] = entry;
                if (SFXId::CurNameDB)
                    SFXId::CurNameDB->registerPair(
                    NameDB::generateName(entry.sfxId.id, NameDB::Type::SFX), entry.sfxId.id);
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

        GroupId::CurNameDB->registerPair(NameDB::generateName(header.groupId, NameDB::Type::Group), header.groupId);

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
            auto& idx = ret.m_songGroups[header.groupId];
            idx = MakeObj<SongGroupIndex>();

            if (absOffs)
            {
                /* Normal pages */
                r.seek(header.pageTableOff, athena::Begin);
                while (!AtEnd16(r))
                {
                    SongGroupIndex::PageEntryDNA<DNAE> entry;
                    entry.read(r);
                    idx->m_normPages[entry.programNo] = entry;
                }

                /* Drum pages */
                r.seek(header.drumTableOff, athena::Begin);
                while (!AtEnd16(r))
                {
                    SongGroupIndex::PageEntryDNA<DNAE> entry;
                    entry.read(r);
                    idx->m_drumPages[entry.programNo] = entry;
                }

                /* MIDI setups */
                r.seek(header.midiSetupsOff, athena::Begin);
                while (r.position() < header.groupEndOff)
                {
                    uint16_t songId;
                    athena::io::Read<athena::io::PropType::None>::Do<decltype(songId), DNAE>({}, songId, r);
                    r.seek(2, athena::Current);
                    std::array<SongGroupIndex::MIDISetup, 16>& setup = idx->m_midiSetups[songId];
                    for (int i = 0; i < 16 ; ++i)
                        setup[i].read(r);
                    SongId::CurNameDB->registerPair(NameDB::generateName(songId, NameDB::Type::Song), songId);
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
                    idx->m_normPages[entry.programNo] = entry;
                }

                /* Drum pages */
                r.seek(subDataOff + header.drumTableOff, athena::Begin);
                while (!AtEnd16(r))
                {
                    SongGroupIndex::MusyX1PageEntryDNA<DNAE> entry;
                    entry.read(r);
                    idx->m_drumPages[entry.programNo] = entry;
                }

                /* MIDI setups */
                r.seek(subDataOff + header.midiSetupsOff, athena::Begin);
                while (r.position() < groupBegin + header.groupEndOff)
                {
                    uint16_t songId;
                    athena::io::Read<athena::io::PropType::None>::Do<decltype(songId), DNAE>({}, songId, r);
                    r.seek(2, athena::Current);
                    std::array<SongGroupIndex::MIDISetup, 16>& setup = idx->m_midiSetups[songId];
                    for (int i = 0; i < 16 ; ++i)
                    {
                        SongGroupIndex::MusyX1MIDISetup ent;
                        ent.read(r);
                        setup[i] = ent;
                    }
                    SongId::CurNameDB->registerPair(NameDB::generateName(songId, NameDB::Type::Song), songId);
                }
            }
        }
        else if (header.type == GroupType::SFX)
        {
            auto& idx = ret.m_sfxGroups[header.groupId];
            idx = MakeObj<SFXGroupIndex>();

            /* SFX entries */
            r.seek(subDataOff + header.pageTableOff, athena::Begin);
            uint16_t count;
            athena::io::Read<athena::io::PropType::None>::Do<decltype(count), DNAE>({}, count, r);
            r.seek(2, athena::Current);
            idx->m_sfxEntries.reserve(count);
            for (int i = 0; i < count; ++i)
            {
                SFXGroupIndex::SFXEntryDNA<DNAE> entry;
                entry.read(r);
                r.seek(2, athena::Current);
                idx->m_sfxEntries[entry.sfxId.id] = entry;
                SFXId::CurNameDB->registerPair(
                    NameDB::generateName(entry.sfxId.id, NameDB::Type::SFX), entry.sfxId.id);
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
    if (data.getProjSize() < 4)
        return {};
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

std::string ParseStringSlashId(const std::string& str, uint16_t& idOut)
{
    size_t slashPos = str.find('/');
    if (slashPos == std::string::npos)
        return {};
    idOut = uint16_t(strtoul(str.data() + slashPos + 1, nullptr, 0));
    return {str.begin(), str.begin() + slashPos};
}

void SongGroupIndex::fromYAML(athena::io::YAMLDocReader& r)
{
    if (auto __v2 = r.enterSubRecord("normPages"))
    {
        m_normPages.reserve(r.getCurNode()->m_mapChildren.size());
        for (const auto& pg : r.getCurNode()->m_mapChildren)
            if (auto __r2 = r.enterSubRecord(pg.first.c_str()))
                m_normPages[strtoul(pg.first.c_str(), nullptr, 0)].read(r);
    }
    if (auto __v2 = r.enterSubRecord("drumPages"))
    {
        m_drumPages.reserve(r.getCurNode()->m_mapChildren.size());
        for (const auto& pg : r.getCurNode()->m_mapChildren)
            if (auto __r2 = r.enterSubRecord(pg.first.c_str()))
                m_drumPages[strtoul(pg.first.c_str(), nullptr, 0)].read(r);
    }
    if (auto __v2 = r.enterSubRecord("songs"))
    {
        m_midiSetups.reserve(r.getCurNode()->m_mapChildren.size());
        for (const auto& song : r.getCurNode()->m_mapChildren)
        {
            size_t chanCount;
            if (auto __v3 = r.enterSubVector(song.first.c_str(), chanCount))
            {
                uint16_t songId;
                std::string songName = ParseStringSlashId(song.first, songId);
                if (songName.empty() || songId == 0xffff)
                    continue;
                SongId::CurNameDB->registerPair(songName, songId);

                std::array<SongGroupIndex::MIDISetup, 16>& setup = m_midiSetups[songId];
                for (int i = 0; i < 16 && i < chanCount; ++i)
                    if (auto __r2 = r.enterSubRecord(nullptr))
                        setup[i].read(r);
            }
        }
    }
}

void SFXGroupIndex::fromYAML(athena::io::YAMLDocReader& r)
{
    for (const auto& sfx : r.getCurNode()->m_mapChildren)
        if (auto __r2 = r.enterSubRecord(sfx.first.c_str()))
        {
            uint16_t sfxId;
            std::string sfxName = ParseStringSlashId(sfx.first, sfxId);
            if (sfxName.empty() || sfxId == 0xffff)
                continue;
            SFXId::CurNameDB->registerPair(sfxName, sfxId);
            m_sfxEntries[sfxId].read(r);
        }
}

AudioGroupProject AudioGroupProject::CreateAudioGroupProject(SystemStringView groupPath)
{
    AudioGroupProject ret;
    SystemString projPath(groupPath);
    projPath += _S("/!project.yaml");
    athena::io::FileReader fi(projPath, 32 * 1024, false);

    if (!fi.hasError())
    {
        athena::io::YAMLDocReader r;
        if (r.parse(&fi) && !r.readString("DNAType").compare("amuse::Project"))
        {
            if (auto __v = r.enterSubRecord("songGroups"))
            {
                ret.m_songGroups.reserve(r.getCurNode()->m_mapChildren.size());
                for (const auto& grp : r.getCurNode()->m_mapChildren)
                {
                    if (auto __r = r.enterSubRecord(grp.first.c_str()))
                    {
                        uint16_t groupId;
                        std::string groupName = ParseStringSlashId(grp.first, groupId);
                        if (groupName.empty() || groupId == 0xffff)
                            continue;
                        GroupId::CurNameDB->registerPair(groupName, groupId);

                        auto& idx = ret.m_songGroups[groupId];
                        idx = MakeObj<SongGroupIndex>();
                        idx->fromYAML(r);
                    }
                }
            }

            if (auto __v = r.enterSubRecord("sfxGroups"))
            {
                ret.m_sfxGroups.reserve(r.getCurNode()->m_mapChildren.size());
                for (const auto& grp : r.getCurNode()->m_mapChildren)
                {
                    if (auto __r = r.enterSubRecord(grp.first.c_str()))
                    {
                        uint16_t groupId;
                        std::string groupName = ParseStringSlashId(grp.first, groupId);
                        if (groupName.empty() || groupId == 0xffff)
                            continue;
                        GroupId::CurNameDB->registerPair(groupName, groupId);

                        auto& idx = ret.m_sfxGroups[groupId];
                        idx = MakeObj<SFXGroupIndex>();
                        idx->fromYAML(r);
                    }
                }
            }
        }
    }

    return ret;
}

static ObjectId RegisterDedupedName(ObjectId origId, amuse::NameDB* db, NameDB::Type tp)
{
    std::string dupeName = std::string(db->resolveNameFromId(origId)) + "-copy";
    std::string useName = dupeName;
    int dupeIdx = 1;
    while (db->m_stringToId.find(useName) != db->m_stringToId.cend())
    {
        char num[16];
        snprintf(num, 16, "%d", dupeIdx++);
        useName = dupeName + num;
    }
    ObjectId ret = db->generateId(tp);
    db->registerPair(useName, ret);
    return ret;
}

SFXGroupIndex::SFXGroupIndex(const SFXGroupIndex& other)
{
    for (const auto& sfx : other.m_sfxEntries)
        m_sfxEntries[RegisterDedupedName(sfx.first, SFXId::CurNameDB, NameDB::Type::SFX)] = sfx.second;
}

AudioGroupProject AudioGroupProject::CreateAudioGroupProject(const AudioGroupProject& oldProj)
{
    AudioGroupProject ret;

    for (const auto& grp : oldProj.songGroups())
        ret.m_songGroups[RegisterDedupedName(grp.first, GroupId::CurNameDB, NameDB::Type::Group)] =
            MakeObj<SongGroupIndex>(*grp.second);
    for (const auto& grp : oldProj.sfxGroups())
        ret.m_sfxGroups[RegisterDedupedName(grp.first, GroupId::CurNameDB, NameDB::Type::Group)] =
            MakeObj<SFXGroupIndex>(*grp.second);

    return ret;
}

void AudioGroupProject::BootstrapObjectIDs(athena::io::IStreamReader& r, GCNDataTag)
{
    while (!AtEnd32(r))
    {
        GroupHeader<athena::Big> header;
        header.read(r);

        GroupId::CurNameDB->registerPair(NameDB::generateName(header.groupId, NameDB::Type::Group), header.groupId);

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

        GroupId::CurNameDB->registerPair(NameDB::generateName(header.groupId, NameDB::Type::Group), header.groupId);

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
        return search->second.get();
    return nullptr;
}

const SFXGroupIndex* AudioGroupProject::getSFXGroupIndex(int groupId) const
{
    auto search = m_sfxGroups.find(groupId);
    if (search != m_sfxGroups.cend())
        return search->second.get();
    return nullptr;
}

void SongGroupIndex::toYAML(athena::io::YAMLDocWriter& w) const
{
    if (!m_normPages.empty())
    {
        if (auto __v2 = w.enterSubRecord("normPages"))
        {
            for (const auto& pg : SortUnorderedMap(m_normPages))
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
    if (!m_drumPages.empty())
    {
        if (auto __v2 = w.enterSubRecord("drumPages"))
        {
            for (const auto& pg : SortUnorderedMap(m_drumPages))
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
    if (!m_midiSetups.empty())
    {
        if (auto __v2 = w.enterSubRecord("songs"))
        {
            for (const auto& song : SortUnorderedMap(m_midiSetups))
            {
                char songString[64];
                snprintf(songString, 64, "%s/0x%04X",
                    SongId::CurNameDB->resolveNameFromId(song.first).data(), int(song.first.id));
                if (auto __v3 = w.enterSubVector(songString))
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

void SFXGroupIndex::toYAML(athena::io::YAMLDocWriter& w) const
{
    for (const auto& sfx : SortUnorderedMap(m_sfxEntries))
    {
        char sfxString[64];
        snprintf(sfxString, 64, "%s/0x%04X",
            SFXId::CurNameDB->resolveNameFromId(sfx.first).data(), int(sfx.first.id));
        if (auto __r2 = w.enterSubRecord(sfxString))
        {
            w.setStyle(athena::io::YAMLNodeStyle::Flow);
            sfx.second.get().write(w);
        }
    }
}

std::vector<uint8_t> AudioGroupProject::toYAML() const
{
    athena::io::YAMLDocWriter w("amuse::Project");

    if (!m_songGroups.empty())
    {
        if (auto __v = w.enterSubRecord("songGroups"))
        {
            for (const auto& p : SortUnorderedMap(m_songGroups))
            {
                char groupString[64];
                snprintf(groupString, 64, "%s/0x%04X", GroupId::CurNameDB->resolveNameFromId(p.first).data(), int(p.first.id));
                if (auto __r = w.enterSubRecord(groupString))
                {
                    p.second.get()->toYAML(w);
                }
            }
        }
    }

    if (!m_sfxGroups.empty())
    {
        if (auto __v = w.enterSubRecord("sfxGroups"))
        {
            for (const auto& p : SortUnorderedMap(m_sfxGroups))
            {
                char groupString[64];
                snprintf(groupString, 64, "%s/0x%04X", GroupId::CurNameDB->resolveNameFromId(p.first).data(), int(p.first.id));
                if (auto __r = w.enterSubRecord(groupString))
                {
                    p.second.get()->toYAML(w);
                }
            }
        }
    }

    athena::io::VectorWriter fo;
    w.finish(&fo);
    return fo.data();
}

#if 0
struct ObjectIdPool
{
    std::unordered_set<SoundMacroId> soundMacros;
    std::unordered_set<SampleId> samples;
    std::unordered_set<TableId> tables;
    std::unordered_set<KeymapId> keymaps;
    std::unordered_set<LayersId> layers;

    void _recursiveAddSoundMacro(SoundMacroId id, const AudioGroupPool& pool)
    {
        if (soundMacros.find(id) != soundMacros.cend())
            return;
        const SoundMacro* macro = pool.soundMacro(id);
        if (!macro)
            return;
        soundMacros.insert(id);
        for (const auto& cmd : macro->m_cmds)
        {
            switch (cmd->Isa())
            {
            case SoundMacro::CmdOp::StartSample:
                samples.insert(static_cast<SoundMacro::CmdStartSample*>(cmd.get())->sample);
                break;
            case SoundMacro::CmdOp::SetAdsr:
                tables.insert(static_cast<SoundMacro::CmdSetAdsr*>(cmd.get())->table);
                break;
            case SoundMacro::CmdOp::ScaleVolume:
                tables.insert(static_cast<SoundMacro::CmdScaleVolume*>(cmd.get())->table);
                break;
            case SoundMacro::CmdOp::Envelope:
                tables.insert(static_cast<SoundMacro::CmdEnvelope*>(cmd.get())->table);
                break;
            case SoundMacro::CmdOp::FadeIn:
                tables.insert(static_cast<SoundMacro::CmdFadeIn*>(cmd.get())->table);
                break;
            case SoundMacro::CmdOp::SetPitchAdsr:
                tables.insert(static_cast<SoundMacro::CmdSetPitchAdsr*>(cmd.get())->table);
                break;
            case SoundMacro::CmdOp::SplitKey:
                _recursiveAddSoundMacro(static_cast<SoundMacro::CmdSplitKey*>(cmd.get())->macro, pool);
                break;
            case SoundMacro::CmdOp::SplitVel:
                _recursiveAddSoundMacro(static_cast<SoundMacro::CmdSplitVel*>(cmd.get())->macro, pool);
                break;
            case SoundMacro::CmdOp::Goto:
                _recursiveAddSoundMacro(static_cast<SoundMacro::CmdGoto*>(cmd.get())->macro, pool);
                break;
            case SoundMacro::CmdOp::PlayMacro:
                _recursiveAddSoundMacro(static_cast<SoundMacro::CmdPlayMacro*>(cmd.get())->macro, pool);
                break;
            case SoundMacro::CmdOp::SplitMod:
                _recursiveAddSoundMacro(static_cast<SoundMacro::CmdSplitMod*>(cmd.get())->macro, pool);
                break;
            case SoundMacro::CmdOp::SplitRnd:
                _recursiveAddSoundMacro(static_cast<SoundMacro::CmdSplitRnd*>(cmd.get())->macro, pool);
                break;
            case SoundMacro::CmdOp::GoSub:
                _recursiveAddSoundMacro(static_cast<SoundMacro::CmdGoSub*>(cmd.get())->macro, pool);
                break;
            case SoundMacro::CmdOp::TrapEvent:
                _recursiveAddSoundMacro(static_cast<SoundMacro::CmdTrapEvent*>(cmd.get())->macro, pool);
                break;
            case SoundMacro::CmdOp::SendMessage:
                _recursiveAddSoundMacro(static_cast<SoundMacro::CmdSendMessage*>(cmd.get())->macro, pool);
                break;
            default:
                break;
            }

        }
    }

    void addRootId(ObjectId id, const AudioGroupPool& pool)
    {
        if (id & 0x8000)
        {
            if (const std::vector<LayerMapping>* lms = pool.layer(id))
            {
                layers.insert(id);
                for (const auto& lm : *lms)
                    _recursiveAddSoundMacro(lm.macro, pool);
            }
        }
        else if (id & 0x4000)
        {
            if (const auto* kms = pool.keymap(id))
            {
                keymaps.insert(id);
                for (int i = 0; i < 128; ++i)
                    _recursiveAddSoundMacro(kms[i].macro, pool);
            }
        }
        else
        {
            _recursiveAddSoundMacro(id, pool);
        }
    }

    void cleanup()
    {
        soundMacros.erase(SoundMacroId{});
        samples.erase(SampleId{});
        tables.erase(TableId{});
        keymaps.erase(KeymapId{});
        layers.erase(LayersId{});
    }
};
#endif

std::vector<uint8_t> AudioGroupProject::toGCNData(const AudioGroupPool& pool,
                                                  const AudioGroupSampleDirectory& sdir) const
{
    constexpr athena::Endian DNAE = athena::Big;

    athena::io::VectorWriter fo;

    std::vector<GroupId> groupIds;
    groupIds.reserve(m_songGroups.size() + m_sfxGroups.size());
    for (auto& p : m_songGroups)
        groupIds.push_back(p.first);
    for (auto& p : m_sfxGroups)
        groupIds.push_back(p.first);
    std::sort(groupIds.begin(), groupIds.end());

    const uint64_t term64 = 0xffffffffffffffff;
    const uint16_t padding = 0;

    for (GroupId id : groupIds)
    {
        auto search = m_songGroups.find(id);
        if (search != m_songGroups.end())
        {
            const SongGroupIndex& index = *search->second;

            auto groupStart = fo.position();
            GroupHeader<DNAE> header = {};
            header.write(fo);

            header.groupId = id;
            header.type = GroupType::Song;

            header.soundMacroIdsOff = fo.position();
            WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(pool.soundMacros()));
            header.samplIdsOff = fo.position();
            WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(sdir.sampleEntries()));
            header.tableIdsOff = fo.position();
            WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(pool.tables()));
            header.keymapIdsOff = fo.position();
            WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(pool.keymaps()));
            header.layerIdsOff = fo.position();
            WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(pool.layers()));

            header.pageTableOff = fo.position();
            for (auto& p : SortUnorderedMap(index.m_normPages))
            {
                SongGroupIndex::PageEntryDNA<DNAE> entry = p.second.get().toDNA<DNAE>(p.first);
                entry.write(fo);
            }
            athena::io::Write<athena::io::PropType::None>::Do<decltype(term64), DNAE>({}, term64, fo);

            header.drumTableOff = fo.position();
            for (auto& p : SortUnorderedMap(index.m_drumPages))
            {
                SongGroupIndex::PageEntryDNA<DNAE> entry = p.second.get().toDNA<DNAE>(p.first);
                entry.write(fo);
            }
            athena::io::Write<athena::io::PropType::None>::Do<decltype(term64), DNAE>({}, term64, fo);

            header.midiSetupsOff = fo.position();
            for (auto& p : SortUnorderedMap(index.m_midiSetups))
            {
                uint16_t songId = p.first.id;
                athena::io::Write<athena::io::PropType::None>::Do<decltype(songId), DNAE>({}, songId, fo);
                athena::io::Write<athena::io::PropType::None>::Do<decltype(padding), DNAE>({}, padding, fo);

                const std::array<SongGroupIndex::MIDISetup, 16>& setup = p.second.get();
                for (int i = 0; i < 16 ; ++i)
                    setup[i].write(fo);
            }

            header.groupEndOff = fo.position();
            fo.seek(groupStart, athena::Begin);
            header.write(fo);
            fo.seek(header.groupEndOff, athena::Begin);
        }
        else
        {
            auto search2 = m_sfxGroups.find(id);
            if (search2 != m_sfxGroups.end())
            {
                const SFXGroupIndex& index = *search2->second;

                auto groupStart = fo.position();
                GroupHeader<DNAE> header = {};
                header.write(fo);

                header.groupId = id;
                header.type = GroupType::SFX;

                header.soundMacroIdsOff = fo.position();
                WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(pool.soundMacros()));
                header.samplIdsOff = fo.position();
                WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(sdir.sampleEntries()));
                header.tableIdsOff = fo.position();
                WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(pool.tables()));
                header.keymapIdsOff = fo.position();
                WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(pool.keymaps()));
                header.layerIdsOff = fo.position();
                WriteRangedObjectIds<DNAE>(fo, SortUnorderedMap(pool.layers()));

                header.pageTableOff = fo.position();
                uint16_t count = index.m_sfxEntries.size();
                athena::io::Write<athena::io::PropType::None>::Do<decltype(count), DNAE>({}, count, fo);
                athena::io::Write<athena::io::PropType::None>::Do<decltype(padding), DNAE>({}, padding, fo);
                for (auto& p : SortUnorderedMap(index.m_sfxEntries))
                {
                    SFXGroupIndex::SFXEntryDNA<DNAE> entry = p.second.get().toDNA<DNAE>(p.first);
                    entry.write(fo);
                }

                header.groupEndOff = fo.position();
                fo.seek(groupStart, athena::Begin);
                header.write(fo);
                fo.seek(header.groupEndOff, athena::Begin);
            }
        }
    }

    const uint32_t finalTerm = 0xffffffff;
    athena::io::Write<athena::io::PropType::None>::Do<decltype(finalTerm), DNAE>({}, finalTerm, fo);

    return fo.data();
}

}

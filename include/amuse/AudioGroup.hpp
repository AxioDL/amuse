#ifndef __AMUSE_AUDIOGROUP_HPP__
#define __AMUSE_AUDIOGROUP_HPP__

#include "AudioGroupPool.hpp"
#include "AudioGroupProject.hpp"
#include "AudioGroupSampleDirectory.hpp"

namespace amuse
{
class AudioGroupData;

/** Runtime audio group index container */
class AudioGroup
{
    AudioGroupProject m_proj;
    AudioGroupPool m_pool;
    AudioGroupSampleDirectory m_sdir;
    const unsigned char* m_samp = nullptr;
    SystemString m_groupPath; /* Typically only set by editor */
    bool m_valid;

    SystemString getSampleBasePath(SampleId sfxId) const;

public:
    operator bool() const { return m_valid; }
    AudioGroup() = default;
    explicit AudioGroup(const AudioGroupData& data) { assign(data); }
    explicit AudioGroup(SystemStringView groupPath) { assign(groupPath); }

    void assign(const AudioGroupData& data);
    void assign(SystemStringView groupPath);
    void setGroupPath(SystemStringView groupPath) { m_groupPath = groupPath; }

    const SampleEntry* getSample(SampleId sfxId) const;
    std::pair<ObjToken<SampleEntryData>, const unsigned char*>
        getSampleData(SampleId sfxId, const SampleEntry* sample) const;
    SampleFileState getSampleFileState(SampleId sfxId,
        const SampleEntry* sample, SystemString* pathOut = nullptr) const;
    void patchSampleMetadata(SampleId sfxId, const SampleEntry* sample) const;
    void makeWAVVersion(SampleId sfxId, const SampleEntry* sample) const;
    void makeCompressedVersion(SampleId sfxId, const SampleEntry* sample) const;
    const AudioGroupProject& getProj() const { return m_proj; }
    const AudioGroupPool& getPool() const { return m_pool; }
    const AudioGroupSampleDirectory& getSdir() const { return m_sdir; }
    AudioGroupProject& getProj() { return m_proj; }
    AudioGroupPool& getPool() { return m_pool; }
    AudioGroupSampleDirectory& getSdir() { return m_sdir; }

    virtual void setIdDatabases() const {}
};

class AudioGroupDatabase final : public AudioGroup
{
    NameDB m_soundMacroDb;
    NameDB m_sampleDb;
    NameDB m_tableDb;
    NameDB m_keymapDb;
    NameDB m_layersDb;

public:
    AudioGroupDatabase() = default;
    explicit AudioGroupDatabase(const AudioGroupData& data)
    {
        setIdDatabases();
        assign(data);
    }
    explicit AudioGroupDatabase(SystemStringView groupPath)
    {
        setIdDatabases();
        assign(groupPath);
    }

    void setIdDatabases() const
    {
        SoundMacroId::CurNameDB = const_cast<NameDB*>(&m_soundMacroDb);
        SampleId::CurNameDB = const_cast<NameDB*>(&m_sampleDb);
        TableId::CurNameDB = const_cast<NameDB*>(&m_tableDb);
        KeymapId::CurNameDB = const_cast<NameDB*>(&m_keymapDb);
        LayersId::CurNameDB = const_cast<NameDB*>(&m_layersDb);
    }
};

class ProjectDatabase
{
    NameDB m_songDb;
    NameDB m_sfxDb;
    NameDB m_groupDb;

public:
    void setIdDatabases() const
    {
        SongId::CurNameDB = const_cast<NameDB*>(&m_songDb);
        SFXId::CurNameDB = const_cast<NameDB*>(&m_sfxDb);
        GroupId::CurNameDB = const_cast<NameDB*>(&m_groupDb);
    }
};
}

#endif // __AMUSE_AUDIOGROUP_HPP__

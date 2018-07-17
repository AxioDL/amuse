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
    SystemString m_groupPath;
    bool m_valid;

public:
    operator bool() const { return m_valid; }
    explicit AudioGroup(const AudioGroupData& data);
    explicit AudioGroup(SystemStringView groupPath);

    const AudioGroupSampleDirectory::Entry* getSample(SampleId sfxId) const;
    const unsigned char* getSampleData(SampleId sfxId, const AudioGroupSampleDirectory::Entry* sample) const;
    const AudioGroupProject& getProj() const { return m_proj; }
    const AudioGroupPool& getPool() const { return m_pool; }
    const AudioGroupSampleDirectory& getSdir() const { return m_sdir; }
    AudioGroupProject& getProj() { return m_proj; }
    AudioGroupPool& getPool() { return m_pool; }
    AudioGroupSampleDirectory& getSdir() { return m_sdir; }
};

class AudioGroupDatabase : public AudioGroup
{
    amuse::NameDB m_soundMacroDb;
    amuse::NameDB m_sampleDb;
    amuse::NameDB m_tableDb;
    amuse::NameDB m_keymapDb;
    amuse::NameDB m_layersDb;

public:
    explicit AudioGroupDatabase(const AudioGroupData& data)
    : AudioGroup(data) {}
    explicit AudioGroupDatabase(SystemStringView groupPath)
    : AudioGroup(groupPath) {}

    void setIdDatabases()
    {
        amuse::SoundMacroId::CurNameDB = &m_soundMacroDb;
        amuse::SampleId::CurNameDB = &m_sampleDb;
        amuse::TableId::CurNameDB = &m_tableDb;
        amuse::KeymapId::CurNameDB = &m_keymapDb;
        amuse::LayersId::CurNameDB = &m_layersDb;
    }
};

class ProjectDatabase
{
    amuse::NameDB m_songDb;
    amuse::NameDB m_sfxDb;
    amuse::NameDB m_groupDb;

public:
    void setIdDatabases()
    {
        amuse::SongId::CurNameDB = &m_songDb;
        amuse::SFXId::CurNameDB = &m_sfxDb;
        amuse::GroupId::CurNameDB = &m_groupDb;
    }
};
}

#endif // __AMUSE_AUDIOGROUP_HPP__

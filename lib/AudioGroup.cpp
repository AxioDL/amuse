#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"

namespace amuse
{

void AudioGroup::assign(const AudioGroupData& data)
{
    m_proj = AudioGroupProject::CreateAudioGroupProject(data);
    m_pool = AudioGroupPool::CreateAudioGroupPool(data);
    m_sdir = AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(data);
    m_samp = data.getSamp();
}
void AudioGroup::assign(SystemStringView groupPath)
{
    /* Reverse order when loading intermediates */
    m_groupPath = groupPath;
    m_sdir = AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(groupPath);
    m_pool = AudioGroupPool::CreateAudioGroupPool(groupPath);
    m_proj = AudioGroupProject::CreateAudioGroupProject(groupPath);
    m_samp = nullptr;
}

const SampleEntry* AudioGroup::getSample(SampleId sfxId) const
{
    auto search = m_sdir.m_entries.find(sfxId);
    if (search == m_sdir.m_entries.cend())
        return nullptr;
    return search->second.get();
}

SystemString AudioGroup::getSampleBasePath(SampleId sfxId) const
{
#if _WIN32
    return m_groupPath + _S('/') +
    athena::utility::utf8ToWide(SampleId::CurNameDB->resolveNameFromId(sfxId));
#else
    return m_groupPath + _S('/') +
    SampleId::CurNameDB->resolveNameFromId(sfxId).data();
#endif
}

std::pair<ObjToken<SampleEntryData>, const unsigned char*>
    AudioGroup::getSampleData(SampleId sfxId, const SampleEntry* sample) const
{
    if (sample->m_data->m_looseData)
    {
        setIdDatabases();
        SystemString basePath = getSampleBasePath(sfxId);
        const_cast<SampleEntry*>(sample)->loadLooseData(basePath);
        return {sample->m_data, sample->m_data->m_looseData.get()};
    }
    return {{}, m_samp + sample->m_data->m_sampleOff};
}

SampleFileState AudioGroup::getSampleFileState(SampleId sfxId, const SampleEntry* sample, SystemString* pathOut) const
{
    if (sample->m_data->m_looseData)
    {
        setIdDatabases();
        SystemString basePath = getSampleBasePath(sfxId);
        return sample->getFileState(basePath, pathOut);
    }
    if (sample->m_data->isFormatDSP() || sample->m_data->getSampleFormat() == SampleFormat::N64)
        return SampleFileState::MemoryOnlyCompressed;
    return SampleFileState::MemoryOnlyWAV;
}

void AudioGroup::patchSampleMetadata(SampleId sfxId, const SampleEntry* sample) const
{
    if (sample->m_data->m_looseData)
    {
        setIdDatabases();
        SystemString basePath = getSampleBasePath(sfxId);
        sample->patchSampleMetadata(basePath);
    }
}

void AudioGroup::makeWAVVersion(SampleId sfxId, const SampleEntry* sample) const
{
    if (sample->m_data->m_looseData)
    {
        setIdDatabases();
        m_sdir._extractWAV(sfxId, *sample->m_data, m_groupPath, sample->m_data->m_looseData.get());
    }
}

void AudioGroup::makeCompressedVersion(SampleId sfxId, const SampleEntry* sample) const
{
    if (sample->m_data->m_looseData)
    {
        setIdDatabases();
        m_sdir._extractCompressed(sfxId, *sample->m_data, m_groupPath, sample->m_data->m_looseData.get(), true);
    }
}
}

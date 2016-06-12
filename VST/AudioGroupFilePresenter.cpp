#include "AudioGroupFilePresenter.hpp"
#include "VSTBackend.hpp"

namespace amuse
{

bool AudioGroupDataCollection::loadProj()
{
    std::wstring path = m_path + L"\\proj";
    athena::io::FileReader r(path, 1024 * 32, false);
    if (r.hasError())
        return false;
    std::vector<uint8_t>& ret = m_projData;
    size_t len = r.length();
    ret.resize(len);
    r.readUBytesToBuf(ret.data(), len);
    return ret.size() != 0;
}

bool AudioGroupDataCollection::loadPool()
{
    std::wstring path = m_path + L"\\pool";
    athena::io::FileReader r(path, 1024 * 32, false);
    if (r.hasError())
        return false;
    std::vector<uint8_t>& ret = m_poolData;
    size_t len = r.length();
    ret.resize(len);
    r.readUBytesToBuf(ret.data(), len);
    return ret.size() != 0;
}

bool AudioGroupDataCollection::loadSdir()
{
    std::wstring path = m_path + L"\\sdir";
    athena::io::FileReader r(path, 1024 * 32, false);
    if (r.hasError())
        return false;
    std::vector<uint8_t>& ret = m_sdirData;
    size_t len = r.length();
    ret.resize(len);
    r.readUBytesToBuf(ret.data(), len);
    return ret.size() != 0;
}

bool AudioGroupDataCollection::loadSamp()
{
    std::wstring path = m_path + L"\\samp";
    athena::io::FileReader r(path, 1024 * 32, false);
    if (r.hasError())
        return false;
    std::vector<uint8_t>& ret = m_sampData;
    size_t len = r.length();
    ret.resize(len);
    r.readUBytesToBuf(ret.data(), len);
    return ret.size() != 0;
}

bool AudioGroupDataCollection::loadMeta()
{
    std::wstring path = m_path + L"\\meta";
    athena::io::FileReader r(path, 1024 * 32, false);
    if (r.hasError())
        return false;
    std::experimental::optional<MetaData>& ret = m_metaData;
    ret.emplace(r);
    return ret.operator bool();
}

AudioGroupDataCollection::AudioGroupDataCollection(const std::wstring& path, const std::wstring& name)
: m_path(path), m_name(name)
{

}

bool AudioGroupDataCollection::_attemptLoad(AudioGroupFilePresenter& presenter)
{
    if (m_metaData && m_loadedData && m_loadedGroup)
        return true;
    if (!loadProj())
        return false;
    if (!loadPool())
        return false;
    if (!loadSdir())
        return false;
    if (!loadSamp())
        return false;
    if (!loadMeta())
        return false;

    return _indexData(presenter);
}

bool AudioGroupDataCollection::_indexData(AudioGroupFilePresenter& presenter)
{
    amuse::Engine& engine = presenter.getBackend().getAmuseEngine();

    switch (m_metaData->fmt)
    {
        case amuse::DataFormat::GCN:
        default:
            m_loadedData.emplace(m_projData.data(), m_projData.size(),
                                 m_poolData.data(), m_poolData.size(),
                                 m_sdirData.data(), m_sdirData.size(),
                                 m_sampData.data(), m_sampData.size(),
                                 amuse::GCNDataTag{});
            break;
        case amuse::DataFormat::N64:
            m_loadedData.emplace(m_projData.data(), m_projData.size(),
                                 m_poolData.data(), m_poolData.size(),
                                 m_sdirData.data(), m_sdirData.size(),
                                 m_sampData.data(), m_sampData.size(),
                                 m_metaData->absOffs, amuse::N64DataTag{});
            break;
        case amuse::DataFormat::PC:
            m_loadedData.emplace(m_projData.data(), m_projData.size(),
                                 m_poolData.data(), m_poolData.size(),
                                 m_sdirData.data(), m_sdirData.size(),
                                 m_sampData.data(), m_sampData.size(),
                                 m_metaData->absOffs, amuse::PCDataTag{});
            break;
    }

    m_loadedGroup = engine.addAudioGroup(*m_loadedData);
    m_groupTokens.clear();
    if (m_loadedGroup)
    {
        m_groupTokens.reserve(m_loadedGroup->getProj().songGroups().size() +
                              m_loadedGroup->getProj().sfxGroups().size());

        {
            const auto& songGroups = m_loadedGroup->getProj().songGroups();
            std::map<int, const amuse::SongGroupIndex*> sortGroups;
            for (const auto& pair : songGroups)
                sortGroups[pair.first] = &pair.second;
            for (const auto& pair : sortGroups)
                m_groupTokens.emplace_back(pair.first, pair.second);
        }
        {
            const auto& sfxGroups = m_loadedGroup->getProj().sfxGroups();
            std::map<int, const amuse::SFXGroupIndex*> sortGroups;
            for (const auto& pair : sfxGroups)
                sortGroups[pair.first] = &pair.second;
            for (const auto& pair : sortGroups)
                m_groupTokens.emplace_back(pair.first, pair.second);
        }
    }

    return m_loadedData && m_loadedGroup;
}

AudioGroupCollection::AudioGroupCollection(const std::wstring& path, const std::wstring& name)
: m_path(path), m_name(name)
{

}

void AudioGroupCollection::addCollection(AudioGroupFilePresenter& presenter,
                                         std::vector<std::pair<std::wstring, amuse::IntrusiveAudioGroupData>>&& collection)
{
    for (std::pair<std::wstring, amuse::IntrusiveAudioGroupData>& pair : collection)
    {
        std::wstring collectionPath = m_path + L'\\' + pair.first;

        amuse::IntrusiveAudioGroupData& dataIn = pair.second;
        auto search = m_groups.find(pair.first);
        if (search == m_groups.end())
        {
            search = m_groups.emplace(pair.first,
                                      std::make_unique<AudioGroupDataCollection>(collectionPath,
                                                                                 pair.first)).first;
        }

        AudioGroupDataCollection& dataCollection = *search->second;
        dataCollection.m_projData.resize(dataIn.getProjSize());
        memmove(dataCollection.m_projData.data(), dataIn.getProj(), dataIn.getProjSize());

        dataCollection.m_poolData.resize(dataIn.getPoolSize());
        memmove(dataCollection.m_poolData.data(), dataIn.getPool(), dataIn.getPoolSize());

        dataCollection.m_sdirData.resize(dataIn.getSdirSize());
        memmove(dataCollection.m_sdirData.data(), dataIn.getSdir(), dataIn.getSdirSize());

        dataCollection.m_sampData.resize(dataIn.getSampSize());
        memmove(dataCollection.m_sampData.data(), dataIn.getSamp(), dataIn.getSampSize());

        dataCollection.m_metaData.emplace(dataIn.getDataFormat(), dataIn.getAbsoluteProjOffsets(), true);
        dataCollection._indexData(presenter);
    }
}

void AudioGroupCollection::update(AudioGroupFilePresenter& presenter)
{
    std::wstring path = m_path + L"\\*";

    WIN32_FIND_DATAW d;
    HANDLE dir = FindFirstFileW(path.c_str(), &d);
    if (dir == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (!wcscmp(d.cFileName, L".") || !wcscmp(d.cFileName, L".."))
            continue;

        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            std::wstring nameStr(d.cFileName);
            auto search = m_groups.find(nameStr);
            if (search == m_groups.end())
            {
                search =
                m_groups.emplace(nameStr,
                                 std::make_unique<AudioGroupDataCollection>(m_path + L'\\' + nameStr,
                                                                            nameStr)).first;
                search->second->_attemptLoad(presenter);
            }
        }
    } while (FindNextFileW(dir, &d));

    FindClose(dir);
}

void AudioGroupFilePresenter::update()
{
    std::wstring path = m_backend.getUserDir() + L"\\*";
    std::map<std::wstring, std::unique_ptr<AudioGroupCollection>>& theMap = m_audioGroupCollections;

    WIN32_FIND_DATAW d;
    HANDLE dir = FindFirstFileW(path.c_str(), &d);
    if (dir == INVALID_HANDLE_VALUE)
        return;
    do
    {
        if (!wcscmp(d.cFileName, L".") || !wcscmp(d.cFileName, L".."))
            continue;

        if (d.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            std::wstring nameStr(d.cFileName);
            auto search = theMap.find(nameStr);
            if (search == theMap.end())
            {
                search = theMap.emplace(nameStr,
                    std::make_unique<AudioGroupCollection>(m_backend.getUserDir() + L'\\' + nameStr, nameStr)).first;
                search->second->update(*this);
            }
        }
    } while (FindNextFileW(dir, &d));

    FindClose(dir);
}

void AudioGroupFilePresenter::addCollection(const std::wstring& name,
     std::vector<std::pair<std::wstring, amuse::IntrusiveAudioGroupData>>&& collection)
{
    std::wstring path = m_backend.getUserDir() + L'\\' + name;
    AudioGroupCollection& insert = *m_audioGroupCollections.emplace(name, std::make_unique<AudioGroupCollection>(path, name)).first->second;
    insert.addCollection(*this, std::move(collection));

    for (std::pair<const std::wstring, std::unique_ptr<AudioGroupDataCollection>>& pair : insert.m_groups)
    {
        std::wstring collectionPath = insert.m_path + L'\\' + pair.first;
        CreateDirectory(collectionPath.c_str(), nullptr);

        FILE* fp = _wfopen((collectionPath + L"\\proj").c_str(), L"wb");
        if (fp)
        {
            fwrite(pair.second->m_projData.data(), 1, pair.second->m_projData.size(), fp);
            fclose(fp);
        }

        fp = _wfopen((collectionPath + L"\\pool").c_str(), L"wb");
        if (fp)
        {
            fwrite(pair.second->m_poolData.data(), 1, pair.second->m_poolData.size(), fp);
            fclose(fp);
        }

        fp = _wfopen((collectionPath + L"\\sdir").c_str(), L"wb");
        if (fp)
        {
            fwrite(pair.second->m_sdirData.data(), 1, pair.second->m_sdirData.size(), fp);
            fclose(fp);
        }

        fp = _wfopen((collectionPath + L"\\samp").c_str(), L"wb");
        if (fp)
        {
            fwrite(pair.second->m_sampData.data(), 1, pair.second->m_sampData.size(), fp);
            fclose(fp);
        }

        fp = _wfopen((collectionPath + L"\\meta").c_str(), L"wb");
        if (fp)
        {
            fwrite(&*pair.second->m_metaData, 1, sizeof(*pair.second->m_metaData), fp);
            fclose(fp);
        }
    }
}

void AudioGroupFilePresenter::populateEditor(VSTEditor& editor)
{
    for (const auto& cgollection : m_audioGroupCollections)
    {

    }
}

}

#include "AudioGroupFilePresenter.hpp"
#include "VSTBackend.hpp"
#include <Shellapi.h>
#include <Shlwapi.h>

namespace amuse
{

static const wchar_t *const GMNames[128] =
{
    L"Acoustic Grand Piano", L"Bright Acoustic Piano", L"Electric Grand Piano", L"Honky-tonk Piano", L"Rhodes Piano", L"Chorused Piano",
    L"Harpsichord", L"Clavinet", L"Celesta", L"Glockenspiel", L"Music Box", L"Vibraphone", L"Marimba", L"Xylophone", L"Tubular Bells", L"Dulcimer",
    L"Drawbar Organ", L"Percussive Organ", L"Rock Organ", L"Church Organ", L"Reed Organ", L"Accordion", L"Harmonica", L"Tango Accordion",
    L"Acoustic Guitar (nylon)", L"Acoustic Guitar (steel)", L"Electric Guitar (jazz)", L"Electric Guitar (clean)", L"Electric Guitar (muted)",
    L"Overdriven Guitar", L"Distortion Guitar", L"Guitar Harmonics", L"Acoustic Bass", L"Electric Bass (finger)", L"Electric Bass (pick)",
    L"Fretless Bass", L"Slap Bass 1", L"Slap Bass 2", L"Synth Bass 1", L"Synth Bass 2", L"Violin", L"Viola", L"Cello", L"Contrabass",
    L"Tremelo Strings", L"Pizzicato Strings", L"Orchestral Harp", L"Timpani", L"String Ensemble 1", L"String Ensemble 2", L"SynthStrings 1",
    L"SynthStrings 2", L"Choir Aahs", L"Voice Oohs", L"Synth Voice", L"Orchestra Hit", L"Trumpet", L"Trombone", L"Tuba", L"Muted Trumpet",
    L"French Horn", L"Brass Section", L"Synth Brass 1", L"Synth Brass 2", L"Soprano Sax", L"Alto Sax", L"Tenor Sax", L"Baritone Sax",
    L"Oboe", L"English Horn", L"Bassoon", L"Clarinet", L"Piccolo", L"Flute", L"Recorder", L"Pan Flute", L"Bottle Blow", L"Shakuhachi", L"Whistle",
    L"Ocarina", L"Lead 1 (square)", L"Lead 2 (sawtooth)", L"Lead 3 (calliope lead)", L"Lead 4 (chiff lead)", L"Lead 5 (charang)",
    L"Lead 6 (voice)", L"Lead 7 (fifths)", L"Lead 8 (bass + lead)", L"Pad 1 (new age)", L"Pad 2 (warm)", L"Pad 3 (polysynth)", L"Pad 4 (choir)",
    L"Pad 5 (bowed)", L"Pad 6 (metallic)", L"Pad 7 (halo)", L"Pad 8 (sweep)", L"FX 1 (rain)", L"FX 2 (soundtrack)", L"FX 3 (crystal)",
    L"FX 4 (atmosphere)", L"FX 5 (brightness)", L"FX 6 (goblins)", L"FX 7 (echoes)", L"FX 8 (sci-fi)", L"Sitar", L"Banjo", L"Shamisen", L"Koto",
    L"Kalimba", L"Bagpipe", L"Fiddle", L"Shanai", L"Tinkle Bell", L"Agogo", L"Steel Drums", L"Woodblock", L"Taiko Drum", L"Melodic Tom",
    L"Synth Drum", L"Reverse Cymbal", L"Guitar Fret Noise", L"Breath Noise", L"Seashore", L"Bird Tweet", L"Telephone Ring", L"Helicopter",
    L"Applause", L"Gunshot"
};

static const wchar_t *const GMPercNames[128] =
{
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, L"Acoustic Bass Drum", L"Bass Drum 1", L"Side Stick",
    L"Acoustic Snare", L"Hand Clap", L"Electric Snare", L"Low Floor Tom", L"Closed Hi-Hat",
    L"High Floor Tom", L"Pedal Hi-Hat", L"Low Tom", L"Open Hi-Hat", L"Low-Mid Tom", L"Hi-Mid Tom",
    L"Crash Cymbal 1", L"High Tom", L"Ride Cymbal 1", L"Chinese Cymbal", L"Ride Bell", L"Tambourine",
    L"Splash Cymbal", L"Cowbell", L"Crash Cymbal 2", L"Vibraslap", L"Ride Cymbal 2", L"Hi Bongo",
    L"Low Bongo", L"Mute Hi Conga", L"Open Hi Conga", L"Low Conga", L"High Timbale", L"Low Timbale",
    L"High Agogo", L"Low Agogo", L"Cabasa", L"Maracas", L"Short Whistle", L"Long Whistle", L"Short Guiro",
    L"Long Guiro", L"Claves", L"Hi Wood Block", L"Low Wood Block", L"Mute Cuica", L"Open Cuica",
    L"Mute Triangle", L"Open Triangle"
};

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

bool AudioGroupDataCollection::_attemptLoad()
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

    return _indexData();
}

bool AudioGroupDataCollection::_indexData()
{
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

    return m_loadedData.operator bool();
}

void AudioGroupDataCollection::addToEngine(amuse::Engine& engine)
{
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
}

void AudioGroupDataCollection::removeFromEngine(amuse::Engine& engine) const
{
    engine.removeAudioGroup(*m_loadedData);
}

AudioGroupCollection::AudioGroupCollection(const std::wstring& path, const std::wstring& name)
: m_path(path), m_name(name)
{

}

void AudioGroupCollection::addCollection(std::vector<std::pair<std::wstring, amuse::IntrusiveAudioGroupData>>&& collection)
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
        dataCollection._indexData();
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
                search->second->_attemptLoad();
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
    CreateDirectory(insert.m_path.c_str(), nullptr);
    insert.addCollection(std::move(collection));

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

void AudioGroupFilePresenter::removeCollection(unsigned idx)
{
    if (idx < m_iteratorVec.size())
    {
        CollectionIterator& it = m_iteratorVec[idx];
        std::wstring collectionPath = it->second->m_path + L'\0';
        SHFILEOPSTRUCT op = {};
        op.wFunc = FO_DELETE;
        op.pFrom = collectionPath.c_str();
        op.fFlags = FOF_NO_UI;
        SHFileOperation(&op);
        m_audioGroupCollections.erase(it);
    }
}

void AudioGroupCollection::populateFiles(VSTEditor& editor, HTREEITEM colHandle, size_t parentIdx)
{
    TVINSERTSTRUCT ins = {};
    ins.item.mask = TVIF_TEXT | TVIF_PARAM;
    ins.hParent = colHandle;
    ins.hInsertAfter = TVI_LAST;

    m_iteratorVec.clear();
    m_iteratorVec.reserve(m_groups.size());
    for (auto it = m_groups.begin() ; it != m_groups.end() ; ++it)
    {
        ins.item.pszText = LPWSTR(it->first.c_str());
        ins.item.lParam = LPARAM(0x80000000 | (parentIdx << 16) | m_iteratorVec.size());
        HTREEITEM item = TreeView_InsertItem(editor.m_collectionTree, &ins);
        if (editor.m_selCollectionIdx == parentIdx && editor.m_selFileIdx == m_iteratorVec.size())
            editor.m_deferredCollectionSel = item;
        m_iteratorVec.push_back(it);
    }
}

void AudioGroupFilePresenter::populateCollectionColumn(VSTEditor& editor)
{
    TreeView_DeleteAllItems(editor.m_collectionTree);
    TVINSERTSTRUCT ins = {};
    ins.hParent = TVI_ROOT;
    ins.hInsertAfter = TVI_LAST;
    ins.item.mask = TVIF_CHILDREN | TVIF_TEXT | TVIF_PARAM;

    m_iteratorVec.clear();
    m_iteratorVec.reserve(m_audioGroupCollections.size());
    for (auto it = m_audioGroupCollections.begin() ; it != m_audioGroupCollections.end() ; ++it)
    {
        ins.item.cChildren = it->second->m_groups.size() ? 1 : 0;
        ins.item.pszText = LPWSTR(it->first.c_str());
        ins.item.lParam = LPARAM(m_iteratorVec.size() << 16);
        HTREEITEM item = TreeView_InsertItem(editor.m_collectionTree, &ins);
        it->second->populateFiles(editor, item, m_iteratorVec.size());
        if (editor.m_selCollectionIdx == m_iteratorVec.size() && editor.m_selFileIdx == -1)
            editor.m_deferredCollectionSel = item;
        m_iteratorVec.push_back(it);
    }
}

void AudioGroupFilePresenter::populateGroupColumn(VSTEditor& editor, int collectionIdx, int fileIdx)
{
    LVITEM item = {};
    item.mask = LVIF_TEXT;

    ListView_DeleteAllItems(editor.m_groupListView);
    if (collectionIdx < m_iteratorVec.size())
    {
        CollectionIterator& it = m_iteratorVec[collectionIdx];
        if (fileIdx < it->second->m_iteratorVec.size())
        {
            size_t idx = 0;
            AudioGroupCollection::GroupIterator& git = it->second->m_iteratorVec[fileIdx];
            for (AudioGroupDataCollection::GroupToken& gtok : git->second->m_groupTokens)
            {
                wchar_t name[256];
                wnsprintf(name, 256, L"%d (%s)", gtok.m_groupId, gtok.m_song ? L"Song" : L"SFX");
                item.pszText = name;
                item.iItem = idx++;
                ListView_InsertItem(editor.m_groupListView, &item);
            }
        }
    }
}

void AudioGroupFilePresenter::populatePageColumn(VSTEditor& editor, int collectionIdx, int fileIdx, int groupIdx)
{
    LVITEM item = {};
    item.mask = LVIF_TEXT | LVIF_PARAM;

    ListView_DeleteAllItems(editor.m_pageListView);
    if (collectionIdx < m_iteratorVec.size())
    {
        CollectionIterator& it = m_iteratorVec[collectionIdx];
        if (fileIdx < it->second->m_iteratorVec.size())
        {
            AudioGroupCollection::GroupIterator& git = it->second->m_iteratorVec[fileIdx];
            if (groupIdx < git->second->m_groupTokens.size())
            {
                AudioGroupDataCollection::GroupToken& groupTok = git->second->m_groupTokens[groupIdx];
                if (groupTok.m_song)
                {
                    std::map<uint8_t, const amuse::SongGroupIndex::PageEntry*> sortPages;
                    for (auto& pair : groupTok.m_song->m_normPages)
                        sortPages[pair.first] = pair.second;

                    size_t idx = 0;
                    for (auto& pair : sortPages)
                    {
                        wchar_t name[256];
                        wnsprintf(name, 256, L"%d (%s)", pair.first, GMNames[pair.first] ? GMNames[pair.first] : L"???");
                        item.pszText = name;
                        item.iItem = idx++;
                        item.lParam = pair.first;
                        ListView_InsertItem(editor.m_pageListView, &item);
                    }

                    sortPages.clear();
                    for (auto& pair : groupTok.m_song->m_drumPages)
                        sortPages[pair.first] = pair.second;

                    for (auto& pair : sortPages)
                    {
                        wchar_t name[256];
                        wnsprintf(name, 256, L"%d (%s)", pair.first, GMPercNames[pair.first] ? GMPercNames[pair.first] : L"???");
                        item.pszText = name;
                        item.iItem = idx++;
                        item.lParam = 0x80000000 | pair.first;
                        ListView_InsertItem(editor.m_pageListView, &item);
                    }
                }
            }
        }
    }
}

}

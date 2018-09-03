#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"
#include <regex>
#include <athena/FileReader.hpp>

using namespace std::literals;

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
void AudioGroup::assign(const AudioGroup& data, SystemStringView groupPath)
{
    /* Reverse order when loading intermediates */
    m_groupPath = groupPath;
    m_sdir = AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(groupPath);
    m_pool = AudioGroupPool::CreateAudioGroupPool(groupPath);
    m_proj = AudioGroupProject::CreateAudioGroupProject(data.getProj());
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
    return std::make_pair(ObjToken<SampleEntryData>(), m_samp + sample->m_data->m_sampleOff);
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

void AudioGroupDatabase::renameSample(SampleId id, std::string_view str)
{
    SystemString oldBasePath = getSampleBasePath(id);
    SampleId::CurNameDB->rename(id, str);
    SystemString newBasePath = getSampleBasePath(id);
    Rename((oldBasePath + _S(".wav")).c_str(), (newBasePath + _S(".wav")).c_str());
    Rename((oldBasePath + _S(".dsp")).c_str(), (newBasePath + _S(".dsp")).c_str());
    Rename((oldBasePath + _S(".vadpcm")).c_str(), (newBasePath + _S(".vadpcm")).c_str());
}

void AudioGroupDatabase::deleteSample(SampleId id)
{
    SystemString basePath = getSampleBasePath(id);
    Unlink((basePath + _S(".wav")).c_str());
    Unlink((basePath + _S(".dsp")).c_str());
    Unlink((basePath + _S(".vadpcm")).c_str());
}

void AudioGroupDatabase::copySampleInto(const SystemString& basePath, const SystemString& newBasePath)
{
    Copy((basePath + _S(".wav")).c_str(), (newBasePath + _S(".wav")).c_str());
    Copy((basePath + _S(".dsp")).c_str(), (newBasePath + _S(".dsp")).c_str());
    Copy((basePath + _S(".vadpcm")).c_str(), (newBasePath + _S(".vadpcm")).c_str());
}

void AudioGroupDatabase::_recursiveRenameMacro(SoundMacroId id, std::string_view str, int& macroIdx,
                                               std::unordered_set<SoundMacroId>& renamedIds)
{
    if (renamedIds.find(id) != renamedIds.cend())
        return;
    if (const SoundMacro* macro = getPool().soundMacro(id))
    {
        if (!strncmp(SoundMacroId::CurNameDB->resolveNameFromId(id).data(), "macro", 5))
        {
            std::string macroName("macro"sv);
            if (macroIdx)
            {
                char num[16];
                snprintf(num, 16, "%d", macroIdx);
                macroName += num;
            }
            macroName += '_';
            macroName += str;
            ++macroIdx;
            SoundMacroId::CurNameDB->rename(id, macroName);
            renamedIds.insert(id);
            int sampleIdx = 0;
            for (const auto& cmd : macro->m_cmds)
            {
                switch (cmd->Isa())
                {
                case SoundMacro::CmdOp::StartSample:
                {
                    SoundMacro::CmdStartSample* ss = static_cast<SoundMacro::CmdStartSample*>(cmd.get());
                    if (!strncmp(SampleId::CurNameDB->resolveNameFromId(ss->sample.id).data(), "sample", 6))
                    {
                        std::string sampleName("sample"sv);
                        if (sampleIdx)
                        {
                            char num[16];
                            snprintf(num, 16, "%d", sampleIdx);
                            sampleName += num;
                        }
                        sampleName += '_';
                        sampleName += macroName;
                        ++sampleIdx;
                        renameSample(ss->sample.id, sampleName);
                    }
                    break;
                }
                case SoundMacro::CmdOp::SplitKey:
                    _recursiveRenameMacro(static_cast<SoundMacro::CmdSplitKey*>(cmd.get())->macro, str, macroIdx, renamedIds);
                    break;
                case SoundMacro::CmdOp::SplitVel:
                    _recursiveRenameMacro(static_cast<SoundMacro::CmdSplitVel*>(cmd.get())->macro, str, macroIdx, renamedIds);
                    break;
                case SoundMacro::CmdOp::Goto:
                    _recursiveRenameMacro(static_cast<SoundMacro::CmdGoto*>(cmd.get())->macro, str, macroIdx, renamedIds);
                    break;
                case SoundMacro::CmdOp::PlayMacro:
                    _recursiveRenameMacro(static_cast<SoundMacro::CmdPlayMacro*>(cmd.get())->macro, str, macroIdx, renamedIds);
                    break;
                case SoundMacro::CmdOp::SplitMod:
                    _recursiveRenameMacro(static_cast<SoundMacro::CmdSplitMod*>(cmd.get())->macro, str, macroIdx, renamedIds);
                    break;
                case SoundMacro::CmdOp::SplitRnd:
                    _recursiveRenameMacro(static_cast<SoundMacro::CmdSplitRnd*>(cmd.get())->macro, str, macroIdx, renamedIds);
                    break;
                case SoundMacro::CmdOp::GoSub:
                    _recursiveRenameMacro(static_cast<SoundMacro::CmdGoSub*>(cmd.get())->macro, str, macroIdx, renamedIds);
                    break;
                case SoundMacro::CmdOp::TrapEvent:
                    _recursiveRenameMacro(static_cast<SoundMacro::CmdTrapEvent*>(cmd.get())->macro, str, macroIdx, renamedIds);
                    break;
                case SoundMacro::CmdOp::SendMessage:
                    _recursiveRenameMacro(static_cast<SoundMacro::CmdSendMessage*>(cmd.get())->macro, str, macroIdx, renamedIds);
                    break;
                default:
                    break;
                }
            }
        }
    }
}

static const std::regex DefineGRPEntry(R"(#define\s+GRP(\S+)\s+(\S+))",
                                       std::regex::ECMAScript|std::regex::optimize);
static const std::regex DefineSNGEntry(R"(#define\s+SNG(\S+)\s+(\S+))",
                                       std::regex::ECMAScript|std::regex::optimize);
static const std::regex DefineSFXEntry(R"(#define\s+SFX(\S+)\s+(\S+))",
                                       std::regex::ECMAScript|std::regex::optimize);

void AudioGroupDatabase::importCHeader(std::string_view header)
{
    setIdDatabases();
    std::match_results<std::string_view::const_iterator> dirMatch;
    auto begin = header.cbegin();
    auto end = header.cend();
    while (std::regex_search(begin, end, dirMatch, DefineGRPEntry))
    {
        std::string key = dirMatch[1].str();
        std::string value = dirMatch[2].str();
        char* endPtr;
        amuse::ObjectId id;
        id.id = strtoul(value.c_str(), &endPtr, 0);
        if (endPtr == value.c_str())
            continue;
        GroupId::CurNameDB->rename(id, key);
        begin = dirMatch.suffix().first;
    }
    begin = header.cbegin();
    end = header.cend();
    while (std::regex_search(begin, end, dirMatch, DefineSNGEntry))
    {
        std::string key = dirMatch[1].str();
        std::string value = dirMatch[2].str();
        char* endPtr;
        amuse::ObjectId id;
        id.id = strtoul(value.c_str(), &endPtr, 0);
        if (endPtr == value.c_str())
            continue;
        SongId::CurNameDB->rename(id, key);
        begin = dirMatch.suffix().first;
    }
    begin = header.cbegin();
    end = header.cend();
    std::unordered_set<SoundMacroId> renamedMacroIDs;
    while (std::regex_search(begin, end, dirMatch, DefineSFXEntry))
    {
        std::string key = dirMatch[1].str();
        std::string value = dirMatch[2].str();
        char* endPtr;
        amuse::ObjectId id;
        id.id = strtoul(value.c_str(), &endPtr, 0);
        if (endPtr == value.c_str())
            continue;
        SFXId::CurNameDB->rename(id, key);
        int macroIdx = 0;
        for (auto& sfxGrp : getProj().sfxGroups())
        {
            for (auto& sfx : sfxGrp.second->m_sfxEntries)
            {
                if (sfx.first == id)
                {
                    ObjectId sfxObjId = sfx.second.objId;
                    if (sfxObjId == ObjectId() || sfxObjId.id & 0xc000)
                        continue;
                    _recursiveRenameMacro(sfxObjId, key, macroIdx, renamedMacroIDs);
                }
            }
        }
        begin = dirMatch.suffix().first;
    }
}

static void WriteDefineLine(std::string& ret, std::string_view typeStr, std::string_view name, ObjectId id)
{
    ret += "#define "sv;
    ret += typeStr;
    ret += name;
    ret += ' ';
    char idStr[16];
    snprintf(idStr, 16, "%d", id.id);
    ret += idStr;
    ret += '\n';
}

std::string AudioGroupDatabase::exportCHeader(std::string_view projectName, std::string_view groupName) const
{
    setIdDatabases();
    std::string ret;
    ret += "/* Auto-generated Amuse Defines\n"
           " *\n"
           " * Project: "sv;
    ret += projectName;
    ret += "\n"
           " * Subproject: "sv;
    ret += groupName;
    ret += "\n"
           " * Date: "sv;
    time_t curTime = time(nullptr);
#ifndef _WIN32
    struct tm curTm;
    localtime_r(&curTime, &curTm);
    char curTmStr[26];
    asctime_r(&curTm, curTmStr);
#else
    struct tm curTm;
    localtime_s(&curTm, &curTime);
    char curTmStr[26];
    asctime_s(curTmStr, &curTm);
#endif
    if (char* ch = strchr(curTmStr, '\n'))
        *ch = '\0';
    ret += curTmStr;
    ret += "\n"
           " */\n\n\n"sv;

    bool addLF = false;
    for (const auto& sg : SortUnorderedMap(getProj().songGroups()))
    {
        auto name = amuse::GroupId::CurNameDB->resolveNameFromId(sg.first);
        WriteDefineLine(ret, "GRP"sv, name, sg.first);
        addLF = true;
    }
    for (const auto& sg : SortUnorderedMap(getProj().sfxGroups()))
    {
        auto name = amuse::GroupId::CurNameDB->resolveNameFromId(sg.first);
        WriteDefineLine(ret, "GRP"sv, name, sg.first);
        addLF = true;
    }

    if (addLF)
        ret += "\n\n"sv;
    addLF = false;

    std::unordered_set<amuse::SongId> songIds;
    for (const auto& sg : getProj().songGroups())
        for (const auto& song : sg.second->m_midiSetups)
            songIds.insert(song.first);
    for (amuse::SongId id : SortUnorderedSet(songIds))
    {
        auto name = amuse::SongId::CurNameDB->resolveNameFromId(id);
        WriteDefineLine(ret, "SNG"sv, name, id);
        addLF = true;
    }

    if (addLF)
        ret += "\n\n"sv;
    addLF = false;

    for (const auto& sg : SortUnorderedMap(getProj().sfxGroups()))
    {
        for (const auto& sfx : SortUnorderedMap(sg.second.get()->m_sfxEntries))
        {
            auto name = amuse::SFXId::CurNameDB->resolveNameFromId(sfx.first);
            WriteDefineLine(ret, "SFX"sv, name, sfx.first.id);
            addLF = true;
        }
    }

    if (addLF)
        ret += "\n\n"sv;

    return ret;
}
}

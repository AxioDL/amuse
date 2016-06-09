#include "amuse/ContainerRegistry.hpp"
#include "amuse/Common.hpp"
#include <stdio.h>
#include <string.h>
#include <string>
#include <memory>
#include <unordered_map>
#include <zlib.h>

#if _WIN32
static void *memmem(const void *haystack, size_t hlen, const void *needle, size_t nlen)
{
    int needle_first;
    const uint8_t *p = static_cast<const uint8_t*>(haystack);
    size_t plen = hlen;

    if (!nlen)
        return NULL;

    needle_first = *(unsigned char *)needle;

    while (plen >= nlen && (p = static_cast<const uint8_t*>(memchr(p, needle_first, plen - nlen + 1))))
    {
        if (!memcmp(p, needle, nlen))
            return (void *)p;

        p++;
        plen = hlen - (p - static_cast<const uint8_t*>(haystack));
    }

    return NULL;
}
#endif

namespace amuse
{

const char* ContainerRegistry::TypeToName(Type tp)
{
    switch (tp)
    {
    case Type::Invalid:
    default:
        return nullptr;
    case Type::Raw4:
        return "4 RAW Chunks";
    case Type::MetroidPrime:
        return "Metroid Prime (GCN)";
    case Type::MetroidPrime2:
        return "Metroid Prime 2 (GCN)";
    case Type::RogueSquadronPC:
        return "Star Wars - Rogue Squadron (PC)";
    case Type::RogueSquadronN64:
        return "Star Wars - Rogue Squadron (N64)";
    case Type::BattleForNabooPC:
        return "Star Wars Episode I - Battle for Naboo (PC)";
    case Type::BattleForNabooN64:
        return "Star Wars Episode I - Battle for Naboo (N64)";
    case Type::RogueSquadron2:
        return "Star Wars - Rogue Squadron 2 (GCN)";
    case Type::RogueSquadron3:
        return "Star Wars - Rogue Squadron 3 (GCN)";
    }
}

static size_t FileLength(FILE* fp)
{
    FSeek(fp, 0, SEEK_END);
    int64_t endPos = FTell(fp);
    fseek(fp, 0, SEEK_SET);
    return size_t(endPos);
}

static std::string ReadString(FILE* fp)
{
    char byte;
    std::string ret;
    while (fread(&byte, 1, 1, fp) == 1 && byte != 0)
        ret.push_back(byte);
    return ret;
}

static bool IsChunkExtension(const char* path, const char*& dotOut)
{
    const char* ext = strrchr(path, '.');
    if (ext)
    {
        if (!CompareCaseInsensitive(ext, ".poo") ||
            !CompareCaseInsensitive(ext, ".pool") ||
            !CompareCaseInsensitive(ext, ".pro") ||
            !CompareCaseInsensitive(ext, ".proj") ||
            !CompareCaseInsensitive(ext, ".sdi") ||
            !CompareCaseInsensitive(ext, ".sdir") ||
            !CompareCaseInsensitive(ext, ".sam") ||
            !CompareCaseInsensitive(ext, ".samp"))
        {
            dotOut = ext;
            return true;
        }
    }
    return false;
}

static bool IsSongExtension(const char* path, const char*& dotOut)
{
    const char* ext = strrchr(path, '.');
    if (ext)
    {
        if (!CompareCaseInsensitive(ext, ".son") ||
            !CompareCaseInsensitive(ext, ".sng") ||
            !CompareCaseInsensitive(ext, ".song"))
        {
            dotOut = ext;
            return true;
        }
    }
    return false;
}

static bool ValidateMP1(FILE* fp)
{
    if (FileLength(fp) > 40 * 1024 * 1024)
        return false;

    uint32_t magic;
    fread(&magic, 1, 4, fp);
    magic = SBig(magic);

    if (magic == 0x00030005)
    {
        FSeek(fp, 8, SEEK_SET);

        uint32_t nameCount;
        fread(&nameCount, 1, 4, fp);
        nameCount = SBig(nameCount);
        for (uint32_t i=0 ; i<nameCount ; ++i)
        {
            FSeek(fp, 8, SEEK_CUR);
            uint32_t nameLen;
            fread(&nameLen, 1, 4, fp);
            nameLen = SBig(nameLen);
            FSeek(fp, nameLen, SEEK_CUR);
        }

        uint32_t resCount;
        fread(&resCount, 1, 4, fp);
        resCount = SBig(resCount);
        for (uint32_t i=0 ; i<resCount ; ++i)
        {
            FSeek(fp, 4, SEEK_CUR);
            uint32_t type;
            fread(&type, 1, 4, fp);
            type = SBig(type);
            FSeek(fp, 8, SEEK_CUR);
            uint32_t offset;
            fread(&offset, 1, 4, fp);
            offset = SBig(offset);

            if (type == 0x41475343)
            {
                int64_t origPos = FTell(fp);
                FSeek(fp, offset, SEEK_SET);
                char testBuf[6];
                if (fread(testBuf, 1, 6, fp) == 6)
                {
                    if (!memcmp(testBuf, "Audio/", 6))
                        return true;
                    else
                        return false;
                }
                FSeek(fp, origPos, SEEK_SET);
            }
        }
    }

    return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadMP1(FILE* fp)
{
    std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
    FileLength(fp);

    uint32_t magic;
    fread(&magic, 1, 4, fp);
    magic = SBig(magic);

    if (magic == 0x00030005)
    {
        FSeek(fp, 8, SEEK_SET);

        uint32_t nameCount;
        fread(&nameCount, 1, 4, fp);
        nameCount = SBig(nameCount);
        for (uint32_t i=0 ; i<nameCount ; ++i)
        {
            FSeek(fp, 8, SEEK_CUR);
            uint32_t nameLen;
            fread(&nameLen, 1, 4, fp);
            nameLen = SBig(nameLen);
            FSeek(fp, nameLen, SEEK_CUR);
        }

        uint32_t resCount;
        fread(&resCount, 1, 4, fp);
        resCount = SBig(resCount);
        ret.reserve(resCount);
        for (uint32_t i=0 ; i<resCount ; ++i)
        {
            FSeek(fp, 4, SEEK_CUR);
            uint32_t type;
            fread(&type, 1, 4, fp);
            type = SBig(type);
            FSeek(fp, 8, SEEK_CUR);
            uint32_t offset;
            fread(&offset, 1, 4, fp);
            offset = SBig(offset);

            if (type == 0x41475343)
            {
                int64_t origPos = FTell(fp);
                FSeek(fp, offset, SEEK_SET);
                char testBuf[6];
                if (fread(testBuf, 1, 6, fp) == 6)
                {
                    if (!memcmp(testBuf, "Audio/", 6))
                    {
                        FSeek(fp, offset, SEEK_SET);
                        ReadString(fp);
                        std::string name = ReadString(fp);

                        uint32_t poolLen;
                        fread(&poolLen, 1, 4, fp);
                        poolLen = SBig(poolLen);
                        std::unique_ptr<uint8_t[]> pool(new uint8_t[poolLen]);
                        fread(pool.get(), 1, poolLen, fp);

                        uint32_t projLen;
                        fread(&projLen, 1, 4, fp);
                        projLen = SBig(projLen);
                        std::unique_ptr<uint8_t[]> proj(new uint8_t[projLen]);
                        fread(proj.get(), 1, projLen, fp);

                        uint32_t sampLen;
                        fread(&sampLen, 1, 4, fp);
                        sampLen = SBig(sampLen);
                        std::unique_ptr<uint8_t[]> samp(new uint8_t[sampLen]);
                        fread(samp.get(), 1, sampLen, fp);

                        uint32_t sdirLen;
                        fread(&sdirLen, 1, 4, fp);
                        sdirLen = SBig(sdirLen);
                        std::unique_ptr<uint8_t[]> sdir(new uint8_t[sdirLen]);
                        fread(sdir.get(), 1, sdirLen, fp);

                        ret.emplace_back(std::move(name), IntrusiveAudioGroupData{proj.release(), projLen,
                                                                                  pool.release(), poolLen,
                                                                                  sdir.release(), sdirLen,
                                                                                  samp.release(), sampLen, GCNDataTag{}});
                    }
                }
                FSeek(fp, origPos, SEEK_SET);
            }
        }
    }

    return ret;
}

static bool ValidateMP1Songs(FILE* fp)
{
    if (FileLength(fp) > 40 * 1024 * 1024)
        return false;

    uint32_t magic;
    fread(&magic, 1, 4, fp);
    magic = SBig(magic);

    if (magic == 0x00030005)
    {
        FSeek(fp, 8, SEEK_SET);

        uint32_t nameCount;
        fread(&nameCount, 1, 4, fp);
        nameCount = SBig(nameCount);
        for (uint32_t i=0 ; i<nameCount ; ++i)
        {
            FSeek(fp, 8, SEEK_CUR);
            uint32_t nameLen;
            fread(&nameLen, 1, 4, fp);
            nameLen = SBig(nameLen);
            FSeek(fp, nameLen, SEEK_CUR);
        }

        uint32_t resCount;
        fread(&resCount, 1, 4, fp);
        resCount = SBig(resCount);
        for (uint32_t i=0 ; i<resCount ; ++i)
        {
            FSeek(fp, 4, SEEK_CUR);
            uint32_t type;
            fread(&type, 1, 4, fp);
            type = SBig(type);
            FSeek(fp, 8, SEEK_CUR);
            uint32_t offset;
            fread(&offset, 1, 4, fp);
            offset = SBig(offset);

            if (type == 0x43534E47)
                return true;
        }
    }

    return false;
}

static std::vector<std::pair<std::string, ContainerRegistry::SongData>> LoadMP1Songs(FILE* fp)
{
    std::vector<std::pair<std::string, ContainerRegistry::SongData>> ret;
    FileLength(fp);

    uint32_t magic;
    fread(&magic, 1, 4, fp);
    magic = SBig(magic);

    if (magic == 0x00030005)
    {
        FSeek(fp, 8, SEEK_SET);

        uint32_t nameCount;
        fread(&nameCount, 1, 4, fp);
        nameCount = SBig(nameCount);

        std::unordered_map<uint32_t, std::string> names;
        names.reserve(nameCount);
        for (uint32_t i=0 ; i<nameCount ; ++i)
        {
            FSeek(fp, 4, SEEK_CUR);
            uint32_t id;
            fread(&id, 1, 4, fp);
            id = SBig(id);
            uint32_t nameLen;
            fread(&nameLen, 1, 4, fp);
            nameLen = SBig(nameLen);
            std::string str(nameLen, '\0');
            fread(&str[0], 1, nameLen, fp);
            names[id] = std::move(str);
        }

        uint32_t resCount;
        fread(&resCount, 1, 4, fp);
        resCount = SBig(resCount);
        ret.reserve(resCount);
        for (uint32_t i=0 ; i<resCount ; ++i)
        {
            FSeek(fp, 4, SEEK_CUR);
            uint32_t type;
            fread(&type, 1, 4, fp);
            type = SBig(type);
            uint32_t id;
            fread(&id, 1, 4, fp);
            id = SBig(id);
            uint32_t size;
            fread(&size, 1, 4, fp);
            size = SBig(size);
            uint32_t offset;
            fread(&offset, 1, 4, fp);
            offset = SBig(offset);

            if (type == 0x43534E47)
            {
                int64_t origPos = FTell(fp);
                FSeek(fp, offset + 4, SEEK_SET);

                uint32_t midiSetup;
                fread(&midiSetup, 1, 4, fp);
                midiSetup = SBig(midiSetup);

                uint32_t groupId;
                fread(&groupId, 1, 4, fp);
                groupId = SBig(groupId);

                FSeek(fp, 4, SEEK_CUR);

                uint32_t sonLength;
                fread(&sonLength, 1, 4, fp);
                sonLength = SBig(sonLength);

                std::unique_ptr<uint8_t[]> song(new uint8_t[sonLength]);
                fread(song.get(), 1, sonLength, fp);

                auto search = names.find(id);
                if (search != names.end())
                    ret.emplace_back(std::move(search->second),
                                     ContainerRegistry::SongData(std::move(song), sonLength, groupId, midiSetup));
                else
                {
                    char name[128];
                    snprintf(name, 128, "%08X", id);
                    ret.emplace_back(name, ContainerRegistry::SongData(std::move(song), sonLength, groupId, midiSetup));
                }


                FSeek(fp, origPos, SEEK_SET);
            }
        }
    }

    return ret;
}

static bool ValidateMP2(FILE* fp)
{
    if (FileLength(fp) > 40 * 1024 * 1024)
        return false;

    uint32_t magic;
    fread(&magic, 1, 4, fp);
    magic = SBig(magic);

    if (magic == 0x00030005)
    {
        FSeek(fp, 8, SEEK_SET);

        uint32_t nameCount;
        fread(&nameCount, 1, 4, fp);
        nameCount = SBig(nameCount);
        for (uint32_t i=0 ; i<nameCount ; ++i)
        {
            FSeek(fp, 8, SEEK_CUR);
            uint32_t nameLen;
            fread(&nameLen, 1, 4, fp);
            nameLen = SBig(nameLen);
            FSeek(fp, nameLen, SEEK_CUR);
        }

        uint32_t resCount;
        fread(&resCount, 1, 4, fp);
        resCount = SBig(resCount);
        for (uint32_t i=0 ; i<resCount ; ++i)
        {
            FSeek(fp, 4, SEEK_CUR);
            uint32_t type;
            fread(&type, 1, 4, fp);
            type = SBig(type);
            FSeek(fp, 8, SEEK_CUR);
            uint32_t offset;
            fread(&offset, 1, 4, fp);
            offset = SBig(offset);

            if (type == 0x41475343)
            {
                int64_t origPos = FTell(fp);
                FSeek(fp, offset, SEEK_SET);
                char testBuf[4];
                if (fread(testBuf, 1, 4, fp) == 4)
                {
                    if (amuse::SBig(*reinterpret_cast<uint32_t*>(testBuf)) == 0x1)
                        return true;
                    else
                        return false;
                }
                FSeek(fp, origPos, SEEK_SET);
            }
        }
    }

    return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadMP2(FILE* fp)
{
    std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
    FileLength(fp);

    uint32_t magic;
    fread(&magic, 1, 4, fp);
    magic = SBig(magic);

    if (magic == 0x00030005)
    {
        FSeek(fp, 8, SEEK_SET);

        uint32_t nameCount;
        fread(&nameCount, 1, 4, fp);
        nameCount = SBig(nameCount);
        for (uint32_t i=0 ; i<nameCount ; ++i)
        {
            FSeek(fp, 8, SEEK_CUR);
            uint32_t nameLen;
            fread(&nameLen, 1, 4, fp);
            nameLen = SBig(nameLen);
            FSeek(fp, nameLen, SEEK_CUR);
        }

        uint32_t resCount;
        fread(&resCount, 1, 4, fp);
        resCount = SBig(resCount);
        ret.reserve(resCount);
        for (uint32_t i=0 ; i<resCount ; ++i)
        {
            FSeek(fp, 4, SEEK_CUR);
            uint32_t type;
            fread(&type, 1, 4, fp);
            type = SBig(type);
            FSeek(fp, 8, SEEK_CUR);
            uint32_t offset;
            fread(&offset, 1, 4, fp);
            offset = SBig(offset);

            if (type == 0x41475343)
            {
                int64_t origPos = FTell(fp);
                FSeek(fp, offset, SEEK_SET);
                char testBuf[4];
                if (fread(testBuf, 1, 4, fp) == 4)
                {
                    if (amuse::SBig(*reinterpret_cast<uint32_t*>(testBuf)) == 0x1)
                    {
                        FSeek(fp, offset + 4, SEEK_SET);
                        std::string name = ReadString(fp);

                        FSeek(fp, 2, SEEK_CUR);
                        uint32_t poolSz;
                        fread(&poolSz, 1, 4, fp);
                        poolSz = SBig(poolSz);

                        uint32_t projSz;
                        fread(&projSz, 1, 4, fp);
                        projSz = SBig(projSz);

                        uint32_t sdirSz;
                        fread(&sdirSz, 1, 4, fp);
                        sdirSz = SBig(sdirSz);

                        uint32_t sampSz;
                        fread(&sampSz, 1, 4, fp);
                        sampSz = SBig(sampSz);
                        
                        if (projSz && poolSz && sdirSz && sampSz)
                        {
                            std::unique_ptr<uint8_t[]> pool(new uint8_t[poolSz]);
                            fread(pool.get(), 1, poolSz, fp);

                            std::unique_ptr<uint8_t[]> proj(new uint8_t[projSz]);
                            fread(proj.get(), 1, projSz, fp);

                            std::unique_ptr<uint8_t[]> sdir(new uint8_t[sdirSz]);
                            fread(sdir.get(), 1, sdirSz, fp);

                            std::unique_ptr<uint8_t[]> samp(new uint8_t[sampSz]);
                            fread(samp.get(), 1, sampSz, fp);

                            ret.emplace_back(std::move(name), IntrusiveAudioGroupData{proj.release(), projSz,
                                                                                      pool.release(), poolSz,
                                                                                      sdir.release(), sdirSz,
                                                                                      samp.release(), sampSz, GCNDataTag{}});
                        }
                    }
                }
                FSeek(fp, origPos, SEEK_SET);
            }
        }
    }

    return ret;
}

struct RS1FSTEntry
{
    uint32_t offset;
    uint32_t decompSz;
    uint32_t compSz;
    uint16_t type;
    uint16_t childCount;
    char name[16];

    void swapBig()
    {
        offset = SBig(offset);
        decompSz = SBig(decompSz);
        compSz = SBig(compSz);
        type = SBig(type);
        childCount = SBig(childCount);
    }
};

static void SwapN64Rom16(void* data, size_t size)
{
    uint16_t* words = reinterpret_cast<uint16_t*>(data);
    for (size_t i=0 ; i<size/2 ; ++i)
        words[i] = SBig(words[i]);
}

static void SwapN64Rom32(void* data, size_t size)
{
    uint32_t* words = reinterpret_cast<uint32_t*>(data);
    for (size_t i=0 ; i<size/4 ; ++i)
        words[i] = SBig(words[i]);
}

static bool ValidateRS1PC(FILE* fp)
{
    size_t endPos = FileLength(fp);
    if (endPos > 100 * 1024 * 1024)
        return false;

    uint32_t fstOff;
    uint32_t fstSz;
    if (fread(&fstOff, 1, 4, fp) == 4 && fread(&fstSz, 1, 4, fp) == 4)
    {
        if (fstOff + fstSz <= endPos)
        {
            FSeek(fp, fstOff, SEEK_SET);
            uint32_t elemCount = fstSz / 32;
            std::unique_ptr<RS1FSTEntry[]> entries(new RS1FSTEntry[elemCount]);
            fread(entries.get(), fstSz, 1, fp);

            uint8_t foundComps = 0;
            for (uint32_t i=0 ; i<elemCount ; ++i)
            {
                RS1FSTEntry& entry = entries[i];
                if (!strncmp("proj_SND", entry.name, 16))
                    foundComps |= 1;
                else if (!strncmp("pool_SND", entry.name, 16))
                    foundComps |= 2;
                else if (!strncmp("sdir_SND", entry.name, 16))
                    foundComps |= 4;
                else if (!strncmp("samp_SND", entry.name, 16))
                    foundComps |= 8;
            }

            if (foundComps == 0xf)
                return true;
        }
    }

    return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadRS1PC(FILE* fp)
{
    std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
    size_t endPos = FileLength(fp);

    uint32_t fstOff;
    uint32_t fstSz;
    if (fread(&fstOff, 1, 4, fp) == 4 && fread(&fstSz, 1, 4, fp) == 4)
    {
        if (fstOff + fstSz <= endPos)
        {
            FSeek(fp, fstOff, SEEK_SET);
            uint32_t elemCount = fstSz / 32;
            std::unique_ptr<RS1FSTEntry[]> entries(new RS1FSTEntry[elemCount]);
            fread(entries.get(), fstSz, 1, fp);

            std::unique_ptr<uint8_t[]> proj;
            size_t projSz = 0;
            std::unique_ptr<uint8_t[]> pool;
            size_t poolSz = 0;
            std::unique_ptr<uint8_t[]> sdir;
            size_t sdirSz = 0;
            std::unique_ptr<uint8_t[]> samp;
            size_t sampSz = 0;

            for (uint32_t i=0 ; i<elemCount ; ++i)
            {
                RS1FSTEntry& entry = entries[i];
                if (!strncmp("proj_SND", entry.name, 16))
                {
                    projSz = entry.decompSz;
                    proj.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(proj.get(), 1, entry.decompSz, fp);
                }
                else if (!strncmp("pool_SND", entry.name, 16))
                {
                    poolSz = entry.decompSz;
                    pool.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(pool.get(), 1, entry.decompSz, fp);
                }
                else if (!strncmp("sdir_SND", entry.name, 16))
                {
                    sdirSz = entry.decompSz;
                    sdir.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(sdir.get(), 1, entry.decompSz, fp);
                }
                else if (!strncmp("samp_SND", entry.name, 16))
                {
                    sampSz = entry.decompSz;
                    samp.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(samp.get(), 1, entry.decompSz, fp);
                }
            }
            
            ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), projSz, pool.release(), poolSz,
                                                              sdir.release(), sdirSz, samp.release(), sampSz,
                                                              false, PCDataTag{}});
        }
    }

    return ret;
}

static bool ValidateRS1N64(FILE* fp)
{
    size_t endPos = FileLength(fp);
    if (endPos > 32 * 1024 * 1024)
        return false; /* N64 ROM definitely won't exceed 32MB */

    std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
    fread(data.get(), 1, endPos, fp);

    if ((data[0] & 0x80) != 0x80 && (data[3] & 0x80) == 0x80)
        SwapN64Rom32(data.get(), endPos);
    else if ((data[0] & 0x80) != 0x80 && (data[1] & 0x80) == 0x80)
        SwapN64Rom16(data.get(), endPos);

#if 0
    const uint32_t* gameId = reinterpret_cast<const uint32_t*>(&data[59]);
    if (*gameId != 0x4553524e && *gameId != 0x4a53524e && *gameId != 0x5053524e)
        return false; /* GameId not 'NRSE', 'NRSJ', or 'NRSP' */
#endif

    const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos,
                                                                     "dbg_data\0\0\0\0\0\0\0\0", 16));
    if (dataSeg)
    {
        dataSeg += 28;
        size_t fstEnd = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
        dataSeg += 4;
        size_t fstOff = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
        if (endPos <= size_t(dataSeg - data.get()) + fstOff || endPos <= size_t(dataSeg - data.get()) + fstEnd)
            return false;

        const RS1FSTEntry* entry = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstOff);
        const RS1FSTEntry* lastEnt = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstEnd);

        uint8_t foundComps = 0;
        for (; entry != lastEnt ; ++entry)
        {
            if (!strncmp("proj_SND", entry->name, 16))
                foundComps |= 1;
            else if (!strncmp("pool_SND", entry->name, 16))
                foundComps |= 2;
            else if (!strncmp("sdir_SND", entry->name, 16))
                foundComps |= 4;
            else if (!strncmp("samp_SND", entry->name, 16))
                foundComps |= 8;
        }

        if (foundComps == 0xf)
            return true;
    }

    return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadRS1N64(FILE* fp)
{
    std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
    size_t endPos = FileLength(fp);

    std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
    fread(data.get(), 1, endPos, fp);

    if ((data[0] & 0x80) != 0x80 && (data[3] & 0x80) == 0x80)
        SwapN64Rom32(data.get(), endPos);
    else if ((data[0] & 0x80) != 0x80 && (data[1] & 0x80) == 0x80)
        SwapN64Rom16(data.get(), endPos);

    const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos,
                                                                     "dbg_data\0\0\0\0\0\0\0\0", 16));
    if (dataSeg)
    {
        dataSeg += 28;
        size_t fstEnd = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
        dataSeg += 4;
        size_t fstOff = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
        if (endPos <= size_t(dataSeg - data.get()) + fstOff || endPos <= size_t(dataSeg - data.get()) + fstEnd)
            return ret;

        const RS1FSTEntry* entry = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstOff);
        const RS1FSTEntry* lastEnt = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstEnd);

        std::unique_ptr<uint8_t[]> proj;
        size_t projSz = 0;
        std::unique_ptr<uint8_t[]> pool;
        size_t poolSz = 0;
        std::unique_ptr<uint8_t[]> sdir;
        size_t sdirSz = 0;
        std::unique_ptr<uint8_t[]> samp;
        size_t sampSz = 0;

        for (; entry != lastEnt ; ++entry)
        {
            RS1FSTEntry ent = *entry;
            ent.swapBig();

            if (!strncmp("proj_SND", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    proj.reset(new uint8_t[ent.decompSz]);
                    memmove(proj.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    proj.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(proj.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
                projSz = ent.decompSz;
            }
            else if (!strncmp("pool_SND", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    pool.reset(new uint8_t[ent.decompSz]);
                    memmove(pool.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    pool.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(pool.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
                poolSz = ent.decompSz;
            }
            else if (!strncmp("sdir_SND", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    sdir.reset(new uint8_t[ent.decompSz]);
                    memmove(sdir.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    sdir.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(sdir.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
                sdirSz = ent.decompSz;
            }
            else if (!strncmp("samp_SND", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    samp.reset(new uint8_t[ent.decompSz]);
                    memmove(samp.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    samp.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(samp.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
                sampSz = ent.decompSz;
            }
        }

        ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), projSz, pool.release(), poolSz,
                                                          sdir.release(), sdirSz, samp.release(), sampSz,
                                                          false, N64DataTag{}});
    }

    return ret;
}

static bool ValidateBFNPC(FILE* fp)
{
    size_t endPos = FileLength(fp);
    if (endPos > 100 * 1024 * 1024)
        return false;
    
    uint32_t fstOff;
    uint32_t fstSz;
    if (fread(&fstOff, 1, 4, fp) == 4 && fread(&fstSz, 1, 4, fp) == 4)
    {
        if (fstOff + fstSz <= endPos)
        {
            FSeek(fp, fstOff, SEEK_SET);
            uint32_t elemCount = fstSz / 32;
            std::unique_ptr<RS1FSTEntry[]> entries(new RS1FSTEntry[elemCount]);
            fread(entries.get(), fstSz, 1, fp);

            uint8_t foundComps = 0;
            for (uint32_t i=0 ; i<elemCount ; ++i)
            {
                RS1FSTEntry& entry = entries[i];
                if (!strncmp("proj", entry.name, 16))
                    foundComps |= 1;
                else if (!strncmp("pool", entry.name, 16))
                    foundComps |= 2;
                else if (!strncmp("sdir", entry.name, 16))
                    foundComps |= 4;
                else if (!strncmp("samp", entry.name, 16))
                    foundComps |= 8;
            }

            if (foundComps == 0xf)
                return true;
        }
    }

    return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadBFNPC(FILE* fp)
{
    std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
    size_t endPos = FileLength(fp);

    uint32_t fstOff;
    uint32_t fstSz;
    if (fread(&fstOff, 1, 4, fp) == 4 && fread(&fstSz, 1, 4, fp) == 4)
    {
        if (fstOff + fstSz <= endPos)
        {
            FSeek(fp, fstOff, SEEK_SET);
            uint32_t elemCount = fstSz / 32;
            std::unique_ptr<RS1FSTEntry[]> entries(new RS1FSTEntry[elemCount]);
            fread(entries.get(), fstSz, 1, fp);

            std::unique_ptr<uint8_t[]> proj;
            size_t projSz = 0;
            std::unique_ptr<uint8_t[]> pool;
            size_t poolSz = 0;
            std::unique_ptr<uint8_t[]> sdir;
            size_t sdirSz = 0;
            std::unique_ptr<uint8_t[]> samp;
            size_t sampSz = 0;

            for (uint32_t i=0 ; i<elemCount ; ++i)
            {
                RS1FSTEntry& entry = entries[i];
                if (!strncmp("proj", entry.name, 16))
                {
                    proj.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(proj.get(), 1, entry.decompSz, fp);
                    projSz = entry.decompSz;
                }
                else if (!strncmp("pool", entry.name, 16))
                {
                    pool.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(pool.get(), 1, entry.decompSz, fp);
                    poolSz = entry.decompSz;
                }
                else if (!strncmp("sdir", entry.name, 16))
                {
                    sdir.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(sdir.get(), 1, entry.decompSz, fp);
                    sdirSz = entry.decompSz;
                }
                else if (!strncmp("samp", entry.name, 16))
                {
                    samp.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(samp.get(), 1, entry.decompSz, fp);
                    sampSz = entry.decompSz;
                }
            }

            ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), projSz, pool.release(), poolSz,
                                                              sdir.release(), sdirSz, samp.release(), sampSz,
                                                              true, PCDataTag{}});
        }
    }

    return ret;
}

static bool ValidateBFNN64(FILE* fp)
{
    size_t endPos = FileLength(fp);
    if (endPos > 32 * 1024 * 1024)
        return false; /* N64 ROM definitely won't exceed 32MB */

    std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
    fread(data.get(), 1, endPos, fp);

    if ((data[0] & 0x80) != 0x80 && (data[3] & 0x80) == 0x80)
        SwapN64Rom32(data.get(), endPos);
    else if ((data[0] & 0x80) != 0x80 && (data[1] & 0x80) == 0x80)
        SwapN64Rom16(data.get(), endPos);

#if 0
    const uint32_t* gameId = reinterpret_cast<const uint32_t*>(&data[59]);
    if (*gameId != 0x4553524e && *gameId != 0x4a53524e && *gameId != 0x5053524e)
        return false; /* GameId not 'NRSE', 'NRSJ', or 'NRSP' */
#endif

    const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos,
                                                                     "dbg_data\0\0\0\0\0\0\0\0", 16));
    if (dataSeg)
    {
        dataSeg += 28;
        size_t fstEnd = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
        dataSeg += 4;
        size_t fstOff = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
        if (endPos <= size_t(dataSeg - data.get()) + fstOff || endPos <= size_t(dataSeg - data.get()) + fstEnd)
            return false;

        const RS1FSTEntry* entry = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstOff);
        const RS1FSTEntry* lastEnt = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstEnd);

        uint8_t foundComps = 0;
        for (; entry != lastEnt ; ++entry)
        {
            if (!strncmp("proj", entry->name, 16))
                foundComps |= 1;
            else if (!strncmp("pool", entry->name, 16))
                foundComps |= 2;
            else if (!strncmp("sdir", entry->name, 16))
                foundComps |= 4;
            else if (!strncmp("samp", entry->name, 16))
                foundComps |= 8;
        }

        if (foundComps == 0xf)
            return true;
    }

    return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadBFNN64(FILE* fp)
{
    std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
    size_t endPos = FileLength(fp);

    std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
    fread(data.get(), 1, endPos, fp);

    if ((data[0] & 0x80) != 0x80 && (data[3] & 0x80) == 0x80)
        SwapN64Rom32(data.get(), endPos);
    else if ((data[0] & 0x80) != 0x80 && (data[1] & 0x80) == 0x80)
        SwapN64Rom16(data.get(), endPos);

    const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos,
                                                                     "dbg_data\0\0\0\0\0\0\0\0", 16));
    if (dataSeg)
    {
        dataSeg += 28;
        size_t fstEnd = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
        dataSeg += 4;
        size_t fstOff = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
        if (endPos <= size_t(dataSeg - data.get()) + fstOff || endPos <= size_t(dataSeg - data.get()) + fstEnd)
            return ret;

        const RS1FSTEntry* entry = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstOff);
        const RS1FSTEntry* lastEnt = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstEnd);

        std::unique_ptr<uint8_t[]> proj;
        size_t projSz = 0;
        std::unique_ptr<uint8_t[]> pool;
        size_t poolSz = 0;
        std::unique_ptr<uint8_t[]> sdir;
        size_t sdirSz = 0;
        std::unique_ptr<uint8_t[]> samp;
        size_t sampSz = 0;

        for (; entry != lastEnt ; ++entry)
        {
            RS1FSTEntry ent = *entry;
            ent.swapBig();

            if (!strncmp("proj", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    proj.reset(new uint8_t[ent.decompSz]);
                    memmove(proj.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    proj.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(proj.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
                projSz = ent.decompSz;
            }
            else if (!strncmp("pool", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    pool.reset(new uint8_t[ent.decompSz]);
                    memmove(pool.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    pool.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(pool.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
                poolSz = ent.decompSz;
            }
            else if (!strncmp("sdir", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    sdir.reset(new uint8_t[ent.decompSz]);
                    memmove(sdir.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    sdir.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(sdir.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
                sdirSz = ent.decompSz;
            }
            else if (!strncmp("samp", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    samp.reset(new uint8_t[ent.decompSz]);
                    memmove(samp.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    samp.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(samp.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
                sampSz = ent.decompSz;
            }
        }

        ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), projSz, pool.release(), poolSz,
                                                          sdir.release(), sdirSz, samp.release(), sampSz,
                                                          true, N64DataTag{}});
    }

    return ret;
}

struct RS2FSTEntry
{
    uint64_t offset;
    uint64_t decompSz;
    uint64_t compSz;
    uint64_t type;
    char name[32];

    void swapBig()
    {
        offset = SBig(offset);
        decompSz = SBig(decompSz);
        compSz = SBig(compSz);
        type = SBig(type);
    }
};

struct RS23GroupHead
{
    uint32_t projOff;
    uint32_t projLen;
    uint32_t poolOff;
    uint32_t poolLen;
    uint32_t sdirOff;
    uint32_t sdirLen;
    uint32_t sampOff;
    uint32_t sampLen;

    uint32_t unkOff;
    uint32_t unkLen;

    uint32_t sonCount;
    uint32_t sonIdxBeginOff;
    uint32_t sonIdxEndOff;

    void swapBig()
    {
        projOff = SBig(projOff);
        projLen = SBig(projLen);
        poolOff = SBig(poolOff);
        poolLen = SBig(poolLen);
        sdirOff = SBig(sdirOff);
        sdirLen = SBig(sdirLen);
        sampOff = SBig(sampOff);
        sampLen = SBig(sampLen);
        unkOff = SBig(unkOff);
        unkLen = SBig(unkLen);
        sonCount = SBig(sonCount);
        sonIdxBeginOff = SBig(sonIdxBeginOff);
        sonIdxEndOff = SBig(sonIdxEndOff);
    }
};

struct RS23SONHead
{
    uint32_t offset;
    uint32_t length;
    uint16_t groupId;
    uint16_t setupId;

    void swapBig()
    {
        offset = SBig(offset);
        length = SBig(length);
        groupId = SBig(groupId);
        setupId = SBig(setupId);
    }
};

static bool ValidateRS2(FILE* fp)
{
    size_t endPos = FileLength(fp);
    if (endPos > 600 * 1024 * 1024)
        return false;
    
    uint64_t fstOff;
    fread(&fstOff, 1, 8, fp);
    fstOff = SBig(fstOff);
    uint64_t fstSz;
    fread(&fstSz, 1, 8, fp);
    fstSz = SBig(fstSz);

    if (fstOff + fstSz > endPos)
        return false;

    FSeek(fp, int64_t(fstOff), SEEK_SET);
    for (size_t i=0 ; i<fstSz/64 ; ++i)
    {
        RS2FSTEntry entry;
        fread(&entry, 1, 64, fp);
        entry.swapBig();
        if (!entry.offset)
            return false; /* This is to reject RS3 data */
        if (!strncmp("data", entry.name, 32))
            return true;
    }

    return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadRS2(FILE* fp)
{
    std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
    size_t endPos = FileLength(fp);

    uint64_t fstOff;
    fread(&fstOff, 1, 8, fp);
    fstOff = SBig(fstOff);
    uint64_t fstSz;
    fread(&fstSz, 1, 8, fp);
    fstSz = SBig(fstSz);

    if (fstOff + fstSz > endPos)
        return ret;

    FSeek(fp, int64_t(fstOff), SEEK_SET);
    for (size_t i=0 ; i<fstSz/64 ; ++i)
    {
        RS2FSTEntry entry;
        fread(&entry, 1, 64, fp);
        entry.swapBig();
        if (!strncmp("data", entry.name, 32))
        {
            FSeek(fp, int64_t(entry.offset), SEEK_SET);
            std::unique_ptr<uint8_t[]> audData(new uint8_t[entry.decompSz]);
            fread(audData.get(), 1, entry.decompSz, fp);

            uint32_t indexOff = SBig(*reinterpret_cast<uint32_t*>(audData.get() + 4));
            uint32_t groupCount = SBig(*reinterpret_cast<uint32_t*>(audData.get() + indexOff));
            const uint32_t* groupOffs = reinterpret_cast<const uint32_t*>(audData.get() + indexOff + 4);

            for (uint32_t j=0 ; j<groupCount ; ++j)
            {
                const uint8_t* groupData = audData.get() + SBig(groupOffs[j]);
                RS23GroupHead head = *reinterpret_cast<const RS23GroupHead*>(groupData);
                head.swapBig();

                std::unique_ptr<uint8_t[]> pool(new uint8_t[head.poolLen]);
                memmove(pool.get(), audData.get() + head.poolOff, head.poolLen);

                std::unique_ptr<uint8_t[]> proj(new uint8_t[head.projLen]);
                memmove(proj.get(), audData.get() + head.projOff, head.projLen);

                std::unique_ptr<uint8_t[]> sdir(new uint8_t[head.sdirLen]);
                memmove(sdir.get(), audData.get() + head.sdirOff, head.sdirLen);

                std::unique_ptr<uint8_t[]> samp(new uint8_t[head.sampLen]);
                memmove(samp.get(), audData.get() + head.sampOff, head.sampLen);

                if (head.projLen && head.poolLen && head.sdirLen && head.sampLen)
                {
                    char name[128];
                    snprintf(name, 128, "GroupFile%02u", j);
                    ret.emplace_back(name, IntrusiveAudioGroupData{proj.release(), head.projLen, pool.release(), head.poolLen,
                                                                   sdir.release(), head.sdirLen, samp.release(), head.sampLen, GCNDataTag{}});
                }
            }

            break;
        }
    }

    return ret;
}

static std::vector<std::pair<std::string, ContainerRegistry::SongData>> LoadRS2Songs(FILE* fp)
{
    std::vector<std::pair<std::string, ContainerRegistry::SongData>> ret;
    size_t endPos = FileLength(fp);

    uint64_t fstOff;
    fread(&fstOff, 1, 8, fp);
    fstOff = SBig(fstOff);
    uint64_t fstSz;
    fread(&fstSz, 1, 8, fp);
    fstSz = SBig(fstSz);

    if (fstOff + fstSz > endPos)
        return ret;

    FSeek(fp, int64_t(fstOff), SEEK_SET);
    for (size_t i=0 ; i<fstSz/64 ; ++i)
    {
        RS2FSTEntry entry;
        fread(&entry, 1, 64, fp);
        entry.swapBig();
        if (!strncmp("data", entry.name, 32))
        {
            FSeek(fp, int64_t(entry.offset), SEEK_SET);
            std::unique_ptr<uint8_t[]> audData(new uint8_t[entry.decompSz]);
            fread(audData.get(), 1, entry.decompSz, fp);

            uint32_t indexOff = SBig(*reinterpret_cast<uint32_t*>(audData.get() + 4));
            uint32_t groupCount = SBig(*reinterpret_cast<uint32_t*>(audData.get() + indexOff));
            const uint32_t* groupOffs = reinterpret_cast<const uint32_t*>(audData.get() + indexOff + 4);

            for (uint32_t j=0 ; j<groupCount ; ++j)
            {
                const uint8_t* groupData = audData.get() + SBig(groupOffs[j]);
                RS23GroupHead head = *reinterpret_cast<const RS23GroupHead*>(groupData);
                head.swapBig();

                if (!head.sonCount)
                    continue;

                const RS23SONHead* sonData = reinterpret_cast<const RS23SONHead*>(audData.get() + head.sonIdxBeginOff);
                for (int s=0 ; s<head.sonCount ; ++s)
                {
                    RS23SONHead sonHead = sonData[s];
                    sonHead.swapBig();

                    char name[128];
                    snprintf(name, 128, "GroupFile%02u-%u", j, s);
                    std::unique_ptr<uint8_t[]> song(new uint8_t[sonHead.length]);
                    memmove(song.get(), audData.get() + sonHead.offset, sonHead.length);
                    ret.emplace_back(name, ContainerRegistry::SongData(std::move(song), sonHead.length,
                                                                       sonHead.groupId, sonHead.setupId));
                }
            }

            break;
        }
    }

    return ret;
}

struct RS3FSTEntry
{
    uint64_t offset;
    uint64_t decompSz;
    uint64_t compSz;
    uint64_t type;
    char name[128];

    void swapBig()
    {
        offset = SBig(offset);
        decompSz = SBig(decompSz);
        compSz = SBig(compSz);
        type = SBig(type);
    }
};

static bool ValidateRS3(FILE* fp)
{
    size_t endPos = FileLength(fp);
    if (endPos > 600 * 1024 * 1024)
        return false;
    
    uint64_t fstOff;
    fread(&fstOff, 1, 8, fp);
    fstOff = SBig(fstOff);
    uint64_t fstSz;
    fread(&fstSz, 1, 8, fp);
    fstSz = SBig(fstSz);

    if (fstOff + fstSz > endPos)
        return false;

    FSeek(fp, int64_t(fstOff), SEEK_SET);
    for (size_t i=0 ; i<fstSz/160 ; ++i)
    {
        RS3FSTEntry entry;
        fread(&entry, 1, 160, fp);
        entry.swapBig();
        if (!strncmp("data", entry.name, 128))
            return true;
    }

    return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadRS3(FILE* fp)
{
    std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
    size_t endPos = FileLength(fp);

    uint64_t fstOff;
    fread(&fstOff, 1, 8, fp);
    fstOff = SBig(fstOff);
    uint64_t fstSz;
    fread(&fstSz, 1, 8, fp);
    fstSz = SBig(fstSz);

    if (fstOff + fstSz > endPos)
        return ret;

    FSeek(fp, int64_t(fstOff), SEEK_SET);
    for (size_t i=0 ; i<fstSz/160 ; ++i)
    {
        RS3FSTEntry entry;
        fread(&entry, 1, 160, fp);
        entry.swapBig();
        if (!strncmp("data", entry.name, 128))
        {
            FSeek(fp, int64_t(entry.offset), SEEK_SET);
            std::unique_ptr<uint8_t[]> audData(new uint8_t[entry.decompSz]);
            fread(audData.get(), 1, entry.decompSz, fp);

            uint32_t indexOff = SBig(*reinterpret_cast<uint32_t*>(audData.get() + 4));
            uint32_t groupCount = SBig(*reinterpret_cast<uint32_t*>(audData.get() + indexOff));
            const uint32_t* groupOffs = reinterpret_cast<const uint32_t*>(audData.get() + indexOff + 4);

            for (uint32_t j=0 ; j<groupCount ; ++j)
            {
                const uint8_t* groupData = audData.get() + SBig(groupOffs[j]);
                RS23GroupHead head = *reinterpret_cast<const RS23GroupHead*>(groupData);
                head.swapBig();

                std::unique_ptr<uint8_t[]> pool(new uint8_t[head.poolLen]);
                memmove(pool.get(), audData.get() + head.poolOff, head.poolLen);

                std::unique_ptr<uint8_t[]> proj(new uint8_t[head.projLen]);
                memmove(proj.get(), audData.get() + head.projOff, head.projLen);

                std::unique_ptr<uint8_t[]> sdir(new uint8_t[head.sdirLen]);
                memmove(sdir.get(), audData.get() + head.sdirOff, head.sdirLen);

                std::unique_ptr<uint8_t[]> samp(new uint8_t[head.sampLen]);
                memmove(samp.get(), audData.get() + head.sampOff, head.sampLen);

                if (head.projLen && head.poolLen && head.sdirLen && head.sampLen)
                {
                    char name[128];
                    snprintf(name, 128, "GroupFile%02u", j);
                    ret.emplace_back(name, IntrusiveAudioGroupData{proj.release(), head.projLen, pool.release(), head.poolLen,
                                                                   sdir.release(), head.sdirLen, samp.release(), head.sampLen, GCNDataTag{}});
                }
            }

            break;
        }
    }

    return ret;
}

ContainerRegistry::Type ContainerRegistry::DetectContainerType(const char* path)
{
    FILE* fp;

    /* See if provided file is one of four raw chunks */
    const char* dot = nullptr;
    if (IsChunkExtension(path, dot))
    {
        char newpath[1024];

        /* Project */
        snprintf(newpath, 1024, "%.*s.pro", int(dot - path), path);
        fp = fopen(newpath, "rb");
        if (!fp)
        {
            snprintf(newpath, 1024, "%.*s.proj", int(dot - path), path);
            fp = fopen(newpath, "rb");
            if (!fp)
                return Type::Invalid;
        }
        fclose(fp);

        /* Pool */
        snprintf(newpath, 1024, "%.*s.poo", int(dot - path), path);
        fp = fopen(newpath, "rb");
        if (!fp)
        {
            snprintf(newpath, 1024, "%.*s.pool", int(dot - path), path);
            fp = fopen(newpath, "rb");
            if (!fp)
                return Type::Invalid;
        }
        fclose(fp);

        /* Sample Directory */
        snprintf(newpath, 1024, "%.*s.sdi", int(dot - path), path);
        fp = fopen(newpath, "rb");
        if (!fp)
        {
            snprintf(newpath, 1024, "%.*s.sdir", int(dot - path), path);
            fp = fopen(newpath, "rb");
            if (!fp)
                return Type::Invalid;
        }
        fclose(fp);

        /* Sample */
        snprintf(newpath, 1024, "%.*s.sam", int(dot - path), path);
        fp = fopen(newpath, "rb");
        if (!fp)
        {
            snprintf(newpath, 1024, "%.*s.samp", int(dot - path), path);
            fp = fopen(newpath, "rb");
            if (!fp)
                return Type::Invalid;
        }
        fclose(fp);

        return Type::Raw4;
    }

    /* Now attempt single-file case */
    fp = fopen(path, "rb");
    if (fp)
    {
        if (ValidateMP1(fp))
        {
            fclose(fp);
            return Type::MetroidPrime;
        }

        if (ValidateMP2(fp))
        {
            fclose(fp);
            return Type::MetroidPrime2;
        }

        if (ValidateRS1PC(fp))
        {
            fclose(fp);
            return Type::RogueSquadronPC;
        }

        if (ValidateRS1N64(fp))
        {
            fclose(fp);
            return Type::RogueSquadronN64;
        }

        if (ValidateBFNPC(fp))
        {
            fclose(fp);
            return Type::BattleForNabooPC;
        }

        if (ValidateBFNN64(fp))
        {
            fclose(fp);
            return Type::BattleForNabooN64;
        }

        if (ValidateRS2(fp))
        {
            fclose(fp);
            return Type::RogueSquadron2;
        }

        if (ValidateRS3(fp))
        {
            fclose(fp);
            return Type::RogueSquadron3;
        }

        fclose(fp);
    }

    return Type::Invalid;
}
    
std::vector<std::pair<std::string, IntrusiveAudioGroupData>>
ContainerRegistry::LoadContainer(const char* path)
{
    Type typeOut;
    return LoadContainer(path, typeOut);
};

std::vector<std::pair<std::string, IntrusiveAudioGroupData>>
ContainerRegistry::LoadContainer(const char* path, Type& typeOut)
{
    FILE* fp;
    typeOut = Type::Invalid;

    /* See if provided file is one of four raw chunks */
    const char* dot = nullptr;
    if (IsChunkExtension(path, dot))
    {
        std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;

        /* Project */
        char projPath[1024];
        snprintf(projPath, 1024, "%.*s.pro", int(dot - path), path);
        fp = fopen(projPath, "rb");
        if (!fp)
        {
            snprintf(projPath, 1024, "%.*s.proj", int(dot - path), path);
            fp = fopen(projPath, "rb");
            if (!fp)
                return ret;
        }
        fclose(fp);

        /* Pool */
        char poolPath[1024];
        snprintf(poolPath, 1024, "%.*s.poo", int(dot - path), path);
        fp = fopen(poolPath, "rb");
        if (!fp)
        {
            snprintf(poolPath, 1024, "%.*s.pool", int(dot - path), path);
            fp = fopen(poolPath, "rb");
            if (!fp)
                return ret;
        }
        fclose(fp);

        /* Sample Directory */
        char sdirPath[1024];
        snprintf(sdirPath, 1024, "%.*s.sdi", int(dot - path), path);
        fp = fopen(sdirPath, "rb");
        if (!fp)
        {
            snprintf(sdirPath, 1024, "%.*s.sdir", int(dot - path), path);
            fp = fopen(sdirPath, "rb");
            if (!fp)
                return ret;
        }
        fclose(fp);

        /* Sample */
        char sampPath[1024];
        snprintf(sampPath, 1024, "%.*s.sam", int(dot - path), path);
        fp = fopen(sampPath, "rb");
        if (!fp)
        {
            snprintf(sampPath, 1024, "%.*s.samp", int(dot - path), path);
            fp = fopen(sampPath, "rb");
            if (!fp)
                return ret;
        }
        fclose(fp);

        fp = fopen(projPath, "rb");
        size_t projLen = FileLength(fp);
        if (!projLen)
            return ret;
        std::unique_ptr<uint8_t[]> proj(new uint8_t[projLen]);
        fread(proj.get(), 1, projLen, fp);

        fp = fopen(poolPath, "rb");
        size_t poolLen = FileLength(fp);
        if (!poolLen)
            return ret;
        std::unique_ptr<uint8_t[]> pool(new uint8_t[poolLen]);
        fread(pool.get(), 1, poolLen, fp);

        fp = fopen(sdirPath, "rb");
        size_t sdirLen = FileLength(fp);
        if (!sdirLen)
            return ret;
        std::unique_ptr<uint8_t[]> sdir(new uint8_t[sdirLen]);
        fread(sdir.get(), 1, sdirLen, fp);

        fp = fopen(sampPath, "rb");
        size_t sampLen = FileLength(fp);
        if (!sampLen)
            return ret;
        std::unique_ptr<uint8_t[]> samp(new uint8_t[sampLen]);
        fread(samp.get(), 1, sampLen, fp);

        fclose(fp);

        /* SDIR-based format detection */
        if (*reinterpret_cast<uint32_t*>(sdir.get() + 8) == 0x0)
            ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), projLen, pool.release(), poolLen,
                                                              sdir.release(), sdirLen, samp.release(), sampLen,
                                                              GCNDataTag{}});
        else if (sdir[9] == 0x0)
            ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), projLen, pool.release(), poolLen,
                                                              sdir.release(), sdirLen, samp.release(), sampLen,
                                                              false, N64DataTag{}});
        else
            ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), projLen, pool.release(), poolLen,
                                                              sdir.release(), sdirLen, samp.release(), sampLen,
                                                              false, PCDataTag{}});

        typeOut = Type::Raw4;
        return ret;
    }

    /* Now attempt single-file case */
    fp = fopen(path, "rb");
    if (fp)
    {
        if (ValidateMP1(fp))
        {
            auto ret = LoadMP1(fp);
            fclose(fp);
            typeOut = Type::MetroidPrime;
            return ret;
        }

        if (ValidateMP2(fp))
        {
            auto ret = LoadMP2(fp);
            fclose(fp);
            typeOut = Type::MetroidPrime2;
            return ret;
        }

        if (ValidateRS1PC(fp))
        {
            auto ret = LoadRS1PC(fp);
            fclose(fp);
            typeOut = Type::RogueSquadronPC;
            return ret;
        }

        if (ValidateRS1N64(fp))
        {
            auto ret = LoadRS1N64(fp);
            fclose(fp);
            typeOut = Type::RogueSquadronN64;
            return ret;
        }

        if (ValidateBFNPC(fp))
        {
            auto ret = LoadBFNPC(fp);
            fclose(fp);
            typeOut = Type::BattleForNabooPC;
            return ret;
        }

        if (ValidateBFNN64(fp))
        {
            auto ret = LoadBFNN64(fp);
            fclose(fp);
            typeOut = Type::BattleForNabooN64;
            return ret;
        }

        if (ValidateRS2(fp))
        {
            auto ret = LoadRS2(fp);
            fclose(fp);
            typeOut = Type::RogueSquadron2;
            return ret;
        }

        if (ValidateRS3(fp))
        {
            auto ret = LoadRS3(fp);
            fclose(fp);
            typeOut = Type::RogueSquadron3;
            return ret;
        }

        fclose(fp);
    }

    return {};
}

std::vector<std::pair<std::string, ContainerRegistry::SongData>>
ContainerRegistry::LoadSongs(const char* path)
{
    FILE* fp;

    /* See if provided file is a raw song */
    const char* dot = nullptr;
    if (IsSongExtension(path, dot))
    {
        fp = fopen(path, "rb");
        size_t fLen = FileLength(fp);
        if (!fLen)
        {
            fclose(fp);
            return {};
        }
        std::unique_ptr<uint8_t[]> song(new uint8_t[fLen]);
        fread(song.get(), 1, fLen, fp);
        fclose(fp);

        std::vector<std::pair<std::string, SongData>> ret;
        ret.emplace_back("Song", SongData(std::move(song), fLen, -1, -1));
        return ret;
    }

    /* Now attempt archive-file case */
    fp = fopen(path, "rb");
    if (fp)
    {
        if (ValidateMP1Songs(fp))
        {
            auto ret = LoadMP1Songs(fp);
            fclose(fp);
            return ret;
        }

#if 0
        if (ValidateRS1PCSongs(fp))
        {
            auto ret = LoadRS1PCSongs(fp);
            fclose(fp);
            return ret;
        }

        if (ValidateRS1N64Songs(fp))
        {
            auto ret = LoadRS1N64Songs(fp);
            fclose(fp);
            return ret;
        }

        if (ValidateBFNPCSongs(fp))
        {
            auto ret = LoadBFNPCSongs(fp);
            fclose(fp);
            return ret;
        }

        if (ValidateBFNN64Songs(fp))
        {
            auto ret = LoadBFNN64Songs(fp);
            fclose(fp);
            return ret;
        }
#endif

        if (ValidateRS2(fp))
        {
            auto ret = LoadRS2Songs(fp);
            fclose(fp);
            return ret;
        }

        fclose(fp);
    }

    return {};
}

}

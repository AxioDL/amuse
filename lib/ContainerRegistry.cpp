#include "amuse/ContainerRegistry.hpp"
#include "amuse/Common.hpp"
#include <stdio.h>
#include <string.h>
#include <string>
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
        return "Star Wars: Rogue Squadron (PC)";
    case Type::RogueSquadronN64:
        return "Star Wars: Rogue Squadron (N64)";
    case Type::RogueSquadron2:
        return "Star Wars: Rogue Squadron 2 (GCN)";
    case Type::RogueSquadron3:
        return "Star Wars: Rogue Squadron 3 (GCN)";
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

static bool ValidateMP1(FILE* fp)
{
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

                        uint32_t len;
                        fread(&len, 1, 4, fp);
                        len = SBig(len);
                        std::unique_ptr<uint8_t[]> pool(new uint8_t[len]);
                        fread(pool.get(), 1, len, fp);

                        fread(&len, 1, 4, fp);
                        len = SBig(len);
                        std::unique_ptr<uint8_t[]> proj(new uint8_t[len]);
                        fread(proj.get(), 1, len, fp);

                        fread(&len, 1, 4, fp);
                        len = SBig(len);
                        std::unique_ptr<uint8_t[]> samp(new uint8_t[len]);
                        fread(samp.get(), 1, len, fp);

                        fread(&len, 1, 4, fp);
                        len = SBig(len);
                        std::unique_ptr<uint8_t[]> sdir(new uint8_t[len]);
                        fread(sdir.get(), 1, len, fp);

                        ret.emplace_back(std::move(name), IntrusiveAudioGroupData{proj.release(), pool.release(),
                                                                                  sdir.release(), samp.release(), GCNDataTag{}});
                    }
                }
                FSeek(fp, origPos, SEEK_SET);
            }
        }
    }

    return ret;
}

static bool ValidateMP2(FILE* fp)
{
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

                        std::unique_ptr<uint8_t[]> pool(new uint8_t[poolSz]);
                        fread(pool.get(), 1, poolSz, fp);

                        std::unique_ptr<uint8_t[]> proj(new uint8_t[projSz]);
                        fread(pool.get(), 1, projSz, fp);

                        std::unique_ptr<uint8_t[]> sdir(new uint8_t[sdirSz]);
                        fread(pool.get(), 1, sdirSz, fp);

                        std::unique_ptr<uint8_t[]> samp(new uint8_t[sampSz]);
                        fread(pool.get(), 1, sampSz, fp);

                        ret.emplace_back(std::move(name), IntrusiveAudioGroupData{proj.release(), pool.release(),
                                                                                  sdir.release(), samp.release(), GCNDataTag{}});
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

static void SwapN64Rom(void* data, size_t size)
{
    uint32_t* words = reinterpret_cast<uint32_t*>(data);
    for (size_t i=0 ; i<size/4 ; ++i)
        words[i] = SBig(words[i]);
}

static bool ValidateRS1PC(FILE* fp)
{
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
            std::unique_ptr<uint8_t[]> pool;
            std::unique_ptr<uint8_t[]> sdir;
            std::unique_ptr<uint8_t[]> samp;

            for (uint32_t i=0 ; i<elemCount ; ++i)
            {
                RS1FSTEntry& entry = entries[i];
                if (!strncmp("proj_SND", entry.name, 16))
                {
                    proj.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(proj.get(), 1, entry.decompSz, fp);
                }
                else if (!strncmp("pool_SND", entry.name, 16))
                {
                    pool.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(pool.get(), 1, entry.decompSz, fp);
                }
                else if (!strncmp("sdir_SND", entry.name, 16))
                {
                    sdir.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(sdir.get(), 1, entry.decompSz, fp);
                }
                else if (!strncmp("samp_SND", entry.name, 16))
                {
                    samp.reset(new uint8_t[entry.decompSz]);
                    FSeek(fp, entry.offset, SEEK_SET);
                    fread(samp.get(), 1, entry.decompSz, fp);
                }
            }
            
            ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), pool.release(),
                                                              sdir.release(), samp.release(), PCDataTag{}});
        }
    }

    return ret;
}

static bool ValidateRS1N64(FILE* fp)
{
    size_t endPos = FileLength(fp);
    if (endPos > 32 * 1024 * 1024)
        return false; /* N64 ROM definitely won't exceed 32MB */

    FSeek(fp, 59, SEEK_SET);
    uint32_t gameId;
    fread(&gameId, 1, 4, fp);
    if (gameId != 0x4e525345 && gameId != 0x4553524e)
        return false; /* GameId not 'NRSE' */
    FSeek(fp, 0, SEEK_SET);

    std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
    fread(data.get(), 1, endPos, fp);

    if ((data[0] & 0x80) != 0x80 && (data[3] & 0x80) == 0x80)
        SwapN64Rom(data.get(), endPos);

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
        SwapN64Rom(data.get(), endPos);

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
        std::unique_ptr<uint8_t[]> pool;
        std::unique_ptr<uint8_t[]> sdir;
        std::unique_ptr<uint8_t[]> samp;

        for (; entry != lastEnt ; ++entry)
        {
            RS1FSTEntry ent = *entry;
            ent.swapBig();

            if (!strncmp("proj_SND", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    proj.reset(new uint8_t[ent.decompSz]);
                    memcpy(proj.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    proj.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(proj.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
            }
            else if (!strncmp("pool_SND", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    pool.reset(new uint8_t[ent.decompSz]);
                    memcpy(pool.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    pool.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(pool.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
            }
            else if (!strncmp("sdir_SND", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    sdir.reset(new uint8_t[ent.decompSz]);
                    memcpy(sdir.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    sdir.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(sdir.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
            }
            else if (!strncmp("samp_SND", ent.name, 16))
            {
                if (ent.compSz == 0xffffffff)
                {
                    samp.reset(new uint8_t[ent.decompSz]);
                    memcpy(samp.get(), dataSeg + ent.offset, ent.decompSz);
                }
                else
                {
                    samp.reset(new uint8_t[ent.decompSz]);
                    uLongf outSz = ent.decompSz;
                    uncompress(samp.get(), &outSz, dataSeg + ent.offset, ent.compSz);
                }
            }
        }

        ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), pool.release(),
                                                          sdir.release(), samp.release(), N64DataTag{}});
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
    }
};

static bool ValidateRS2(FILE* fp)
{
    size_t endPos = FileLength(fp);

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
                memcpy(pool.get(), audData.get() + head.poolOff, head.poolLen);

                std::unique_ptr<uint8_t[]> proj(new uint8_t[head.projLen]);
                memcpy(proj.get(), audData.get() + head.projOff, head.projLen);

                std::unique_ptr<uint8_t[]> sdir(new uint8_t[head.sdirLen]);
                memcpy(sdir.get(), audData.get() + head.sdirOff, head.sdirLen);

                std::unique_ptr<uint8_t[]> samp(new uint8_t[head.sampLen]);
                memcpy(samp.get(), audData.get() + head.sampOff, head.sampLen);

                char name[128];
                snprintf(name, 128, "GroupFile%u", j);
                ret.emplace_back(name, IntrusiveAudioGroupData{proj.release(), pool.release(),
                                                               sdir.release(), samp.release(), GCNDataTag{}});
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
                memcpy(pool.get(), audData.get() + head.poolOff, head.poolLen);

                std::unique_ptr<uint8_t[]> proj(new uint8_t[head.projLen]);
                memcpy(proj.get(), audData.get() + head.projOff, head.projLen);

                std::unique_ptr<uint8_t[]> sdir(new uint8_t[head.sdirLen]);
                memcpy(sdir.get(), audData.get() + head.sdirOff, head.sdirLen);

                std::unique_ptr<uint8_t[]> samp(new uint8_t[head.sampLen]);
                memcpy(samp.get(), audData.get() + head.sampOff, head.sampLen);

                char name[128];
                snprintf(name, 128, "GroupFile%u", j);
                ret.emplace_back(name, IntrusiveAudioGroupData{proj.release(), pool.release(),
                                                               sdir.release(), samp.release(), GCNDataTag{}});
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
    FILE* fp;

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
        size_t fLen = FileLength(fp);
        if (!fLen)
            return ret;
        std::unique_ptr<uint8_t[]> proj(new uint8_t[fLen]);
        fread(proj.get(), 1, fLen, fp);

        fp = fopen(poolPath, "rb");
        fLen = FileLength(fp);
        if (!fLen)
            return ret;
        std::unique_ptr<uint8_t[]> pool(new uint8_t[fLen]);
        fread(pool.get(), 1, fLen, fp);

        fp = fopen(sdirPath, "rb");
        fLen = FileLength(fp);
        if (!fLen)
            return ret;
        std::unique_ptr<uint8_t[]> sdir(new uint8_t[fLen]);
        fread(sdir.get(), 1, fLen, fp);

        fp = fopen(sampPath, "rb");
        fLen = FileLength(fp);
        if (!fLen)
            return ret;
        std::unique_ptr<uint8_t[]> samp(new uint8_t[fLen]);
        fread(samp.get(), 1, fLen, fp);

        fclose(fp);

        /* SDIR-based format detection */
        if (*reinterpret_cast<uint32_t*>(sdir.get() + 8) == 0x0)
            ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), pool.release(),
                                                              sdir.release(), samp.release(), GCNDataTag{}});
        else if (sdir[9] == 0x0)
            ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), pool.release(),
                                                              sdir.release(), samp.release(), N64DataTag{}});
        else
            ret.emplace_back("Group", IntrusiveAudioGroupData{proj.release(), pool.release(),
                                                              sdir.release(), samp.release(), PCDataTag{}});

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
            return ret;
        }

        if (ValidateMP2(fp))
        {
            auto ret = LoadMP2(fp);
            fclose(fp);
            return ret;
        }

        if (ValidateRS1PC(fp))
        {
            auto ret = LoadRS1PC(fp);
            fclose(fp);
            return ret;
        }

        if (ValidateRS1N64(fp))
        {
            auto ret = LoadRS1N64(fp);
            fclose(fp);
            return ret;
        }

        if (ValidateRS2(fp))
        {
            auto ret = LoadRS2(fp);
            fclose(fp);
            return ret;
        }

        if (ValidateRS3(fp))
        {
            auto ret = LoadRS3(fp);
            fclose(fp);
            return ret;
        }

        fclose(fp);
    }

    return {};
}

}

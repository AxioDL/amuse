#include "amuse/ContainerRegistry.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>

#include "amuse/Common.hpp"

#include <lzokay.hpp>
#include <zlib.h>

#if __SWITCH__
/*-
 * Copyright (c) 2005 Pascal Gloor <pascal.gloor@spale.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. The name of the author may not be used to endorse or promote
 *    products derived from this software without specific prior written
 *    permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
static void* memmem(const void* l, size_t l_len, const void* s, size_t s_len) {
  char *cur, *last;
  const char* cl = (const char*)l;
  const char* cs = (const char*)s;

  /* we need something to compare */
  if (l_len == 0 || s_len == 0)
    return NULL;

  /* "s" must be smaller or equal to "l" */
  if (l_len < s_len)
    return NULL;

  /* special case where s_len == 1 */
  if (s_len == 1)
    return memchr(l, (int)*cs, l_len);

  /* the last position where its possible to find "s" in "l" */
  last = (char*)cl + l_len - s_len;

  for (cur = (char*)cl; cur <= last; cur++)
    if (cur[0] == cs[0] && memcmp(cur, cs, s_len) == 0)
      return cur;

  return NULL;
}
#endif

#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Stringapiset.h>

static void* memmem(const void* haystack, size_t hlen, const void* needle, size_t nlen) {
  int needle_first;
  const uint8_t* p = static_cast<const uint8_t*>(haystack);
  size_t plen = hlen;

  if (!nlen)
    return NULL;

  needle_first = *(unsigned char*)needle;

  while (plen >= nlen && (p = static_cast<const uint8_t*>(memchr(p, needle_first, plen - nlen + 1)))) {
    if (!memcmp(p, needle, nlen))
      return (void*)p;

    p++;
    plen = hlen - (p - static_cast<const uint8_t*>(haystack));
  }

  return NULL;
}

#endif

static std::string StrToSys(const std::string& str) { return str; }

namespace amuse {

const char* ContainerRegistry::TypeToName(Type tp) {
  switch (tp) {
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
  case Type::Factor5N64Rev:
    return "Factor5 Revision ROM (N64)";
  case Type::RogueSquadron2:
    return "Star Wars - Rogue Squadron 2 (GCN)";
  case Type::RogueSquadron3:
    return "Star Wars - Rogue Squadron 3 (GCN)";
  }
}

static size_t FileLength(FILE* fp) {
  FSeek(fp, 0, SEEK_END);
  int64_t endPos = FTell(fp);
  fseek(fp, 0, SEEK_SET);
  return size_t(endPos);
}

static std::string ReadString(FILE* fp) {
  char byte;
  std::string ret;
  while (fread(&byte, 1, 1, fp) == 1 && byte != 0)
    ret.push_back(byte);
  return ret;
}

static bool IsChunkExtension(const char* path, const char*& dotOut) {
  const char* ext = StrRChr(path, '.');
  if (ext) {
    if (!CompareCaseInsensitive(ext, ".poo") || !CompareCaseInsensitive(ext, ".pool") ||
        !CompareCaseInsensitive(ext, ".pro") || !CompareCaseInsensitive(ext, ".proj") ||
        !CompareCaseInsensitive(ext, ".sdi") || !CompareCaseInsensitive(ext, ".sdir") ||
        !CompareCaseInsensitive(ext, ".sam") || !CompareCaseInsensitive(ext, ".samp")) {
      dotOut = ext;
      return true;
    }
  }
  return false;
}

static bool IsSongExtension(const char* path, const char*& dotOut) {
  const char* ext = StrRChr(path, '.');
  if (ext) {
    if (!CompareCaseInsensitive(ext, ".son") || !CompareCaseInsensitive(ext, ".sng") ||
        !CompareCaseInsensitive(ext, ".song")) {
      dotOut = ext;
      return true;
    }
  }
  return false;
}

static bool ValidateMP1(FILE* fp) {
  if (FileLength(fp) > 40 * 1024 * 1024)
    return false;

  uint32_t magic;
  fread(&magic, 1, 4, fp);
  magic = SBig(magic);

  if (magic == 0x00030005) {
    FSeek(fp, 8, SEEK_SET);

    uint32_t nameCount;
    fread(&nameCount, 1, 4, fp);
    nameCount = SBig(nameCount);
    for (uint32_t i = 0; i < nameCount; ++i) {
      FSeek(fp, 8, SEEK_CUR);
      uint32_t nameLen;
      fread(&nameLen, 1, 4, fp);
      nameLen = SBig(nameLen);
      FSeek(fp, nameLen, SEEK_CUR);
    }

    uint32_t resCount;
    fread(&resCount, 1, 4, fp);
    resCount = SBig(resCount);
    for (uint32_t i = 0; i < resCount; ++i) {
      FSeek(fp, 4, SEEK_CUR);
      uint32_t type;
      fread(&type, 1, 4, fp);
      type = SBig(type);
      FSeek(fp, 8, SEEK_CUR);
      uint32_t offset;
      fread(&offset, 1, 4, fp);
      offset = SBig(offset);

      if (type == 0x41475343) {
        int64_t origPos = FTell(fp);
        FSeek(fp, offset, SEEK_SET);
        char testBuf[6];
        if (fread(testBuf, 1, 6, fp) == 6) {
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

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadMP1(FILE* fp) {
  std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
  FileLength(fp);

  uint32_t magic;
  fread(&magic, 1, 4, fp);
  magic = SBig(magic);

  if (magic == 0x00030005) {
    FSeek(fp, 8, SEEK_SET);

    uint32_t nameCount;
    fread(&nameCount, 1, 4, fp);
    nameCount = SBig(nameCount);
    for (uint32_t i = 0; i < nameCount; ++i) {
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
    for (uint32_t i = 0; i < resCount; ++i) {
      FSeek(fp, 4, SEEK_CUR);
      uint32_t type;
      fread(&type, 1, 4, fp);
      type = SBig(type);
      FSeek(fp, 8, SEEK_CUR);
      uint32_t offset;
      fread(&offset, 1, 4, fp);
      offset = SBig(offset);

      if (type == 0x41475343) {
        int64_t origPos = FTell(fp);
        FSeek(fp, offset, SEEK_SET);
        char testBuf[6];
        if (fread(testBuf, 1, 6, fp) == 6) {
          if (!memcmp(testBuf, "Audio/", 6)) {
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

            ret.emplace_back(std::move(name),
                             IntrusiveAudioGroupData{proj.release(), projLen, pool.release(), poolLen, sdir.release(),
                                                     sdirLen, samp.release(), sampLen, GCNDataTag{}});
          }
        }
        FSeek(fp, origPos, SEEK_SET);
      }
    }
  }

  return ret;
}

static bool ValidateMP1Songs(FILE* fp) {
  if (FileLength(fp) > 40 * 1024 * 1024)
    return false;

  uint32_t magic;
  fread(&magic, 1, 4, fp);
  magic = SBig(magic);

  if (magic == 0x00030005) {
    FSeek(fp, 8, SEEK_SET);

    uint32_t nameCount;
    fread(&nameCount, 1, 4, fp);
    nameCount = SBig(nameCount);
    for (uint32_t i = 0; i < nameCount; ++i) {
      FSeek(fp, 8, SEEK_CUR);
      uint32_t nameLen;
      fread(&nameLen, 1, 4, fp);
      nameLen = SBig(nameLen);
      FSeek(fp, nameLen, SEEK_CUR);
    }

    uint32_t resCount;
    fread(&resCount, 1, 4, fp);
    resCount = SBig(resCount);
    for (uint32_t i = 0; i < resCount; ++i) {
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

static std::vector<std::pair<std::string, ContainerRegistry::SongData>> LoadMP1Songs(FILE* fp) {
  std::vector<std::pair<std::string, ContainerRegistry::SongData>> ret;
  FileLength(fp);

  uint32_t magic;
  fread(&magic, 1, 4, fp);
  magic = SBig(magic);

  if (magic == 0x00030005) {
    FSeek(fp, 8, SEEK_SET);

    uint32_t nameCount;
    fread(&nameCount, 1, 4, fp);
    nameCount = SBig(nameCount);

    std::unordered_map<uint32_t, std::string> names;
    names.reserve(nameCount);
    for (uint32_t i = 0; i < nameCount; ++i) {
      FSeek(fp, 4, SEEK_CUR);
      uint32_t id;
      fread(&id, 1, 4, fp);
      id = SBig(id);
      uint32_t nameLen;
      fread(&nameLen, 1, 4, fp);
      nameLen = SBig(nameLen);
      std::string str(nameLen, '\0');
      fread(&str[0], 1, nameLen, fp);
      names[id] = StrToSys(str);
    }

    uint32_t resCount;
    fread(&resCount, 1, 4, fp);
    resCount = SBig(resCount);
    ret.reserve(resCount);
    for (uint32_t i = 0; i < resCount; ++i) {
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

      if (type == 0x43534E47) {
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
        else {
          std::string name = fmt::format(FMT_STRING("{:08X}"), id);
          ret.emplace_back(std::move(name), ContainerRegistry::SongData(std::move(song), sonLength, groupId, midiSetup));
        }

        FSeek(fp, origPos, SEEK_SET);
      }
    }
  }

  return ret;
}

static bool ValidateMP2(FILE* fp) {
  uint32_t magic;
  fread(&magic, 1, 4, fp);
  magic = SBig(magic);

  if (magic == 0x00030005) {
    FSeek(fp, 8, SEEK_SET);

    uint32_t nameCount;
    fread(&nameCount, 1, 4, fp);
    nameCount = SBig(nameCount);
    for (uint32_t i = 0; i < nameCount; ++i) {
      FSeek(fp, 8, SEEK_CUR);
      uint32_t nameLen;
      fread(&nameLen, 1, 4, fp);
      nameLen = SBig(nameLen);
      FSeek(fp, nameLen, SEEK_CUR);
    }

    uint32_t resCount;
    fread(&resCount, 1, 4, fp);
    resCount = SBig(resCount);
    for (uint32_t i = 0; i < resCount; ++i) {
      uint32_t compressed;
      fread(&compressed, 1, 4, fp);
      uint32_t type;
      fread(&type, 1, 4, fp);
      type = SBig(type);
      FSeek(fp, 8, SEEK_CUR);
      uint32_t offset;
      fread(&offset, 1, 4, fp);
      offset = SBig(offset);

      if (type == 0x41475343) {
        int64_t origPos = FTell(fp);
        FSeek(fp, offset, SEEK_SET);
        char testBuf[4];
        if (!compressed) {
          if (fread(testBuf, 1, 4, fp) != 4)
            return false;
        } else {
          FSeek(fp, 4, SEEK_CUR);
          uint16_t chunkSz;
          fread(&chunkSz, 1, 2, fp);
          chunkSz = SBig(chunkSz);
          uint8_t compBuf[0x8000];
          uint8_t destBuf[0x8000 * 2];
          fread(compBuf, 1, chunkSz, fp);
          size_t dsz;
          lzokay::decompress(compBuf, chunkSz, destBuf, 0x8000 * 2, dsz);
          memcpy(testBuf, destBuf, 4);
        }
        if (amuse::SBig(*reinterpret_cast<uint32_t*>(testBuf)) == 0x1)
          return true;
        else
          return false;
        FSeek(fp, origPos, SEEK_SET);
      }
    }
  }

  return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadMP2(FILE* fp) {
  std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
  FileLength(fp);

  uint32_t magic;
  fread(&magic, 1, 4, fp);
  magic = SBig(magic);

  if (magic == 0x00030005) {
    FSeek(fp, 8, SEEK_SET);

    uint32_t nameCount;
    fread(&nameCount, 1, 4, fp);
    nameCount = SBig(nameCount);
    for (uint32_t i = 0; i < nameCount; ++i) {
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
    for (uint32_t i = 0; i < resCount; ++i) {
      uint32_t compressed;
      fread(&compressed, 1, 4, fp);
      uint32_t type;
      fread(&type, 1, 4, fp);
      type = SBig(type);
      FSeek(fp, 8, SEEK_CUR);
      uint32_t offset;
      fread(&offset, 1, 4, fp);
      offset = SBig(offset);

      if (type == 0x41475343) {
        int64_t origPos = FTell(fp);
        FSeek(fp, offset, SEEK_SET);
        char testBuf[4];
        FILE* old_fp = fp;
        if (compressed) {
          uint32_t decompSz;
          fread(&decompSz, 1, 4, fp);
          decompSz = SBig(decompSz);
          uint8_t compBuf[0x8000];
          uint8_t* buf = new uint8_t[decompSz];
          uint8_t* bufCur = buf;
          uint32_t rem = decompSz;
          while (rem) {
            uint16_t chunkSz;
            fread(&chunkSz, 1, 2, fp);
            chunkSz = SBig(chunkSz);
            fread(compBuf, 1, chunkSz, fp);
            size_t dsz;
            lzokay::decompress(compBuf, chunkSz, bufCur, rem, dsz);
            bufCur += dsz;
            rem -= dsz;
          }

          fp = FOpen("amuse_tmp.dat", "rw");
          rewind(fp);
          fwrite(buf, 1, decompSz, fp);
          rewind(fp);
        }
        if (fread(testBuf, 1, 4, fp) == 4) {
          if (amuse::SBig(*reinterpret_cast<uint32_t*>(testBuf)) == 0x1) {
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

            if (projSz && poolSz && sdirSz && sampSz) {
              std::unique_ptr<uint8_t[]> pool(new uint8_t[poolSz]);
              fread(pool.get(), 1, poolSz, fp);

              std::unique_ptr<uint8_t[]> proj(new uint8_t[projSz]);
              fread(proj.get(), 1, projSz, fp);

              std::unique_ptr<uint8_t[]> sdir(new uint8_t[sdirSz]);
              fread(sdir.get(), 1, sdirSz, fp);

              std::unique_ptr<uint8_t[]> samp(new uint8_t[sampSz]);
              fread(samp.get(), 1, sampSz, fp);

              ret.emplace_back(std::move(name),
                               IntrusiveAudioGroupData{proj.release(), projSz, pool.release(), poolSz, sdir.release(),
                                                       sdirSz, samp.release(), sampSz, GCNDataTag{}});
            }
          }
        }
        if (compressed) {
          fclose(fp);
          Unlink("amuse_tmp.dat");
        }
        fp = old_fp;
        FSeek(fp, origPos, SEEK_SET);
      }
    }
  }

  return ret;
}

struct RS1FSTEntry {
  uint32_t offset;
  uint32_t decompSz;
  uint32_t compSz;
  uint16_t type;
  uint16_t childCount;
  char name[16];

  void swapBig() {
    offset = SBig(offset);
    decompSz = SBig(decompSz);
    compSz = SBig(compSz);
    type = SBig(type);
    childCount = SBig(childCount);
  }
};

static void SwapN64Rom16(void* data, size_t size) {
  uint16_t* words = reinterpret_cast<uint16_t*>(data);
  for (size_t i = 0; i < size / 2; ++i)
    words[i] = SBig(words[i]);
}

static void SwapN64Rom32(void* data, size_t size) {
  uint32_t* words = reinterpret_cast<uint32_t*>(data);
  for (size_t i = 0; i < size / 4; ++i)
    words[i] = SBig(words[i]);
}

static const struct RS1SongMapping {
  const char* name;
  int songId;
} RS1Mappings[] = {{"logo1_SNG", 0},       {"roguetitle_SNG", 1},
                   {"roguetheme_SNG", 1},  {"title_SNG", 2},
                   {"hangar1_SNG", 3},     {"hangar2_SNG", 3},
                   {"hangar3_SNG", 3},     {"jingle01_SNG", 4},
                   {"jingle02_SNG", 4},    {"jingle03_SNG", 4},
                   {"jingle04_SNG", 4},    {"jingle05_SNG", 4},
                   {"jingle06_SNG", 4},    {"jingle07_SNG", 4},
                   {"jingle08_SNG", 4},    {"c1l1_theme_SNG", 4},
                   {"c1l1_prob1_SNG", 4},  {"c1l1_prob2_SNG", 4},
                   {"c1l1_spc01_SNG", 4},  {"c1l1_spc02_SNG", 4},
                   {"c1l2_theme_SNG", 5},  {"c1l2_spc01_SNG", 5},
                   {"c1l3_spc01_SNG", 6},  {"c1l3_theme_SNG", 6},
                   {"c1l4_spc01_SNG", 7},  {"c1l4_theme_SNG", 7},
                   {"action1_SNG", 7},     {"action1b_SNG", 7},
                   {"action2_SNG", 7},     {"action4_SNG", 7},
                   {"action5_SNG", 7},     {"action6_SNG", 7},
                   {"action7_SNG", 7},     {"c1l5_act01_SNG", 8},
                   {"c1l5_act02_SNG", 8},  {"c1l5_theme_SNG", 8},
                   {"c1l5_spc01_SNG", 8},  {"c2l1_theme_SNG", 9},
                   {"c2l1_spc01_SNG", 9},  {"imperial_SNG", 10},
                   {"c2l2_theme_SNG", 10}, {"c2l2_spc01_SNG", 10},
                   {"c3l5_theme_SNG", 10}, {"c2l3_theme_SNG", 11},
                   {"c2l3_spc01_SNG", 11}, {"c2l5_theme_SNG", 12},
                   {"c2l5_spc01_SNG", 12}, {"c5l1_start_SNG", 12},
                   {"c4l1_theme_SNG", 12}, {"c5l1_1_SNG", 12},
                   {"c5l1_2_SNG", 12},     {"c5l1_3_SNG", 12},
                   {"c3l1_theme_SNG", 13}, {"c5l2_theme_SNG", 13},
                   {"c5l2_spc01_SNG", 13}, {"c5l3_theme_SNG", 13},
                   {"c3l2_theme_SNG", 14}, {"silent01_SNG", 14},
                   {"silent02_SNG", 14},   {"c3l5_spc01_SNG", 14},
                   {"c3l4_theme_SNG", 14}, {"c3l4_spc01_SNG", 14},
                   {"credits_SNG", 20},    {"c1l1_cut1_SNG", 30},
                   {"c1l2_cut1_SNG", 30},  {"c1l3_cut1_SNG", 30},
                   {"c1l4_cut1_SNG", 30},  {"c1l5_cut1_SNG", 30},
                   {"c2l1_cut1_SNG", 30},  {"c2l2_cut1_SNG", 30},
                   {"c2l3_cut1_SNG", 30},  {"c2l5_cut1_SNG", 30},
                   {"c2l6_cut1_SNG", 30},  {"c3l1_cut1_SNG", 30},
                   {"c3l2_cut1_SNG", 30},  {"c3l3_cut1_SNG", 30},
                   {"c3l4_cut1_SNG", 30},  {"c3l5_cut1_SNG", 30},
                   {"c4l1_cut1_SNG", 30},  {"c5l1_cut1_SNG", 30},
                   {"c5l2_cut1_SNG", 30},  {"c5l3_cut1_SNG", 30},
                   {"extr_cut1_SNG", 30},  {"cut_jing1_SNG", 30},
                   {"cut_jing2_SNG", 30},  {"cut_seq1_SNG", 30},
                   {"cut_seq2_SNG", 30},   {"cut_seq3_SNG", 30},
                   {"cut_seq4_SNG", 30},   {"cut_seq5_SNG", 30},
                   {"cut_seq6_SNG", 30},   {"cut_seq7_SNG", 30},
                   {"cut_seq8_SNG", 30},   {}};

static int LookupRS1SongId(const char* name) {
  const RS1SongMapping* map = RS1Mappings;
  while (map->name) {
    if (!strcmp(name, map->name))
      return map->songId;
    ++map;
  }
  return -1;
}

static bool ValidateRS1PC(FILE* fp) {
  size_t endPos = FileLength(fp);
  if (endPos > 100 * 1024 * 1024)
    return false;

  uint32_t fstOff;
  uint32_t fstSz;
  if (fread(&fstOff, 1, 4, fp) == 4 && fread(&fstSz, 1, 4, fp) == 4) {
    if (fstOff + fstSz <= endPos) {
      FSeek(fp, fstOff, SEEK_SET);
      uint32_t elemCount = fstSz / 32;
      std::unique_ptr<RS1FSTEntry[]> entries(new RS1FSTEntry[elemCount]);
      fread(entries.get(), fstSz, 1, fp);

      uint8_t foundComps = 0;
      for (uint32_t i = 0; i < elemCount; ++i) {
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

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadRS1PC(FILE* fp) {
  std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
  size_t endPos = FileLength(fp);

  uint32_t fstOff;
  uint32_t fstSz;
  if (fread(&fstOff, 1, 4, fp) == 4 && fread(&fstSz, 1, 4, fp) == 4) {
    if (fstOff + fstSz <= endPos) {
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

      for (uint32_t i = 0; i < elemCount; ++i) {
        RS1FSTEntry& entry = entries[i];
        if (!strncmp("proj_SND", entry.name, 16)) {
          projSz = entry.decompSz;
          proj.reset(new uint8_t[entry.decompSz]);
          FSeek(fp, entry.offset, SEEK_SET);
          fread(proj.get(), 1, entry.decompSz, fp);
        } else if (!strncmp("pool_SND", entry.name, 16)) {
          poolSz = entry.decompSz;
          pool.reset(new uint8_t[entry.decompSz]);
          FSeek(fp, entry.offset, SEEK_SET);
          fread(pool.get(), 1, entry.decompSz, fp);
        } else if (!strncmp("sdir_SND", entry.name, 16)) {
          sdirSz = entry.decompSz;
          sdir.reset(new uint8_t[entry.decompSz]);
          FSeek(fp, entry.offset, SEEK_SET);
          fread(sdir.get(), 1, entry.decompSz, fp);
        } else if (!strncmp("samp_SND", entry.name, 16)) {
          sampSz = entry.decompSz;
          samp.reset(new uint8_t[entry.decompSz]);
          FSeek(fp, entry.offset, SEEK_SET);
          fread(samp.get(), 1, entry.decompSz, fp);
        }
      }

      ret.emplace_back("Group",
                       IntrusiveAudioGroupData{proj.release(), projSz, pool.release(), poolSz, sdir.release(), sdirSz,
                                               samp.release(), sampSz, false, PCDataTag{}});
    }
  }

  return ret;
}

static std::vector<std::pair<std::string, ContainerRegistry::SongData>> LoadRS1PCSongs(FILE* fp) {
  std::vector<std::pair<std::string, ContainerRegistry::SongData>> ret;
  size_t endPos = FileLength(fp);

  uint32_t fstOff;
  uint32_t fstSz;
  if (fread(&fstOff, 1, 4, fp) == 4 && fread(&fstSz, 1, 4, fp) == 4) {
    if (fstOff + fstSz <= endPos) {
      FSeek(fp, fstOff, SEEK_SET);
      uint32_t elemCount = fstSz / 32;
      std::unique_ptr<RS1FSTEntry[]> entries(new RS1FSTEntry[elemCount]);
      fread(entries.get(), fstSz, 1, fp);

      for (uint32_t i = 0; i < elemCount; ++i) {
        RS1FSTEntry& entry = entries[i];
        if (strstr(entry.name, "SNG")) {
          std::unique_ptr<uint8_t[]> song(new uint8_t[entry.decompSz]);
          FSeek(fp, entry.offset, SEEK_SET);
          fread(song.get(), 1, entry.decompSz, fp);

          std::string name = StrToSys(entry.name);
          ret.emplace_back(
              name, ContainerRegistry::SongData(std::move(song), entry.decompSz, -1, LookupRS1SongId(entry.name)));
        }
      }
    }
  }

  return ret;
}

static bool ValidateRS1N64(FILE* fp) {
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

  const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos, "dbg_data\0\0\0\0\0\0\0\0", 16));
  if (dataSeg) {
    dataSeg += 28;
    size_t fstEnd = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
    dataSeg += 4;
    size_t fstOff = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
    if (endPos <= size_t(dataSeg - data.get()) + fstOff || endPos <= size_t(dataSeg - data.get()) + fstEnd)
      return false;

    const RS1FSTEntry* entry = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstOff);
    const RS1FSTEntry* lastEnt = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstEnd);

    uint8_t foundComps = 0;
    for (; entry != lastEnt; ++entry) {
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

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadRS1N64(FILE* fp) {
  std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
  size_t endPos = FileLength(fp);

  std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
  fread(data.get(), 1, endPos, fp);

  if ((data[0] & 0x80) != 0x80 && (data[3] & 0x80) == 0x80)
    SwapN64Rom32(data.get(), endPos);
  else if ((data[0] & 0x80) != 0x80 && (data[1] & 0x80) == 0x80)
    SwapN64Rom16(data.get(), endPos);

  const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos, "dbg_data\0\0\0\0\0\0\0\0", 16));
  if (dataSeg) {
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

    for (; entry != lastEnt; ++entry) {
      RS1FSTEntry ent = *entry;
      ent.swapBig();

      if (!strncmp("proj_SND", ent.name, 16)) {
        if (ent.compSz == 0xffffffff) {
          proj.reset(new uint8_t[ent.decompSz]);
          memmove(proj.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          proj.reset(new uint8_t[ent.decompSz]);
          uLongf outSz = ent.decompSz;
          uncompress(proj.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }
        projSz = ent.decompSz;
      } else if (!strncmp("pool_SND", ent.name, 16)) {
        if (ent.compSz == 0xffffffff) {
          pool.reset(new uint8_t[ent.decompSz]);
          memmove(pool.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          pool.reset(new uint8_t[ent.decompSz]);
          uLongf outSz = ent.decompSz;
          uncompress(pool.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }
        poolSz = ent.decompSz;
      } else if (!strncmp("sdir_SND", ent.name, 16)) {
        if (ent.compSz == 0xffffffff) {
          sdir.reset(new uint8_t[ent.decompSz]);
          memmove(sdir.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          sdir.reset(new uint8_t[ent.decompSz]);
          uLongf outSz = ent.decompSz;
          uncompress(sdir.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }
        sdirSz = ent.decompSz;
      } else if (!strncmp("samp_SND", ent.name, 16)) {
        if (ent.compSz == 0xffffffff) {
          samp.reset(new uint8_t[ent.decompSz]);
          memmove(samp.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          samp.reset(new uint8_t[ent.decompSz]);
          uLongf outSz = ent.decompSz;
          uncompress(samp.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }
        sampSz = ent.decompSz;
      }
    }

    ret.emplace_back("Group",
                     IntrusiveAudioGroupData{proj.release(), projSz, pool.release(), poolSz, sdir.release(), sdirSz,
                                             samp.release(), sampSz, false, N64DataTag{}});
  }

  return ret;
}

static std::vector<std::pair<std::string, ContainerRegistry::SongData>> LoadRS1N64Songs(FILE* fp) {
  std::vector<std::pair<std::string, ContainerRegistry::SongData>> ret;
  size_t endPos = FileLength(fp);

  std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
  fread(data.get(), 1, endPos, fp);

  if ((data[0] & 0x80) != 0x80 && (data[3] & 0x80) == 0x80)
    SwapN64Rom32(data.get(), endPos);
  else if ((data[0] & 0x80) != 0x80 && (data[1] & 0x80) == 0x80)
    SwapN64Rom16(data.get(), endPos);

  const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos, "dbg_data\0\0\0\0\0\0\0\0", 16));
  if (dataSeg) {
    dataSeg += 28;
    size_t fstEnd = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
    dataSeg += 4;
    size_t fstOff = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
    if (endPos <= size_t(dataSeg - data.get()) + fstOff || endPos <= size_t(dataSeg - data.get()) + fstEnd)
      return ret;

    const RS1FSTEntry* entry = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstOff);
    const RS1FSTEntry* lastEnt = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstEnd);

    for (; entry != lastEnt; ++entry) {
      RS1FSTEntry ent = *entry;
      ent.swapBig();

      if (strstr(ent.name, "SNG")) {
        std::unique_ptr<uint8_t[]> song(new uint8_t[ent.decompSz]);

        if (ent.compSz == 0xffffffff) {
          memmove(song.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          uLongf outSz = ent.decompSz;
          uncompress(song.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }

        std::string name = StrToSys(ent.name);
        ret.emplace_back(name,
                         ContainerRegistry::SongData(std::move(song), ent.decompSz, -1, LookupRS1SongId(ent.name)));
      }
    }
  }

  return ret;
}

static bool ValidateFactor5N64Rev(FILE* fp) {
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

  const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos, "dbg_data\0\0\0\0\0\0\0\0", 16));
  if (dataSeg) {
    dataSeg += 28;
    size_t fstEnd = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
    dataSeg += 4;
    size_t fstOff = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
    if (endPos <= size_t(dataSeg - data.get()) + fstOff || endPos <= size_t(dataSeg - data.get()) + fstEnd)
      return false;

    const RS1FSTEntry* entry = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstOff);
    const RS1FSTEntry* lastEnt = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstEnd);

    uint8_t foundComps = 0;
    for (; entry != lastEnt; ++entry) {
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

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadFactor5N64Rev(FILE* fp) {
  std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;
  size_t endPos = FileLength(fp);

  std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
  fread(data.get(), 1, endPos, fp);

  if ((data[0] & 0x80) != 0x80 && (data[3] & 0x80) == 0x80)
    SwapN64Rom32(data.get(), endPos);
  else if ((data[0] & 0x80) != 0x80 && (data[1] & 0x80) == 0x80)
    SwapN64Rom16(data.get(), endPos);

  const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos, "dbg_data\0\0\0\0\0\0\0\0", 16));
  if (dataSeg) {
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

    for (; entry != lastEnt; ++entry) {
      RS1FSTEntry ent = *entry;
      ent.swapBig();

      if (!strncmp("proj", ent.name, 16)) {
        if (ent.compSz == 0xffffffff) {
          proj.reset(new uint8_t[ent.decompSz]);
          memmove(proj.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          proj.reset(new uint8_t[ent.decompSz]);
          uLongf outSz = ent.decompSz;
          uncompress(proj.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }
        projSz = ent.decompSz;
      } else if (!strncmp("pool", ent.name, 16)) {
        if (ent.compSz == 0xffffffff) {
          pool.reset(new uint8_t[ent.decompSz]);
          memmove(pool.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          pool.reset(new uint8_t[ent.decompSz]);
          uLongf outSz = ent.decompSz;
          uncompress(pool.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }
        poolSz = ent.decompSz;
      } else if (!strncmp("sdir", ent.name, 16)) {
        if (ent.compSz == 0xffffffff) {
          sdir.reset(new uint8_t[ent.decompSz]);
          memmove(sdir.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          sdir.reset(new uint8_t[ent.decompSz]);
          uLongf outSz = ent.decompSz;
          uncompress(sdir.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }
        sdirSz = ent.decompSz;
      } else if (!strncmp("samp", ent.name, 16)) {
        if (ent.compSz == 0xffffffff) {
          samp.reset(new uint8_t[ent.decompSz]);
          memmove(samp.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          samp.reset(new uint8_t[ent.decompSz]);
          uLongf outSz = ent.decompSz;
          uncompress(samp.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }
        sampSz = ent.decompSz;
      }
    }

    ret.emplace_back("Group",
                     IntrusiveAudioGroupData{proj.release(), projSz, pool.release(), poolSz, sdir.release(), sdirSz,
                                             samp.release(), sampSz, true, N64DataTag{}});
  }

  return ret;
}

static std::vector<std::pair<std::string, ContainerRegistry::SongData>> LoadFactor5N64RevSongs(FILE* fp) {
  std::vector<std::pair<std::string, ContainerRegistry::SongData>> ret;
  size_t endPos = FileLength(fp);

  std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
  fread(data.get(), 1, endPos, fp);

  if ((data[0] & 0x80) != 0x80 && (data[3] & 0x80) == 0x80)
    SwapN64Rom32(data.get(), endPos);
  else if ((data[0] & 0x80) != 0x80 && (data[1] & 0x80) == 0x80)
    SwapN64Rom16(data.get(), endPos);

  const uint8_t* dataSeg = reinterpret_cast<const uint8_t*>(memmem(data.get(), endPos, "dbg_data\0\0\0\0\0\0\0\0", 16));
  if (dataSeg) {
    dataSeg += 28;
    size_t fstEnd = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
    dataSeg += 4;
    size_t fstOff = SBig(*reinterpret_cast<const uint32_t*>(dataSeg));
    if (endPos <= size_t(dataSeg - data.get()) + fstOff || endPos <= size_t(dataSeg - data.get()) + fstEnd)
      return ret;

    const RS1FSTEntry* entry = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstOff);
    const RS1FSTEntry* lastEnt = reinterpret_cast<const RS1FSTEntry*>(dataSeg + fstEnd);

    for (; entry != lastEnt; ++entry) {
      RS1FSTEntry ent = *entry;
      ent.swapBig();

      if (!strncmp(ent.name, "s_", 2)) {
        long idx = strtol(ent.name + 2, nullptr, 10);

        std::unique_ptr<uint8_t[]> song(new uint8_t[ent.decompSz]);

        if (ent.compSz == 0xffffffff) {
          memmove(song.get(), dataSeg + ent.offset, ent.decompSz);
        } else {
          uLongf outSz = ent.decompSz;
          uncompress(song.get(), &outSz, dataSeg + ent.offset, ent.compSz);
        }

        std::string name = StrToSys(ent.name);
        ret.emplace_back(name, ContainerRegistry::SongData(std::move(song), ent.decompSz, -1, idx));
      }
    }
  }

  return ret;
}

struct RS2FSTEntry {
  uint64_t offset;
  uint64_t decompSz;
  uint64_t compSz;
  uint64_t type;
  char name[32];

  void swapBig() {
    offset = SBig(offset);
    decompSz = SBig(decompSz);
    compSz = SBig(compSz);
    type = SBig(type);
  }
};

struct RS23GroupHead {
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

  void swapBig() {
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

struct RS23SONHead {
  uint32_t offset;
  uint32_t length;
  uint16_t groupId;
  uint16_t setupId;

  void swapBig() {
    offset = SBig(offset);
    length = SBig(length);
    groupId = SBig(groupId);
    setupId = SBig(setupId);
  }
};

static bool ValidateRS2(FILE* fp) {
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
  for (size_t i = 0; i < fstSz / 64; ++i) {
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

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadRS2(FILE* fp) {
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
  for (size_t i = 0; i < fstSz / 64; ++i) {
    RS2FSTEntry entry;
    fread(&entry, 1, 64, fp);
    entry.swapBig();
    if (!strncmp("data", entry.name, 32)) {
      FSeek(fp, int64_t(entry.offset), SEEK_SET);
      std::unique_ptr<uint8_t[]> audData(new uint8_t[entry.decompSz]);
      fread(audData.get(), 1, entry.decompSz, fp);

      uint32_t indexOff = SBig(*reinterpret_cast<uint32_t*>(audData.get() + 4));
      uint32_t groupCount = SBig(*reinterpret_cast<uint32_t*>(audData.get() + indexOff));
      const uint32_t* groupOffs = reinterpret_cast<const uint32_t*>(audData.get() + indexOff + 4);

      for (uint32_t j = 0; j < groupCount; ++j) {
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

        if (head.projLen && head.poolLen && head.sdirLen && head.sampLen) {
          std::string name = fmt::format(FMT_STRING("GroupFile{:02d}"), j);
          ret.emplace_back(std::move(name),
              IntrusiveAudioGroupData{proj.release(), head.projLen, pool.release(), head.poolLen,
                                      sdir.release(), head.sdirLen, samp.release(), head.sampLen,
                                      GCNDataTag{}});
        }
      }

      break;
    }
  }

  return ret;
}

static std::vector<std::pair<std::string, ContainerRegistry::SongData>> LoadRS2Songs(FILE* fp) {
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
  for (size_t i = 0; i < fstSz / 64; ++i) {
    RS2FSTEntry entry;
    fread(&entry, 1, 64, fp);
    entry.swapBig();
    if (!strncmp("data", entry.name, 32)) {
      FSeek(fp, int64_t(entry.offset), SEEK_SET);
      std::unique_ptr<uint8_t[]> audData(new uint8_t[entry.decompSz]);
      fread(audData.get(), 1, entry.decompSz, fp);

      uint32_t indexOff = SBig(*reinterpret_cast<uint32_t*>(audData.get() + 4));
      uint32_t groupCount = SBig(*reinterpret_cast<uint32_t*>(audData.get() + indexOff));
      const uint32_t* groupOffs = reinterpret_cast<const uint32_t*>(audData.get() + indexOff + 4);

      for (uint32_t j = 0; j < groupCount; ++j) {
        const uint8_t* groupData = audData.get() + SBig(groupOffs[j]);
        RS23GroupHead head = *reinterpret_cast<const RS23GroupHead*>(groupData);
        head.swapBig();

        if (!head.sonCount)
          continue;

        const RS23SONHead* sonData = reinterpret_cast<const RS23SONHead*>(audData.get() + head.sonIdxBeginOff);
        for (uint32_t s = 0; s < head.sonCount; ++s) {
          RS23SONHead sonHead = sonData[s];
          sonHead.swapBig();

          std::string name = fmt::format(FMT_STRING("GroupFile{:02d}-{}"), j, s);
          std::unique_ptr<uint8_t[]> song(new uint8_t[sonHead.length]);
          memmove(song.get(), audData.get() + sonHead.offset, sonHead.length);
          ret.emplace_back(std::move(name),
              ContainerRegistry::SongData(std::move(song), sonHead.length, sonHead.groupId, sonHead.setupId));
        }
      }

      break;
    }
  }

  return ret;
}

struct RS3FSTEntry {
  uint64_t offset;
  uint64_t decompSz;
  uint64_t compSz;
  uint64_t type;
  char name[128];

  void swapBig() {
    offset = SBig(offset);
    decompSz = SBig(decompSz);
    compSz = SBig(compSz);
    type = SBig(type);
  }
};

static bool ValidateRS3(FILE* fp) {
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
  for (size_t i = 0; i < fstSz / 160; ++i) {
    RS3FSTEntry entry;
    fread(&entry, 1, 160, fp);
    entry.swapBig();
    if (!strncmp("data", entry.name, 128))
      return true;
  }

  return false;
}

static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadRS3(FILE* fp) {
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
  for (size_t i = 0; i < fstSz / 160; ++i) {
    RS3FSTEntry entry;
    fread(&entry, 1, 160, fp);
    entry.swapBig();
    if (!strncmp("data", entry.name, 128)) {
      FSeek(fp, int64_t(entry.offset), SEEK_SET);
      std::unique_ptr<uint8_t[]> audData(new uint8_t[entry.decompSz]);
      fread(audData.get(), 1, entry.decompSz, fp);

      uint32_t indexOff = SBig(*reinterpret_cast<uint32_t*>(audData.get() + 4));
      uint32_t groupCount = SBig(*reinterpret_cast<uint32_t*>(audData.get() + indexOff));
      const uint32_t* groupOffs = reinterpret_cast<const uint32_t*>(audData.get() + indexOff + 4);

      for (uint32_t j = 0; j < groupCount; ++j) {
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

        if (head.projLen && head.poolLen && head.sdirLen && head.sampLen) {
          std::string name = fmt::format(FMT_STRING("GroupFile{:02d}"), j);
          ret.emplace_back(std::move(name),
              IntrusiveAudioGroupData{proj.release(), head.projLen, pool.release(), head.poolLen,
                                      sdir.release(), head.sdirLen, samp.release(), head.sampLen,
                                      GCNDataTag{}});
        }
      }

      break;
    }
  }

  return ret;
}

static bool ValidateStarFoxAdvSongs(FILE* fp) {
  size_t endPos = FileLength(fp);
  if (endPos > 2 * 1024 * 1024)
    return false;

  std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
  fread(data.get(), 1, endPos, fp);

  const uint32_t* lengths = reinterpret_cast<const uint32_t*>(data.get());
  size_t totalLen = 0;
  int i = 0;
  for (; i < 128; ++i) {
    uint32_t len = SBig(lengths[i]);
    if (len == 0)
      break;
    totalLen += len;
    totalLen = ((totalLen + 31) & ~31);
  }
  totalLen += (((i * 4) + 31) & ~31);

  return totalLen == endPos;
}

static std::vector<std::pair<std::string, ContainerRegistry::SongData>> LoadStarFoxAdvSongs(FILE* midifp) {
  std::vector<std::pair<std::string, ContainerRegistry::SongData>> ret;

  size_t endPos = FileLength(midifp);
  if (endPos > 2 * 1024 * 1024)
    return {};

  std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
  fread(data.get(), 1, endPos, midifp);

  const uint32_t* lengths = reinterpret_cast<const uint32_t*>(data.get());
  size_t i = 0;
  for (; i < 128; ++i) {
    uint32_t len = SBig(lengths[i]);
    if (len == 0)
      break;
  }

  size_t sngCount = i;
  size_t cur = (((sngCount * 4) + 31) & ~31);
  for (i = 0; i < sngCount; ++i) {
    uint32_t len = SBig(lengths[i]);
    if (len == 0)
      break;

    std::string name = fmt::format(FMT_STRING("Song{}"), unsigned(i));
    std::unique_ptr<uint8_t[]> song(new uint8_t[len]);
    memmove(song.get(), data.get() + cur, len);
    ret.emplace_back(std::move(name), ContainerRegistry::SongData(std::move(song), len, -1, i));

    cur += len;
    cur = ((cur + 31) & ~31);
  }

  return ret;
}

static bool ValidatePaperMarioTTYDSongs(FILE* fp) {
  size_t endPos = FileLength(fp);
  std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
  fread(data.get(), 1, endPos, fp);
  uint32_t off = 0;
  while (off < endPos) {
    int32_t len = SBig(*(reinterpret_cast<int32_t*>(data.get() + off)));
    if (len == -1)
      break;
    off += len;
  }
  return (off + 4) == endPos;
}

struct TTYDSongDesc {
  char name[30];
  uint8_t group;
  uint8_t setup;
};

static std::vector<std::pair<std::string, ContainerRegistry::SongData>> LoadPaperMarioTTYDSongs(FILE* midifp,
                                                                                                 FILE* descFp) {
  if (!descFp)
    return {};

  std::vector<TTYDSongDesc> songDescs;
  /* We need at least 143 for the default song table */
  songDescs.reserve(143);

  while (true) {
    TTYDSongDesc songDesc;
    fread(&songDesc, sizeof(TTYDSongDesc), 1, descFp);
    if (songDesc.name[0] == 0)
      break;

    songDescs.push_back(songDesc);
  }
  size_t endPos = FileLength(midifp);
  std::unique_ptr<uint8_t[]> data(new uint8_t[endPos]);
  fread(data.get(), 1, endPos, midifp);
  uint32_t off = 0;
  uint32_t song = 0;
  std::vector<std::pair<std::string, ContainerRegistry::SongData>> ret;

  while (off < endPos) {
    int32_t len = SBig(*(reinterpret_cast<int32_t*>(data.get() + off)));
    if (len == -1)
      break;

    std::unique_ptr<uint8_t[]> songData(new uint8_t[len - 32]);
    memcpy(songData.get(), (data.get() + off + 32), len - 32);
    ret.emplace_back(
        StrToSys(std::string(songDescs[song].name, 30)),
        ContainerRegistry::SongData(std::move(songData), len - 32, songDescs[song].group, songDescs[song].setup));
    off += len;
    song++;
  }

  return ret;
}

ContainerRegistry::Type ContainerRegistry::DetectContainerType(const char* path) {
  FILE* fp;

  /* See if provided file is one of four raw chunks */
  const char* dot = nullptr;
  if (IsChunkExtension(path, dot)) {
    std::string newpath;
    
    /* Project */
    newpath = fmt::format(FMT_STRING("{:.{}}.pro"), path, int(dot - path));
    fp = FOpen(newpath.c_str(), "rb");
    if (!fp) {
      newpath = fmt::format(FMT_STRING("{:.{}}.proj"), path, int(dot - path));
      fp = FOpen(newpath.c_str(), "rb");
      if (!fp)
        return Type::Invalid;
    }
    fclose(fp);

    /* Pool */
    newpath = fmt::format(FMT_STRING("{:.{}}.poo"), path, int(dot - path));
    fp = FOpen(newpath.c_str(), "rb");
    if (!fp) {
      newpath = fmt::format(FMT_STRING("{:.{}}.pool"), path, int(dot - path));
      fp = FOpen(newpath.c_str(), "rb");
      if (!fp)
        return Type::Invalid;
    }
    fclose(fp);

    /* Sample Directory */
    newpath = fmt::format(FMT_STRING("{:.{}}.sdi"), path, int(dot - path));
    fp = FOpen(newpath.c_str(), "rb");
    if (!fp) {
      newpath = fmt::format(FMT_STRING("{:.{}}.sdir"), path, int(dot - path));
      fp = FOpen(newpath.c_str(), "rb");
      if (!fp)
        return Type::Invalid;
    }
    fclose(fp);

    /* Sample */
    newpath = fmt::format(FMT_STRING("{:.{}}.sam"), path, int(dot - path));
    fp = FOpen(newpath.c_str(), "rb");
    if (!fp) {
      newpath = fmt::format(FMT_STRING("{:.{}}.samp"), path, int(dot - path));
      fp = FOpen(newpath.c_str(), "rb");
      if (!fp)
        return Type::Invalid;
    }
    fclose(fp);

    return Type::Raw4;
  }

  /* Now attempt single-file case */
  fp = FOpen(path, "rb");
  if (fp) {
    if (ValidateMP1(fp)) {
      fclose(fp);
      return Type::MetroidPrime;
    }

    if (ValidateMP2(fp)) {
      fclose(fp);
      return Type::MetroidPrime2;
    }

    if (ValidateRS1PC(fp)) {
      fclose(fp);
      return Type::RogueSquadronPC;
    }

    if (ValidateRS1N64(fp)) {
      fclose(fp);
      return Type::RogueSquadronN64;
    }

    if (ValidateFactor5N64Rev(fp)) {
      fclose(fp);
      return Type::Factor5N64Rev;
    }

    if (ValidateRS2(fp)) {
      fclose(fp);
      return Type::RogueSquadron2;
    }

    if (ValidateRS3(fp)) {
      fclose(fp);
      return Type::RogueSquadron3;
    }

    fclose(fp);
  }

  return Type::Invalid;
}

std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ContainerRegistry::LoadContainer(const char* path) {
  Type typeOut;
  return LoadContainer(path, typeOut);
};

std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ContainerRegistry::LoadContainer(const char* path,
                                                                                               Type& typeOut) {
  FILE* fp;
  typeOut = Type::Invalid;

  /* See if provided file is one of four raw chunks */
  const char* dot = nullptr;
  if (IsChunkExtension(path, dot)) {
    std::vector<std::pair<std::string, IntrusiveAudioGroupData>> ret;

    std::string baseName;
    if (const char* sep = std::max(StrRChr(path, '/'), StrRChr(path, '\\')))
      baseName = std::string(sep + 1, dot - sep - 1);
    else
      baseName = std::string(path, dot - path);

    /* Project */
    std::string projPath;
    projPath = fmt::format(FMT_STRING("{:.{}}.pro"), path, int(dot - path));
    fp = FOpen(projPath.c_str(), "rb");
    if (!fp) {
      projPath = fmt::format(FMT_STRING("{:.{}}.proj"), path, int(dot - path));
      fp = FOpen(projPath.c_str(), "rb");
      if (!fp)
        return ret;
    }
    fclose(fp);

    /* Pool */
    std::string poolPath;
    poolPath = fmt::format(FMT_STRING("{:.{}}.poo"), path, int(dot - path));
    fp = FOpen(poolPath.c_str(), "rb");
    if (!fp) {
      poolPath = fmt::format(FMT_STRING("{:.{}}.pool"), path, int(dot - path));
      fp = FOpen(poolPath.c_str(), "rb");
      if (!fp)
        return ret;
    }
    fclose(fp);

    /* Sample Directory */
    std::string sdirPath;
    sdirPath = fmt::format(FMT_STRING("{:.{}}.sdi"), path, int(dot - path));
    fp = FOpen(sdirPath.c_str(), "rb");
    if (!fp) {
      sdirPath = fmt::format(FMT_STRING("{:.{}}.sdir"), path, int(dot - path));
      fp = FOpen(sdirPath.c_str(), "rb");
      if (!fp)
        return ret;
    }
    fclose(fp);

    /* Sample */
    std::string sampPath;
    sampPath = fmt::format(FMT_STRING("{:.{}}.sam"), path, int(dot - path));
    fp = FOpen(sampPath.c_str(), "rb");
    if (!fp) {
      sampPath = fmt::format(FMT_STRING("{:.{}}.samp"), path, int(dot - path));
      fp = FOpen(sampPath.c_str(), "rb");
      if (!fp)
        return ret;
    }
    fclose(fp);

    fp = FOpen(projPath.c_str(), "rb");
    size_t projLen = FileLength(fp);
    if (!projLen)
      return ret;
    std::unique_ptr<uint8_t[]> proj(new uint8_t[projLen]);
    fread(proj.get(), 1, projLen, fp);

    fp = FOpen(poolPath.c_str(), "rb");
    size_t poolLen = FileLength(fp);
    if (!poolLen)
      return ret;
    std::unique_ptr<uint8_t[]> pool(new uint8_t[poolLen]);
    fread(pool.get(), 1, poolLen, fp);

    fp = FOpen(sdirPath.c_str(), "rb");
    size_t sdirLen = FileLength(fp);
    if (!sdirLen)
      return ret;
    std::unique_ptr<uint8_t[]> sdir(new uint8_t[sdirLen]);
    fread(sdir.get(), 1, sdirLen, fp);

    fp = FOpen(sampPath.c_str(), "rb");
    size_t sampLen = FileLength(fp);
    if (!sampLen)
      return ret;
    std::unique_ptr<uint8_t[]> samp(new uint8_t[sampLen]);
    fread(samp.get(), 1, sampLen, fp);

    fclose(fp);

    /* SDIR-based format detection */
    if (*reinterpret_cast<uint32_t*>(sdir.get() + 8) == 0x0)
      ret.emplace_back(baseName,
                       IntrusiveAudioGroupData{proj.release(), projLen, pool.release(), poolLen, sdir.release(),
                                               sdirLen, samp.release(), sampLen, GCNDataTag{}});
    else if (sdir[9] == 0x0)
      ret.emplace_back(baseName,
                       IntrusiveAudioGroupData{proj.release(), projLen, pool.release(), poolLen, sdir.release(),
                                               sdirLen, samp.release(), sampLen, false, N64DataTag{}});
    else
      ret.emplace_back(baseName,
                       IntrusiveAudioGroupData{proj.release(), projLen, pool.release(), poolLen, sdir.release(),
                                               sdirLen, samp.release(), sampLen, false, PCDataTag{}});

    typeOut = Type::Raw4;
    return ret;
  }

  /* Now attempt single-file case */
  fp = FOpen(path, "rb");
  if (fp) {
    if (ValidateMP1(fp)) {
      auto ret = LoadMP1(fp);
      fclose(fp);
      typeOut = Type::MetroidPrime;
      return ret;
    }

    if (ValidateMP2(fp)) {
      auto ret = LoadMP2(fp);
      fclose(fp);
      typeOut = Type::MetroidPrime2;
      return ret;
    }

    if (ValidateRS1PC(fp)) {
      auto ret = LoadRS1PC(fp);
      fclose(fp);
      typeOut = Type::RogueSquadronPC;
      return ret;
    }

    if (ValidateRS1N64(fp)) {
      auto ret = LoadRS1N64(fp);
      fclose(fp);
      typeOut = Type::RogueSquadronN64;
      return ret;
    }

    if (ValidateFactor5N64Rev(fp)) {
      auto ret = LoadFactor5N64Rev(fp);
      fclose(fp);
      typeOut = Type::Factor5N64Rev;
      return ret;
    }

    if (ValidateRS2(fp)) {
      auto ret = LoadRS2(fp);
      fclose(fp);
      typeOut = Type::RogueSquadron2;
      return ret;
    }

    if (ValidateRS3(fp)) {
      auto ret = LoadRS3(fp);
      fclose(fp);
      typeOut = Type::RogueSquadron3;
      return ret;
    }

    fclose(fp);
  }

  return {};
}

std::vector<std::pair<std::string, ContainerRegistry::SongData>> ContainerRegistry::LoadSongs(const char* path) {
  FILE* fp;

  /* See if provided file is a raw song */
  const char* dot = nullptr;
  if (IsSongExtension(path, dot)) {
    fp = FOpen(path, "rb");
    size_t fLen = FileLength(fp);
    if (!fLen) {
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
  fp = FOpen(path, "rb");
  if (fp) {
    if (ValidateMP1Songs(fp)) {
      auto ret = LoadMP1Songs(fp);
      fclose(fp);
      return ret;
    }

    if (ValidateRS1PC(fp)) {
      auto ret = LoadRS1PCSongs(fp);
      fclose(fp);
      return ret;
    }

    if (ValidateRS1N64(fp)) {
      auto ret = LoadRS1N64Songs(fp);
      fclose(fp);
      return ret;
    }

    if (ValidateFactor5N64Rev(fp)) {
      auto ret = LoadFactor5N64RevSongs(fp);
      fclose(fp);
      return ret;
    }

    if (ValidateRS2(fp)) {
      auto ret = LoadRS2Songs(fp);
      fclose(fp);
      return ret;
    }

    if (ValidateStarFoxAdvSongs(fp)) {
      auto ret = LoadStarFoxAdvSongs(fp);
      fclose(fp);
      return ret;
    }

    if (ValidatePaperMarioTTYDSongs(fp)) {
      /* Song Description */
      dot = StrRChr(path, '.');
      std::string newpath = fmt::format(FMT_STRING("{:.{}}.stbl"), path, int(dot - path));
      FILE* descFp = FOpen(newpath.c_str(), "rb");
      if (descFp) {
        auto ret = LoadPaperMarioTTYDSongs(fp, descFp);
        fclose(fp);
        fclose(descFp);
        return ret;
      }
    }

    fclose(fp);
  }

  return {};
}
} // namespace amuse

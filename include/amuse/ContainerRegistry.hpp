#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

#include "amuse/AudioGroupData.hpp"
#include "amuse/Common.hpp"

namespace amuse {

class ContainerRegistry {
public:
  enum class Type {
    Invalid = -1,
    Raw4 = 0,
    MetroidPrime,
    MetroidPrime2,
    RogueSquadronPC,
    RogueSquadronN64,
    Factor5N64Rev,
    RogueSquadron2,
    RogueSquadron3
  };
  struct SongData {
    std::unique_ptr<uint8_t[]> m_data;
    size_t m_size;
    int16_t m_groupId;
    int16_t m_setupId;
    SongData(std::unique_ptr<uint8_t[]>&& data, size_t size, int16_t groupId, int16_t setupId)
    : m_data(std::move(data)), m_size(size), m_groupId(groupId), m_setupId(setupId) {}
  };
  static const char* TypeToName(Type tp);
  static Type DetectContainerType(const char* path);
  static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadContainer(const char* path);
  static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadContainer(const char* path,
                                                                                     Type& typeOut);
  static std::vector<std::pair<std::string, SongData>> LoadSongs(const char* path);
};
} // namespace amuse

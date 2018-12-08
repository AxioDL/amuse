#pragma once

#include <vector>
#include <cstdint>

namespace amuse {

class SongConverter {
public:
  static std::vector<uint8_t> SongToMIDI(const unsigned char* data, int& versionOut, bool& isBig);
  static std::vector<uint8_t> MIDIToSong(const std::vector<uint8_t>& data, int version, bool big);
};
} // namespace amuse

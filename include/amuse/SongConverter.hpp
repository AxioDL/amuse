#ifndef __AMUSE_SONGCONVERTER_HPP__
#define __AMUSE_SONGCONVERTER_HPP__

#include <vector>
#include <stdint.h>

namespace amuse
{

class SongConverter
{
public:
    static std::vector<uint8_t> SongToMIDI(const unsigned char* data, int& versionOut, bool& isBig);
    static std::vector<uint8_t> MIDIToSong(const std::vector<uint8_t>& data, int version, bool big);
};

}

#endif // __AMUSE_SONGCONVERTER_HPP__

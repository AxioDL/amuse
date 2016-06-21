#ifndef __AMUSE_SONGCONVERTER_HPP__
#define __AMUSE_SONGCONVERTER_HPP__

#include <vector>
#include <stdint.h>

namespace amuse
{

class SongConverter
{
public:
    enum class Target
    {
        N64,
        GCN,
        PC
    };
    static std::vector<uint8_t> SongToMIDI(const unsigned char* data, Target& targetOut);
    static std::vector<uint8_t> MIDIToSong(const unsigned char* data, Target target);
};

}

#endif // __AMUSE_SONGCONVERTER_HPP__

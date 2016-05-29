#ifndef __AMUSE_CONTAINERREGISTRY_HPP__
#define __AMUSE_CONTAINERREGISTRY_HPP__

#include "AudioGroupData.hpp"
#include <string>
#include <vector>

namespace amuse
{

class ContainerRegistry
{
public:
    enum class Type
    {
        Invalid = -1,
        Raw4 = 0,
        MetroidPrime,
        MetroidPrime2,
        RogueSquadronPC,
        RogueSquadronN64,
        BattleForNabooPC,
        BattleForNabooN64,
        RogueSquadron2,
        RogueSquadron3
    };
    static const char* TypeToName(Type tp);
    static Type DetectContainerType(const char* path);
    static std::vector<std::pair<std::string, IntrusiveAudioGroupData>> LoadContainer(const char* path);
};

}

#endif // __AMUSE_CONTAINERREGISTRY_HPP__

#ifndef __AMUSE_CONTAINERREGISTRY_HPP__
#define __AMUSE_CONTAINERREGISTRY_HPP__

#include "AudioGroupData.hpp"
#include "Common.hpp"
#include <string>
#include <vector>
#include <memory>

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
    struct SongData
    {
        std::unique_ptr<uint8_t[]> m_data;
        size_t m_size;
        int16_t m_groupId;
        int16_t m_setupId;
        SongData(std::unique_ptr<uint8_t[]>&& data, size_t size, int16_t groupId, int16_t setupId)
        : m_data(std::move(data)), m_size(size), m_groupId(groupId), m_setupId(setupId)
        {
        }
    };
    static const SystemChar* TypeToName(Type tp);
    static Type DetectContainerType(const SystemChar* path);
    static std::vector<std::pair<SystemString, IntrusiveAudioGroupData>> LoadContainer(const SystemChar* path);
    static std::vector<std::pair<SystemString, IntrusiveAudioGroupData>> LoadContainer(const SystemChar* path,
                                                                                       Type& typeOut);
    static std::vector<std::pair<SystemString, SongData>> LoadSongs(const SystemChar* path);
};
}

#endif // __AMUSE_CONTAINERREGISTRY_HPP__

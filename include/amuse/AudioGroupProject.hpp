#ifndef __AMUSE_AUDIOGROUPPROJECT_HPP__
#define __AMUSE_AUDIOGROUPPROJECT_HPP__

#include "Entity.hpp"
#include "Common.hpp"
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>

namespace amuse
{
class AudioGroupData;

/** Common index members of SongGroups and SFXGroups */
struct AudioGroupIndex
{
    const uint16_t* m_soundMacroIndex;
    const uint16_t* m_tablesIndex;
    const uint16_t* m_keymapsIndex;
    const uint16_t* m_layersIndex;
};

/** Root index of SongGroup */
struct SongGroupIndex : AudioGroupIndex
{
    /** Maps GM program numbers to sound entities */
    struct PageEntry
    {
        ObjectId objId;
        uint8_t priority;
        uint8_t maxVoices;
        uint8_t programNo;
        uint8_t pad;
    };
    std::unordered_map<uint8_t, const PageEntry*> m_normPages;
    std::unordered_map<uint8_t, const PageEntry*> m_drumPages;

    /** Maps SongID to 16 MIDI channel numbers to GM program numbers and settings */
    struct MIDISetup
    {
        uint8_t programNo;
        uint8_t volume;
        uint8_t panning;
        uint8_t reverb;
        uint8_t chorus;
    };
    std::unordered_map<int, const std::array<MIDISetup, 16>*> m_midiSetups;
};

/** Root index of SFXGroup */
struct SFXGroupIndex : AudioGroupIndex
{
    /** Maps game-side SFX define IDs to sound entities */
    struct SFXEntry
    {
        uint16_t defineId;
        ObjectId objId;
        uint8_t priority;
        uint8_t maxVoices;
        uint8_t defVel;
        uint8_t panning;
        uint8_t defKey;
        uint8_t pad;
    };
    std::unordered_map<uint16_t, const SFXEntry*> m_sfxEntries;
};

/** Collection of SongGroup and SFXGroup indexes */
class AudioGroupProject
{
    std::unordered_map<int, SongGroupIndex> m_songGroups;
    std::unordered_map<int, SFXGroupIndex> m_sfxGroups;

    /* MusyX 1.0 structures converted to MusyX 2.0 structures for pointer-compatible access */
    std::unique_ptr<SongGroupIndex::PageEntry[]> m_convNormalPages;
    std::unique_ptr<SongGroupIndex::PageEntry[]> m_convDrumPages;
    std::unique_ptr<std::array<SongGroupIndex::MIDISetup, 16>[]> m_convMidiSetups;
    void _allocateConvBuffers(const unsigned char* data, N64DataTag);
    void _allocateConvBuffers(const unsigned char* data, PCDataTag);

public:
    AudioGroupProject(const unsigned char* data, GCNDataTag);
    AudioGroupProject(const unsigned char* data, N64DataTag);
    AudioGroupProject(const unsigned char* data, PCDataTag);
    static AudioGroupProject CreateAudioGroupProject(const AudioGroupData& data);

    const SongGroupIndex* getSongGroupIndex(int groupId) const;
    const SFXGroupIndex* getSFXGroupIndex(int groupId) const;

    const std::unordered_map<int, SongGroupIndex>& songGroups() const {return m_songGroups;}
    const std::unordered_map<int, SFXGroupIndex>& sfxGroups() const {return m_sfxGroups;}
};

}

#endif // __AMUSE_AUDIOGROUPPROJECT_HPP__

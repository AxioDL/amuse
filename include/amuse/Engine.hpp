#ifndef __AMUSE_ENGINE_HPP__
#define __AMUSE_ENGINE_HPP__

#include <memory>
#include <list>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include "Emitter.hpp"
#include "AudioGroupSampleDirectory.hpp"

namespace amuse
{
class IBackendVoiceAllocator;
class Voice;
class Emitter;
class Sequencer;
class AudioGroup;
class AudioGroupData;

/** Main audio playback system for a single audio output */
class Engine
{
    IBackendVoiceAllocator& m_backend;
    std::unordered_map<int, std::unique_ptr<AudioGroup>> m_audioGroups;
    std::list<Voice> m_activeVoices;
    std::list<Emitter> m_activeEmitters;
    std::list<Sequencer> m_activeSequencers;
    std::linear_congruential_engine<uint32_t, 0x41c64e6d, 0x3039, UINT32_MAX> m_random;
    int m_nextVid = 0;
    Voice* _allocateVoice(const AudioGroup& group, double sampleRate, bool dynamicPitch, bool emitter);
    std::list<Voice>::iterator _destroyVoice(Voice* voice);
    AudioGroup* _findGroupFromSfxId(int sfxId, const AudioGroupSampleDirectory::Entry*& entOut) const;
    AudioGroup* _findGroupFromSongId(int songId) const;
public:
    Engine(IBackendVoiceAllocator& backend);

    /** Update all active audio entities and fill OS audio buffers as needed */
    void pumpEngine();

    /** Add audio group data pointers to engine; must remain resident! */
    bool addAudioGroup(int groupId, const AudioGroupData& data);

    /** Remove audio group from engine */
    void removeAudioGroup(int groupId);

    /** Start soundFX playing from loaded audio groups */
    Voice* fxStart(int sfxId, float vol, float pan);

    /** Start soundFX playing from loaded audio groups, attach to positional emitter */
    Emitter* addEmitter(const Vector3f& pos, const Vector3f& dir, float maxDist,
                        float falloff, int sfxId, float minVol, float maxVol);

    /** Start song playing from loaded audio groups */
    Sequencer* seqPlay(int songId, const unsigned char* arrData);

    /** Find voice from VoiceId */
    Voice* findVoice(int vid);

    /** Stop all voices in `kg`, stops immediately (no KeyOff) when `flag` set */
    void killKeygroup(uint8_t kg, uint8_t flag);

    /** Send all voices using `macroId` the message `val` */
    void sendMacroMessage(ObjectId macroId, int32_t val);

    /** Obtain next random number from engine's PRNG */
    uint32_t nextRandom() {return m_random();}
};

}

#endif // __AMUSE_ENGINE_HPP__

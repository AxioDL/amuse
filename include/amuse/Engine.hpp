#ifndef __AMUSE_ENGINE_HPP__
#define __AMUSE_ENGINE_HPP__

#include <memory>
#include <list>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include "Emitter.hpp"
#include "AudioGroupSampleDirectory.hpp"
#include "Sequencer.hpp"
#include "Studio.hpp"

namespace amuse
{
class IBackendVoiceAllocator;
class Voice;
class Submix;
class Emitter;
class AudioGroup;
class AudioGroupData;
class IMIDIReader;

enum class AmplitudeMode
{
    PerSample,      /**< Per-sample amplitude evaluation (dt = 1.0 / sampleRate, rather CPU demanding) */
    BlockLinearized /**< Per-block lerp amplitude evaluation (dt = 160.0 / sampleRate) */
};

/** Main audio playback system for a single audio output */
class Engine
{
    friend class Voice;
    friend class Emitter;
    friend class Sequencer;
    friend class Studio;
    friend struct Sequencer::ChannelState;

    IBackendVoiceAllocator& m_backend;
    AmplitudeMode m_ampMode;
    std::unique_ptr<IMIDIReader> m_midiReader;
    std::unordered_map<const AudioGroupData*, std::unique_ptr<AudioGroup>> m_audioGroups;
    std::list<std::shared_ptr<Voice>> m_activeVoices;
    std::list<std::shared_ptr<Emitter>> m_activeEmitters;
    std::list<std::shared_ptr<Sequencer>> m_activeSequencers;
    std::list<std::weak_ptr<Studio>> m_activeStudios; /* lifetime dependent on contributing audio entities */
    bool m_defaultStudioReady = false;
    std::shared_ptr<Studio> m_defaultStudio;
    std::unordered_map<uint16_t, std::tuple<AudioGroup*, int, const SFXGroupIndex::SFXEntry*>> m_sfxLookup;
    std::linear_congruential_engine<uint32_t, 0x41c64e6d, 0x3039, UINT32_MAX> m_random;
    int m_nextVid = 0;

    AudioGroup* _addAudioGroup(const AudioGroupData& data, std::unique_ptr<AudioGroup>&& grp);
    std::pair<AudioGroup*, const SongGroupIndex*> _findSongGroup(int groupId) const;
    std::pair<AudioGroup*, const SFXGroupIndex*> _findSFXGroup(int groupId) const;

    std::list<std::shared_ptr<Voice>>::iterator _allocateVoice(const AudioGroup& group, int groupId, double sampleRate,
                                                               bool dynamicPitch, bool emitter,
                                                               std::weak_ptr<Studio> studio);
    std::list<std::shared_ptr<Sequencer>>::iterator _allocateSequencer(const AudioGroup& group, int groupId,
                                                                       int setupId, std::weak_ptr<Studio> studio);
    std::shared_ptr<Studio> _allocateStudio(bool mainOut);
    std::list<std::shared_ptr<Voice>>::iterator _destroyVoice(std::list<std::shared_ptr<Voice>>::iterator it);
    std::list<std::shared_ptr<Sequencer>>::iterator
    _destroySequencer(std::list<std::shared_ptr<Sequencer>>::iterator it);
    void _bringOutYourDead();
    void _5MsCallback(double dt);

public:
    ~Engine();
    Engine(IBackendVoiceAllocator& backend, AmplitudeMode ampMode = AmplitudeMode::PerSample);

    /** Access voice backend of engine */
    IBackendVoiceAllocator& getBackend() { return m_backend; }

    /** Update all active audio entities and fill OS audio buffers as needed */
    void pumpEngine();

    /** Add audio group data pointers to engine; must remain resident! */
    const AudioGroup* addAudioGroup(const AudioGroupData& data);

    /** Remove audio group from engine */
    void removeAudioGroup(const AudioGroupData& data);

    /** Access engine's default studio */
    std::shared_ptr<Studio> getDefaultStudio() { return m_defaultStudio; }

    /** Create new Studio within engine */
    std::shared_ptr<Studio> addStudio(bool mainOut);

    /** Start soundFX playing from loaded audio groups */
    std::shared_ptr<Voice> fxStart(int sfxId, float vol, float pan, std::weak_ptr<Studio> smx);
    std::shared_ptr<Voice> fxStart(int sfxId, float vol, float pan)
    {
        return fxStart(sfxId, vol, pan, m_defaultStudio);
    }

    /** Start soundFX playing from loaded audio groups, attach to positional emitter */
    std::shared_ptr<Emitter> addEmitter(const Vector3f& pos, const Vector3f& dir, float maxDist, float falloff,
                                        int sfxId, float minVol, float maxVol, std::weak_ptr<Studio> smx);

    /** Start song playing from loaded audio groups */
    std::shared_ptr<Sequencer> seqPlay(int groupId, int songId, const unsigned char* arrData,
                                       std::weak_ptr<Studio> smx);
    std::shared_ptr<Sequencer> seqPlay(int groupId, int songId, const unsigned char* arrData)
    {
        return seqPlay(groupId, songId, arrData, m_defaultStudio);
    }

    /** Set total volume of engine */
    void setVolume(float vol);

    /** Find voice from VoiceId */
    std::shared_ptr<Voice> findVoice(int vid);

    /** Stop all voices in `kg`, stops immediately (no KeyOff) when `now` set */
    void killKeygroup(uint8_t kg, bool now);

    /** Send all voices using `macroId` the message `val` */
    void sendMacroMessage(ObjectId macroId, int32_t val);

    /** Obtain next random number from engine's PRNG */
    uint32_t nextRandom() { return m_random(); }

    /** Obtain list of active sequencers */
    std::list<std::shared_ptr<Sequencer>>& getActiveSequencers() { return m_activeSequencers; }
};
}

#endif // __AMUSE_ENGINE_HPP__

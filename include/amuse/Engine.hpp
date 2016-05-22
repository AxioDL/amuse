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
    PerSample, /**< Per-sample amplitude evaluation (dt = 1.0 / sampleRate, rather CPU demanding) */
    BlockLinearized /**< Per-block lerp amplitude evaluation (dt = 160.0 / sampleRate) */
};

/** Main audio playback system for a single audio output */
class Engine
{
    friend class Voice;
    friend class Emitter;
    friend class Sequencer;
    friend struct Sequencer::ChannelState;

    IBackendVoiceAllocator& m_backend;
    AmplitudeMode m_ampMode;
    std::unique_ptr<IMIDIReader> m_midiReader;
    std::unordered_map<const AudioGroupData*, std::unique_ptr<AudioGroup>> m_audioGroups;
    std::list<std::shared_ptr<Voice>> m_activeVoices;
    std::list<std::shared_ptr<Emitter>> m_activeEmitters;
    std::list<std::shared_ptr<Sequencer>> m_activeSequencers;
    std::list<Submix> m_activeSubmixes;
    std::unordered_map<uint16_t, std::tuple<AudioGroup*, int, const SFXGroupIndex::SFXEntry*>> m_sfxLookup;
    std::linear_congruential_engine<uint32_t, 0x41c64e6d, 0x3039, UINT32_MAX> m_random;
    int m_nextVid = 0;

    std::pair<AudioGroup*, const SongGroupIndex*> _findSongGroup(int groupId) const;
    std::pair<AudioGroup*, const SFXGroupIndex*> _findSFXGroup(int groupId) const;

    std::shared_ptr<Voice> _allocateVoice(const AudioGroup& group, int groupId, double sampleRate,
                                          bool dynamicPitch, bool emitter, Submix* smx);
    std::shared_ptr<Sequencer> _allocateSequencer(const AudioGroup& group, int groupId,
                                                  int setupId, Submix* smx);
    Submix* _allocateSubmix(Submix* smx);
    std::list<std::shared_ptr<Voice>>::iterator _destroyVoice(Voice* voice);
    std::list<std::shared_ptr<Sequencer>>::iterator _destroySequencer(Sequencer* sequencer);
    std::list<Submix>::iterator _destroySubmix(Submix* smx);
    std::list<Submix>::iterator _removeSubmix(Submix* smx);
    void _bringOutYourDead();
    void _5MsCallback(double dt);
public:
    ~Engine();
    Engine(IBackendVoiceAllocator& backend, AmplitudeMode ampMode=AmplitudeMode::PerSample);

    /** Access voice backend of engine */
    IBackendVoiceAllocator& getBackend() {return m_backend;}

    /** Update all active audio entities and fill OS audio buffers as needed */
    void pumpEngine();

    /** Add audio group data pointers to engine; must remain resident! */
    const AudioGroup* addAudioGroup(const AudioGroupData& data);

    /** Remove audio group from engine */
    void removeAudioGroup(const AudioGroupData& data);

    /** Create new Submix (a.k.a 'Studio') within root mix engine */
    Submix* addSubmix(Submix* parent=nullptr);

    /** Remove Submix and deallocate */
    void removeSubmix(Submix* smx);

    /** Start soundFX playing from loaded audio groups */
    std::shared_ptr<Voice> fxStart(int sfxId, float vol, float pan, Submix* smx=nullptr);

    /** Start soundFX playing from loaded audio groups, attach to positional emitter */
    std::shared_ptr<Emitter> addEmitter(const Vector3f& pos, const Vector3f& dir, float maxDist,
                                        float falloff, int sfxId, float minVol, float maxVol,
                                        Submix* smx=nullptr);

    /** Start song playing from loaded audio groups */
    std::shared_ptr<Sequencer> seqPlay(int groupId, int songId, const unsigned char* arrData,
                                       Submix* smx=nullptr);

    /** Find voice from VoiceId */
    std::shared_ptr<Voice> findVoice(int vid);

    /** Stop all voices in `kg`, stops immediately (no KeyOff) when `now` set */
    void killKeygroup(uint8_t kg, bool now);

    /** Send all voices using `macroId` the message `val` */
    void sendMacroMessage(ObjectId macroId, int32_t val);

    /** Obtain next random number from engine's PRNG */
    uint32_t nextRandom() {return m_random();}

    /** Obtain list of active sequencers */
    std::list<std::shared_ptr<Sequencer>>& getActiveSequencers() {return m_activeSequencers;}
};

}

#endif // __AMUSE_ENGINE_HPP__

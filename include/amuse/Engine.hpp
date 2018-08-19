#ifndef __AMUSE_ENGINE_HPP__
#define __AMUSE_ENGINE_HPP__

#include <memory>
#include <list>
#include <random>
#include <unordered_map>
#include <unordered_set>
#include "Emitter.hpp"
#include "Listener.hpp"
#include "AudioGroupSampleDirectory.hpp"
#include "Sequencer.hpp"
#include "Studio.hpp"
#include "IBackendVoiceAllocator.hpp"

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
    std::list<ObjToken<Voice>> m_activeVoices;
    std::list<ObjToken<Emitter>> m_activeEmitters;
    std::list<ObjToken<Listener>> m_activeListeners;
    std::list<ObjToken<Sequencer>> m_activeSequencers;
    bool m_defaultStudioReady = false;
    ObjToken<Studio> m_defaultStudio;
    std::unordered_map<SFXId, std::tuple<AudioGroup*, GroupId, const SFXGroupIndex::SFXEntry*>> m_sfxLookup;
    std::linear_congruential_engine<uint32_t, 0x41c64e6d, 0x3039, UINT32_MAX> m_random;
    int m_nextVid = 0;
    float m_masterVolume = 1.f;
    AudioChannelSet m_channelSet = AudioChannelSet::Unknown;

    AudioGroup* _addAudioGroup(const AudioGroupData& data, std::unique_ptr<AudioGroup>&& grp);
    std::pair<AudioGroup*, const SongGroupIndex*> _findSongGroup(GroupId groupId) const;
    std::pair<AudioGroup*, const SFXGroupIndex*> _findSFXGroup(GroupId groupId) const;

    std::list<ObjToken<Voice>>::iterator _allocateVoice(const AudioGroup& group, GroupId groupId, double sampleRate,
                                                        bool dynamicPitch, bool emitter, ObjToken<Studio> studio);
    std::list<ObjToken<Sequencer>>::iterator _allocateSequencer(const AudioGroup& group, GroupId groupId,
                                                                SongId setupId, ObjToken<Studio> studio);
    ObjToken<Studio> _allocateStudio(bool mainOut);
    std::list<ObjToken<Voice>>::iterator _destroyVoice(std::list<ObjToken<Voice>>::iterator it);
    std::list<ObjToken<Sequencer>>::iterator
    _destroySequencer(std::list<ObjToken<Sequencer>>::iterator it);
    void _bringOutYourDead();

public:
    ~Engine();
    Engine(IBackendVoiceAllocator& backend, AmplitudeMode ampMode = AmplitudeMode::PerSample);

    /** Access voice backend of engine */
    IBackendVoiceAllocator& getBackend() { return m_backend; }

    /** Access MIDI reader */
    IMIDIReader* getMIDIReader() const { return m_midiReader.get(); }

    /** Add audio group data pointers to engine; must remain resident! */
    const AudioGroup* addAudioGroup(const AudioGroupData& data);

    /** Remove audio group from engine */
    void removeAudioGroup(const AudioGroupData& data);

    /** Access engine's default studio */
    ObjToken<Studio> getDefaultStudio() { return m_defaultStudio; }

    /** Create new Studio within engine */
    ObjToken<Studio> addStudio(bool mainOut);

    /** Start soundFX playing from loaded audio groups */
    ObjToken<Voice> fxStart(SFXId sfxId, float vol, float pan, ObjToken<Studio> smx);
    ObjToken<Voice> fxStart(SFXId sfxId, float vol, float pan)
    {
        return fxStart(sfxId, vol, pan, m_defaultStudio);
    }

    /** Start soundFX playing from explicit group data (for editor use) */
    ObjToken<Voice> fxStart(const AudioGroup* group, GroupId groupId, SFXId sfxId, float vol, float pan, ObjToken<Studio> smx);
    ObjToken<Voice> fxStart(const AudioGroup* group, GroupId groupId, SFXId sfxId, float vol, float pan)
    {
        return fxStart(group, groupId, sfxId, vol, pan, m_defaultStudio);
    }

    /** Start SoundMacro node playing directly (for editor use) */
    ObjToken<Voice> macroStart(const AudioGroup* group, SoundMacroId id, uint8_t key,
                               uint8_t vel, uint8_t mod, ObjToken<Studio> smx);
    ObjToken<Voice> macroStart(const AudioGroup* group, SoundMacroId id, uint8_t key,
                               uint8_t vel, uint8_t mod)
    {
        return macroStart(group, id, key, vel, mod, m_defaultStudio);
    }

    /** Start SoundMacro object playing directly (for editor use) */
    ObjToken<Voice> macroStart(const AudioGroup* group, const SoundMacro* macro, uint8_t key,
                               uint8_t vel, uint8_t mod, ObjToken<Studio> smx);
    ObjToken<Voice> macroStart(const AudioGroup* group, const SoundMacro* macro, uint8_t key,
                               uint8_t vel, uint8_t mod)
    {
        return macroStart(group, macro, key, vel, mod, m_defaultStudio);
    }

    /** Start PageObject node playing directly (for editor use) */
    ObjToken<Voice> pageObjectStart(const AudioGroup* group, ObjectId id, uint8_t key,
                                    uint8_t vel, uint8_t mod, ObjToken<Studio> smx);
    ObjToken<Voice> pageObjectStart(const AudioGroup* group, ObjectId id, uint8_t key,
                                    uint8_t vel, uint8_t mod)
    {
        return pageObjectStart(group, id, key, vel, mod, m_defaultStudio);
    }

    /** Start soundFX playing from loaded audio groups, attach to positional emitter */
    ObjToken<Emitter> addEmitter(const float* pos, const float* dir, float maxDist, float falloff,
                                 SFXId sfxId, float minVol, float maxVol, bool doppler, ObjToken<Studio> smx);
    ObjToken<Emitter> addEmitter(const float* pos, const float* dir, float maxDist, float falloff,
                                 SFXId sfxId, float minVol, float maxVol, bool doppler)
    {
        return addEmitter(pos, dir, maxDist, falloff, sfxId, minVol, maxVol, doppler, m_defaultStudio);
    }

    /** Build listener and add to engine's listener list */
    ObjToken<Listener> addListener(const float* pos, const float* dir, const float* heading, const float* up,
                                   float frontDiff, float backDiff, float soundSpeed, float volume);

    /** Remove listener from engine's listener list */
    void removeListener(Listener* listener);

    /** Start song playing from loaded audio groups */
    ObjToken<Sequencer> seqPlay(GroupId groupId, SongId songId, const unsigned char* arrData, ObjToken<Studio> smx);
    ObjToken<Sequencer> seqPlay(GroupId groupId, SongId songId, const unsigned char* arrData)
    {
        return seqPlay(groupId, songId, arrData, m_defaultStudio);
    }

    /** Start song playing from explicit group data (for editor use) */
    ObjToken<Sequencer> seqPlay(const AudioGroup* group, GroupId groupId, SongId songId, const unsigned char* arrData, ObjToken<Studio> smx);
    ObjToken<Sequencer> seqPlay(const AudioGroup* group, GroupId groupId, SongId songId, const unsigned char* arrData)
    {
        return seqPlay(group, groupId, songId, arrData, m_defaultStudio);
    }

    /** Set total volume of engine */
    void setVolume(float vol);

    /** Find voice from VoiceId */
    ObjToken<Voice> findVoice(int vid);

    /** Stop all voices in `kg`, stops immediately (no KeyOff) when `now` set */
    void killKeygroup(uint8_t kg, bool now);

    /** Send all voices using `macroId` the message `val` */
    void sendMacroMessage(ObjectId macroId, int32_t val);

    /** Obtain next random number from engine's PRNG */
    uint32_t nextRandom() { return m_random(); }

    /** Obtain list of active voices */
    std::list<ObjToken<Voice>>& getActiveVoices() { return m_activeVoices; }

    /** Obtain total active voice count (including child voices) */
    size_t getNumTotalActiveVoices() const;

    /** Obtain list of active sequencers */
    std::list<ObjToken<Sequencer>>& getActiveSequencers() { return m_activeSequencers; }

    /** All mixing occurs in virtual 5ms intervals;
     *  this is called at the start of each interval for all mixable entities */
    void _on5MsInterval(IBackendVoiceAllocator& engine, double dt);

    /** When a pumping cycle is complete this is called to allow the client to
     *  perform periodic cleanup tasks */
    void _onPumpCycleComplete(IBackendVoiceAllocator& engine);
};
}

#endif // __AMUSE_ENGINE_HPP__

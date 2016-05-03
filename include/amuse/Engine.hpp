#ifndef __AMUSE_ENGINE_HPP__
#define __AMUSE_ENGINE_HPP__

#include <memory>
#include <list>
#include <unordered_map>
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
    std::list<std::unique_ptr<Voice>> m_activeVoices;
    std::list<std::unique_ptr<Emitter>> m_activeEmitters;
    std::list<std::unique_ptr<Sequencer>> m_activeSequencers;
    Voice* _allocateVoice(int groupId, double sampleRate, bool dynamicPitch);
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
};

}

#endif // __AMUSE_ENGINE_HPP__

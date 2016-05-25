#ifndef __AMUSE_AUDIOUNIT_BACKEND_HPP__
#define __AMUSE_AUDIOUNIT_BACKEND_HPP__
#ifdef __APPLE__

#include <Availability.h>

#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101100
#define AMUSE_HAS_AUDIO_UNIT 1

#include <AudioUnit/AudioUnit.h>

#include "optional.hpp"

#include "amuse/BooBackend.hpp"
#include "amuse/Engine.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendSubmix.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"

namespace amuse
{

/** Backend MIDI event reader for controlling sequencer with external hardware / software */
class AudioUnitBackendMIDIReader : public BooBackendMIDIReader
{
    friend class AudioUnitBackendVoiceAllocator;
public:
    AudioUnitBackendMIDIReader(Engine& engine)
    : BooBackendMIDIReader(engine, "AudioUnit MIDI") {}
};

/** Backend voice allocator implementation for AudioUnit mixer */
class AudioUnitBackendVoiceAllocator : public BooBackendVoiceAllocator
{
    friend class AudioUnitBackendMIDIReader;
public:
    AudioUnitBackendVoiceAllocator(boo::IAudioVoiceEngine& booEngine)
    : BooBackendVoiceAllocator(booEngine) {}
};

void RegisterAudioUnit();

}

@interface AmuseAudioUnit : AUAudioUnit
{
    std::unique_ptr<boo::IAudioVoiceEngine> m_booBackend;
    std::experimental::optional<amuse::AudioUnitBackendVoiceAllocator> m_voxAlloc;
    std::experimental::optional<amuse::Engine> m_engine;
    AUAudioUnitBusArray* m_outs;
}
@end

#endif
#endif
#endif // __AMUSE_AUDIOUNIT_BACKEND_HPP__

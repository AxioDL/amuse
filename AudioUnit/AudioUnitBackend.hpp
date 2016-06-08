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
#import "AudioGroupFilePresenter.hpp"

@class AudioUnitViewController;

namespace amuse
{

/** Backend voice allocator implementation for AudioUnit mixer */
class AudioUnitBackendVoiceAllocator : public BooBackendVoiceAllocator
{
public:
    AudioUnitBackendVoiceAllocator(boo::IAudioVoiceEngine& booEngine)
    : BooBackendVoiceAllocator(booEngine) {}
};

void RegisterAudioUnit();

}

@interface AmuseAudioUnit : AUAudioUnit <AudioGroupClient>
{
@public
    AudioUnitViewController* m_viewController;
    std::unique_ptr<boo::IAudioVoiceEngine> m_booBackend;
    std::experimental::optional<amuse::AudioUnitBackendVoiceAllocator> m_voxAlloc;
    std::experimental::optional<amuse::Engine> m_engine;
    AudioGroupFilePresenter* m_filePresenter;
    AUAudioUnitBus* m_outBus;
    AUAudioUnitBusArray* m_outs;
}
- (nullable id)initWithComponentDescription:(AudioComponentDescription)componentDescription
                                      error:(NSError * __nullable * __nonnull)outError
                             viewController:(AudioUnitViewController* __nonnull)vc;
- (void)requestAudioGroup:(AudioGroupToken*)group;
@end

#endif
#endif
#endif // __AMUSE_AUDIOUNIT_BACKEND_HPP__

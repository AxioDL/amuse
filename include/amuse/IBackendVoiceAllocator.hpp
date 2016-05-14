#ifndef __AMUSE_IBACKENDVOICEALLOCATOR_HPP__
#define __AMUSE_IBACKENDVOICEALLOCATOR_HPP__

#include <memory>

namespace amuse
{
class IBackendVoice;
class IBackendSubmix;
class Voice;
class Submix;

/** Same enum as boo for describing speaker-configuration */
enum class AudioChannelSet
{
    Stereo,
    Quad,
    Surround51,
    Surround71,
    Unknown = 0xff
};

/**
 * @brief Client-implemented voice allocator
 */
class IBackendVoiceAllocator
{
public:
    virtual ~IBackendVoiceAllocator() = default;

    /** Amuse obtains a new voice from the platform this way */
    virtual std::unique_ptr<IBackendVoice> allocateVoice(Voice& clientVox,
                                                         double sampleRate,
                                                         bool dynamicPitch)=0;

    /** Amuse obtains a new submix from the platform this way */
    virtual std::unique_ptr<IBackendSubmix> allocateSubmix(Submix& clientSmx)=0;

    /** Amuse obtains speaker-configuration from the platform this way */
    virtual AudioChannelSet getAvailableSet()=0;

    /** Amuse flushes voice samples to the backend this way */
    virtual void pumpAndMixVoices()=0;
};

}

#endif // __AMUSE_IBACKENDVOICEALLOCATOR_HPP__

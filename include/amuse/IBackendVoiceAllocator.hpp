#ifndef __AMUSE_IBACKENDVOICEALLOCATOR_HPP__
#define __AMUSE_IBACKENDVOICEALLOCATOR_HPP__

#include <memory>

namespace amuse
{
class IBackendVoice;
class Voice;

/**
 * @brief Client-implemented voice allocator
 */
class IBackendVoiceAllocator
{
public:
    virtual ~IBackendVoiceAllocator() = default;

    /** Amuse obtains a new voice from the platform this way */
    virtual std::unique_ptr<IBackendVoice> allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch)=0;
};

}

#endif // __AMUSE_IBACKENDVOICEALLOCATOR_HPP__

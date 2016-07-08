#ifndef __AMUSE_SURROUNDPROFILES_HPP__
#define __AMUSE_SURROUNDPROFILES_HPP__

#include "IBackendVoice.hpp"
#include "IBackendVoiceAllocator.hpp"
#include "Emitter.hpp"

namespace amuse
{
struct ReferenceVector;

/** Support class for attenuating channel audio based on speaker 'positions' */
class SurroundProfiles
{
    static void SetupRefs(float matOut[8], const ChannelMap& map, const Vector3f& listenEmit, const ReferenceVector refs[]);

public:
    static void SetupMatrix(float matOut[8], const ChannelMap& map, AudioChannelSet set, const Vector3f& emitPos,
                            const Vector3f& listenPos, const Vector3f& listenDir, const Vector3f& listenUp);
};
}

#endif // __AMUSE_SURROUNDPROFILES_HPP__

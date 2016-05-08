#ifndef __AMUSE_EFFECTBASE_HPP__
#define __AMUSE_EFFECTBASE_HPP__

#include <stdint.h>
#include <stdlib.h>

namespace amuse
{
class ChannelMap;

class EffectBase
{
public:
    virtual void applyEffect(int16_t* audio, size_t frameCount,
                             const ChannelMap& chanMap, double sampleRate)=0;
    virtual void applyEffect(int32_t* audio, size_t frameCount,
                             const ChannelMap& chanMap, double sampleRate)=0;
    virtual void applyEffect(float* audio, size_t frameCount,
                             const ChannelMap& chanMap, double sampleRate)=0;
};

}

#endif // __AMUSE_EFFECTBASE_HPP__

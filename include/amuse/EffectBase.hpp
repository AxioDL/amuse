#ifndef __AMUSE_EFFECTBASE_HPP__
#define __AMUSE_EFFECTBASE_HPP__

#include <stdint.h>
#include <stdlib.h>

namespace amuse
{
struct ChannelMap;

class EffectBaseTypeless
{
public:
    virtual ~EffectBaseTypeless() = default;
};

template <typename T>
class EffectBase : public EffectBaseTypeless
{
public:
    virtual void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap)=0;
};

}

#endif // __AMUSE_EFFECTBASE_HPP__

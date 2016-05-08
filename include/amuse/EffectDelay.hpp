#ifndef __AMUSE_EFFECTDELAY_HPP__
#define __AMUSE_EFFECTDELAY_HPP__

#include "EffectBase.hpp"
#include <stdint.h>

namespace amuse
{

/** Mixes the audio back into itself after specified delay */
class EffectDelay : public EffectBase
{
    uint32_t m_delay[8]; /**< [10, 5000] time in ms of each channel's delay */
    uint32_t m_feedback[8]; /**< [0, 100] percent to mix delayed signal with input signal */
    uint32_t m_output[8]; /**< [0, 100] total output percent */
public:
    EffectDelay(uint32_t initDelay, uint32_t initFeedback, uint32_t initOutput);
    void applyEffect(int16_t* audio, size_t frameCount,
                     const ChannelMap& chanMap, double sampleRate);
    void applyEffect(int32_t* audio, size_t frameCount,
                     const ChannelMap& chanMap, double sampleRate);
    void applyEffect(float* audio, size_t frameCount,
                     const ChannelMap& chanMap, double sampleRate);
};

}

#endif // __AMUSE_EFFECTDELAY_HPP__

#ifndef __AMUSE_EFFECTDELAY_HPP__
#define __AMUSE_EFFECTDELAY_HPP__

#include "EffectBase.hpp"
#include <stdint.h>
#include <memory>

namespace amuse
{

/** Mixes the audio back into itself after specified delay */
template <typename T>
class EffectDelay : public EffectBase<T>
{
    uint32_t x0_currentSize[8];
    uint32_t xc_currentPos[8];
    uint32_t x18_currentFeedback[8];
    uint32_t x24_currentOutput[8];

    std::unique_ptr<T[]> x30_chanLines[8];

    uint32_t x3c_delay[8]; /**< [10, 5000] time in ms of each channel's delay */
    uint32_t x48_feedback[8]; /**< [0, 100] percent to mix delayed signal with input signal */
    uint32_t x54_output[8]; /**< [0, 100] total output percent */

    uint32_t m_sampsPerMs;
    uint32_t m_blockSamples;
    bool m_dirty = true;
    void _update();
public:
    EffectDelay(uint32_t initDelay, uint32_t initFeedback, uint32_t initOutput, double sampleRate);
    void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap);
};

}

#endif // __AMUSE_EFFECTDELAY_HPP__

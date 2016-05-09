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
    uint32_t x0_currentSize[8]; /**< per-channel delay-line buffer sizes */
    uint32_t xc_currentPos[8]; /**< per-channel block-index */
    uint32_t x18_currentFeedback[8]; /**< [0, 128] feedback attenuator */
    uint32_t x24_currentOutput[8]; /**< [0, 128] total attenuator */

    std::unique_ptr<T[]> x30_chanLines[8];

    uint32_t x3c_delay[8]; /**< [10, 5000] time in ms of each channel's delay */
    uint32_t x48_feedback[8]; /**< [0, 100] percent to mix delayed signal with input signal */
    uint32_t x54_output[8]; /**< [0, 100] total output percent */

    uint32_t m_sampsPerMs; /**< canonical count of samples per ms for the current backend */
    uint32_t m_blockSamples; /**< count of samples in a 5ms block */
    bool m_dirty = true; /**< needs update of internal parameter data */
    void _update();
public:
    EffectDelay(uint32_t initDelay, uint32_t initFeedback, uint32_t initOutput, double sampleRate);
    void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap);
};

}

#endif // __AMUSE_EFFECTDELAY_HPP__

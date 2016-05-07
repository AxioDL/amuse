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
};

}

#endif // __AMUSE_EFFECTDELAY_HPP__

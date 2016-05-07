#ifndef __AMUSE_EFFECTCHORUS_HPP__
#define __AMUSE_EFFECTCHORUS_HPP__

#include "EffectBase.hpp"
#include <stdint.h>

namespace amuse
{

/** Mixes the audio back into itself after continuously-varying delay */
class EffectChorus : public EffectBase
{
    uint32_t m_baseDelay; /**< [5, 15] minimum value (in ms) for computed delay */
    uint32_t m_variation; /**< [0, 5] time error (in ms) to set delay within */
    uint32_t m_period; /**< [500, 10000] time (in ms) of one delay-shift cycle */
};

}

#endif // __AMUSE_EFFECTCHORUS_HPP__

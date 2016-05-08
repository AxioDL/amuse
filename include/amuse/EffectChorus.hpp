#ifndef __AMUSE_EFFECTCHORUS_HPP__
#define __AMUSE_EFFECTCHORUS_HPP__

#include "EffectBase.hpp"
#include <stdint.h>

namespace amuse
{

#define AMUSE_CHORUS_NUM_BLOCKS 3

/** Mixes the audio back into itself after continuously-varying delay */
template <typename T>
class EffectChorus : public EffectBase<T>
{
    T* x0_lastLeft[AMUSE_CHORUS_NUM_BLOCKS];
    T* xc_lastRight[AMUSE_CHORUS_NUM_BLOCKS];
    T* x18_lastRearLeft[AMUSE_CHORUS_NUM_BLOCKS];
    T* x18_lastRearRight[AMUSE_CHORUS_NUM_BLOCKS];
    T* x18_lastCenter[AMUSE_CHORUS_NUM_BLOCKS];
    T* x18_lastLFE[AMUSE_CHORUS_NUM_BLOCKS];
    T* x18_lastSideLeft[AMUSE_CHORUS_NUM_BLOCKS];
    T* x18_lastSideRight[AMUSE_CHORUS_NUM_BLOCKS];

    uint8_t x24_currentLast = 1;
    T x28_oldLeft[4] = {};
    T x38_oldRight[4] = {};
    T x48_oldRearLeft[4] = {};
    T x48_oldRearRight[4] = {};
    T x48_oldCenter[4] = {};
    T x48_oldLFE[4] = {};
    T x48_oldSideLeft[4] = {};
    T x48_oldSideRight[4] = {};

    uint32_t x58_currentPosLo = 0;
    uint32_t x5c_currentPosHi = 0;

    int32_t x60_pitchOffset;
    uint32_t x64_pitchOffsetPeriodCount;
    uint32_t x68_pitchOffsetPeriod;

    struct SrcInfo
    {
        T* x6c_dest;
        T* x70_smpBase;
        T* x74_old;
        uint32_t x78_posLo;
        uint32_t x7c_posHi;
        uint32_t x80_pitchLo;
        uint32_t x84_pitchHi;
        uint32_t x88_trigger;
        uint32_t x8c_target = 0;

        void doSrc1(size_t blockSamples, size_t chanCount);
        void doSrc2(size_t blockSamples, size_t chanCount);
    } x6c_src;

    uint32_t x90_baseDelay; /**< [5, 15] minimum value (in ms) for computed delay */
    uint32_t x94_variation; /**< [0, 5] time error (in ms) to set delay within */
    uint32_t x98_period; /**< [500, 10000] time (in ms) of one delay-shift cycle */

    double m_sampsPerMs;
    uint32_t m_blockSamples;
    bool m_dirty = true;

    void _update();

public:
    ~EffectChorus();
    EffectChorus(uint32_t baseDelay, uint32_t variation, uint32_t period, double sampleRate);
    void applyEffect(T* audio, size_t frameCount,
                     const ChannelMap& chanMap, double sampleRate);
};

}

#endif // __AMUSE_EFFECTCHORUS_HPP__

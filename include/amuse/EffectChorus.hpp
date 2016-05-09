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
    T* x0_lastChans[8][AMUSE_CHORUS_NUM_BLOCKS]; /**< Evenly-allocated pointer-table for each channel's delay */

    uint8_t x24_currentLast = 1; /**< Last 5ms block-idx to be processed */
    T x28_oldChans[8][4] = {}; /**< Unprocessed history of previous 4 samples */

    uint32_t x58_currentPosLo = 0; /**< 16.7 fixed-point low-part of sample index */
    uint32_t x5c_currentPosHi = 0; /**< 16.7 fixed-point high-part of sample index */

    int32_t x60_pitchOffset; /**< packed 16.16 fixed-point value of pitchHi and pitchLo quantities */
    uint32_t x64_pitchOffsetPeriodCount; /**< trigger value for flipping SRC state */
    uint32_t x68_pitchOffsetPeriod; /**< intermediate block window quantity for calculating SRC state */

    struct SrcInfo
    {
        T* x6c_dest; /**< selected channel's live buffer */
        T* x70_smpBase; /**< selected channel's delay buffer */
        T* x74_old; /**< selected channel's 4-sample history buffer */
        uint32_t x78_posLo; /**< 16.7 fixed-point low-part of sample index */
        uint32_t x7c_posHi; /**< 16.7 fixed-point high-part of sample index */
        uint32_t x80_pitchLo; /**< 16.7 fixed-point low-part of sample-rate conversion differential */
        uint32_t x84_pitchHi; /**< 16.7 fixed-point low-part of sample-rate conversion differential */
        uint32_t x88_trigger; /**< total count of samples per channel across all blocks */
        uint32_t x8c_target = 0; /**< value to reset to when trigger hit */

        void doSrc1(size_t blockSamples, size_t chanCount);
        void doSrc2(size_t blockSamples, size_t chanCount);
    } x6c_src;

    uint32_t x90_baseDelay; /**< [5, 15] minimum value (in ms) for computed delay */
    uint32_t x94_variation; /**< [0, 5] time error (in ms) to set delay within */
    uint32_t x98_period; /**< [500, 10000] time (in ms) of one delay-shift cycle */

    uint32_t m_sampsPerMs; /**< canonical count of samples per ms for the current backend */
    uint32_t m_blockSamples; /**< count of samples in a 5ms block */
    bool m_dirty = true; /**< needs update of internal parameter data */

    void _update();

public:
    ~EffectChorus();
    EffectChorus(uint32_t baseDelay, uint32_t variation, uint32_t period, double sampleRate);
    void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap);
};

}

#endif // __AMUSE_EFFECTCHORUS_HPP__

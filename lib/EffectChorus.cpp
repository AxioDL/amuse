#include "amuse/EffectChorus.hpp"
#include "amuse/Common.hpp"
#include <string.h>

namespace amuse
{

EffectChorus::EffectChorus(uint32_t baseDelay, uint32_t variation,
                           uint32_t period, double sampleRate)
: x90_baseDelay(clamp(5u, baseDelay, 15u)),
  x94_variation(clamp(0u, variation, 5u)),
  x98_period(clamp(500u, period, 10000u)),
  m_sampleRate(sampleRate),
  m_blockSamples(sampleRate * 160.0 / 32000.0)
{
    int32_t* buf = new int32_t[m_blockSamples * AMUSE_CHORUS_NUM_BLOCKS * 8];
    memset(buf, 0, m_blockSamples * AMUSE_CHORUS_NUM_BLOCKS * 8 * sizeof(int32_t));
    size_t chanPitch = m_blockSamples * AMUSE_CHORUS_NUM_BLOCKS;

    for (int i=0 ; i<AMUSE_CHORUS_NUM_BLOCKS ; ++i)
        x0_lastLeft[i] = buf + m_blockSamples * i;

    for (int i=0 ; i<AMUSE_CHORUS_NUM_BLOCKS ; ++i)
        xc_lastRight[i] = buf + chanPitch + m_blockSamples * i;

    for (int i=0 ; i<AMUSE_CHORUS_NUM_BLOCKS ; ++i)
        x18_lastRearLeft[i] = buf + chanPitch * 2 + m_blockSamples * i;

    for (int i=0 ; i<AMUSE_CHORUS_NUM_BLOCKS ; ++i)
        x18_lastRearRight[i] = buf + chanPitch * 3 + m_blockSamples * i;

    for (int i=0 ; i<AMUSE_CHORUS_NUM_BLOCKS ; ++i)
        x18_lastCenter[i] = buf + chanPitch * 4 + m_blockSamples * i;

    for (int i=0 ; i<AMUSE_CHORUS_NUM_BLOCKS ; ++i)
        x18_lastLFE[i] = buf + chanPitch * 5 + m_blockSamples * i;

    for (int i=0 ; i<AMUSE_CHORUS_NUM_BLOCKS ; ++i)
        x18_lastSideLeft[i] = buf + chanPitch * 6 + m_blockSamples * i;

    for (int i=0 ; i<AMUSE_CHORUS_NUM_BLOCKS ; ++i)
        x18_lastSideRight[i] = buf + chanPitch * 7 + m_blockSamples * i;

    x6c_src.x88_trigger = chanPitch;

    x5c_currentPosHi = m_blockSamples * 2 - ((x90_baseDelay - 5) * (m_sampleRate / 1000.0));
    uint32_t temp = (x5c_currentPosHi + (x24_currentLast - 1) * m_blockSamples);
    x5c_currentPosHi = temp - int32_t(temp * 15 / (m_sampleRate / 1000.0)) * chanPitch;

    x68_pitchOffsetPeriod = (x98_period * 2 / 10 + 1) & ~1;
    x60_pitchOffset = x94_variation * 2048 / x68_pitchOffsetPeriod;
}

EffectChorus::~EffectChorus()
{
    delete[] x0_lastLeft[0];
}

void EffectChorus::applyEffect(int16_t* audio, size_t frameCount,
                               const ChannelMap& chanMap, double sampleRate)
{
}

void EffectChorus::applyEffect(int32_t* audio, size_t frameCount,
                               const ChannelMap& chanMap, double sampleRate)
{
}

void EffectChorus::applyEffect(float* audio, size_t frameCount,
                               const ChannelMap& chanMap, double sampleRate)
{
}

}

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

    x0_lastLeft[0] = buf;
    x0_lastLeft[1] = x0_lastLeft[0] + m_blockSamples;
    x0_lastLeft[2] = x0_lastLeft[0] + m_blockSamples * 2;

    xc_lastRight[0] = buf + chanPitch;
    xc_lastRight[1] = xc_lastRight[0] + m_blockSamples;
    xc_lastRight[2] = xc_lastRight[0] + m_blockSamples * 2;

    x18_lastRearLeft[0] = buf + chanPitch * 2;
    x18_lastRearLeft[1] = x18_lastRearLeft[0] + m_blockSamples;
    x18_lastRearLeft[2] = x18_lastRearLeft[0] + m_blockSamples * 2;

    x18_lastRearRight[0] = buf + chanPitch * 3;
    x18_lastRearRight[1] = x18_lastRearRight[0] + m_blockSamples;
    x18_lastRearRight[2] = x18_lastRearRight[0] + m_blockSamples * 2;

    x18_lastCenter[0] = buf + chanPitch * 4;
    x18_lastCenter[1] = x18_lastCenter[0] + m_blockSamples;
    x18_lastCenter[2] = x18_lastCenter[0] + m_blockSamples * 2;

    x18_lastLFE[0] = buf + chanPitch * 5;
    x18_lastLFE[1] = x18_lastLFE[0] + m_blockSamples;
    x18_lastLFE[2] = x18_lastLFE[0] + m_blockSamples * 2;

    x18_lastSideLeft[0] = buf + chanPitch * 6;
    x18_lastSideLeft[1] = x18_lastSideLeft[0] + m_blockSamples;
    x18_lastSideLeft[2] = x18_lastSideLeft[0] + m_blockSamples * 2;

    x18_lastSideRight[0] = buf + chanPitch * 7;
    x18_lastSideRight[1] = x18_lastSideRight[0] + m_blockSamples;
    x18_lastSideRight[2] = x18_lastSideRight[0] + m_blockSamples * 2;

    x6c_src.x88_trigger = chanPitch;

    x5c_currentPosHi = m_blockSamples * 2 - ((x90_baseDelay - 5) * m_sampleRate / 1000.f);
    uint32_t temp = (x5c_currentPosHi + (x24_currentLast - 1) * m_blockSamples);
    x5c_currentPosHi = temp - ((temp * 8 / 15) >> 8) * chanPitch;

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

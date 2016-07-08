#include "amuse/EffectChorus.hpp"
#include "amuse/Common.hpp"
#include "amuse/IBackendVoice.hpp"
#include <string.h>
#include <cmath>

namespace amuse
{

/* clang-format off */
static const float rsmpTab12khz[] =
{
    0.097504, 0.802216, 0.101593, -0.000977,
    0.093506, 0.802032, 0.105804, -0.001038,
    0.089600, 0.801697, 0.110107, -0.001160,
    0.085785, 0.801178, 0.114471, -0.001282,
    0.082031, 0.800476, 0.118927, -0.001404,
    0.078369, 0.799622, 0.123474, -0.001526,
    0.074799, 0.798615, 0.128143, -0.001648,
    0.071350, 0.797424, 0.132874, -0.001770,
    0.067963, 0.796051, 0.137695, -0.001923,
    0.064697, 0.794525, 0.142609, -0.002045,
    0.061493, 0.792847, 0.147614, -0.002197,
    0.058350, 0.790985, 0.152710, -0.002319,
    0.055328, 0.788940, 0.157898, -0.002472,
    0.052368, 0.786743, 0.163177, -0.002655,
    0.049500, 0.784424, 0.168518, -0.002808,
    0.046722, 0.781891, 0.173981, -0.002991,
    0.044006, 0.779205, 0.179504, -0.003143,
    0.041412, 0.776367, 0.185120, -0.003326,
    0.038879, 0.773376, 0.190826, -0.003510,
    0.036407, 0.770233, 0.196594, -0.003693,
    0.034027, 0.766937, 0.202484, -0.003876,
    0.031738, 0.763489, 0.208435, -0.004059,
    0.029510, 0.759857, 0.214447, -0.004272,
    0.027374, 0.756104, 0.220551, -0.004456,
    0.025299, 0.752197, 0.226746, -0.004669,
    0.023315, 0.748169, 0.233002, -0.004852,
    0.021393, 0.743988, 0.239319, -0.005066,
    0.019562, 0.739655, 0.245728, -0.005310,
    0.017792, 0.735199, 0.252197, -0.005524,
    0.016052, 0.730591, 0.258728, -0.005707,
    0.014404, 0.725861, 0.265350, -0.005920,
    0.012817, 0.721008, 0.272034, -0.006165,
    0.011322, 0.716003, 0.278778, -0.006378,
    0.009888, 0.710907, 0.285583, -0.006561,
    0.008514, 0.705658, 0.292450, -0.006775,
    0.007202, 0.700317, 0.299347, -0.007019,
    0.005920, 0.694855, 0.306335, -0.007233,
    0.004700, 0.689270, 0.313385, -0.007416,
    0.003571, 0.683563, 0.320465, -0.007629,
    0.002472, 0.677734, 0.327606, -0.007874,
    0.001434, 0.671844, 0.334778, -0.008087,
    0.000458, 0.665833, 0.341980, -0.008270,
    -0.000488, 0.659729, 0.349243, -0.008453,
    -0.001343, 0.653534, 0.356567, -0.008636,
    -0.002167, 0.647217, 0.363892, -0.008850,
    -0.002960, 0.640839, 0.371277, -0.009033,
    -0.003693, 0.634338, 0.378693, -0.009216,
    -0.004364, 0.627777, 0.386139, -0.009338,
    -0.004974, 0.621155, 0.393616, -0.009491,
    -0.005585, 0.614441, 0.401093, -0.009644,
    -0.006134, 0.607635, 0.408600, -0.009796,
    -0.006653, 0.600769, 0.416107, -0.009918,
    -0.007141, 0.593842, 0.423645, -0.010010,
    -0.007568, 0.586853, 0.431213, -0.010132,
    -0.007965, 0.579773, 0.438751, -0.010223,
    -0.008331, 0.572662, 0.446320, -0.010284,
    -0.008667, 0.565521, 0.453888, -0.010345,
    -0.008972, 0.558319, 0.461456, -0.010406,
    -0.009216, 0.551056, 0.469025, -0.010406,
    -0.009460, 0.543732, 0.476593, -0.010406,
    -0.009674, 0.536407, 0.484131, -0.010376,
    -0.009857, 0.529022, 0.491669, -0.010376,
    -0.010010, 0.521606, 0.499176, -0.010315,
    -0.010132, 0.514160, 0.506683, -0.010254,
    -0.010254, 0.506683, 0.514160, -0.010132,
    -0.010315, 0.499176, 0.521606, -0.010010,
    -0.010376, 0.491669, 0.529022, -0.009857,
    -0.010376, 0.484131, 0.536407, -0.009674,
    -0.010406, 0.476593, 0.543732, -0.009460,
    -0.010406, 0.469025, 0.551056, -0.009216,
    -0.010406, 0.461456, 0.558319, -0.008972,
    -0.010345, 0.453888, 0.565521, -0.008667,
    -0.010284, 0.446320, 0.572662, -0.008331,
    -0.010223, 0.438751, 0.579773, -0.007965,
    -0.010132, 0.431213, 0.586853, -0.007568,
    -0.010010, 0.423645, 0.593842, -0.007141,
    -0.009918, 0.416107, 0.600769, -0.006653,
    -0.009796, 0.408600, 0.607635, -0.006134,
    -0.009644, 0.401093, 0.614441, -0.005585,
    -0.009491, 0.393616, 0.621155, -0.004974,
    -0.009338, 0.386139, 0.627777, -0.004364,
    -0.009216, 0.378693, 0.634338, -0.003693,
    -0.009033, 0.371277, 0.640839, -0.002960,
    -0.008850, 0.363892, 0.647217, -0.002167,
    -0.008636, 0.356567, 0.653534, -0.001343,
    -0.008453, 0.349243, 0.659729, -0.000488,
    -0.008270, 0.341980, 0.665833, 0.000458,
    -0.008087, 0.334778, 0.671844, 0.001434,
    -0.007874, 0.327606, 0.677734, 0.002472,
    -0.007629, 0.320465, 0.683563, 0.003571,
    -0.007416, 0.313385, 0.689270, 0.004700,
    -0.007233, 0.306335, 0.694855, 0.005920,
    -0.007019, 0.299347, 0.700317, 0.007202,
    -0.006775, 0.292450, 0.705658, 0.008514,
    -0.006561, 0.285583, 0.710907, 0.009888,
    -0.006378, 0.278778, 0.716003, 0.011322,
    -0.006165, 0.272034, 0.721008, 0.012817,
    -0.005920, 0.265350, 0.725861, 0.014404,
    -0.005707, 0.258728, 0.730591, 0.016052,
    -0.005524, 0.252197, 0.735199, 0.017792,
    -0.005310, 0.245728, 0.739655, 0.019562,
    -0.005066, 0.239319, 0.743988, 0.021393,
    -0.004852, 0.233002, 0.748169, 0.023315,
    -0.004669, 0.226746, 0.752197, 0.025299,
    -0.004456, 0.220551, 0.756104, 0.027374,
    -0.004272, 0.214447, 0.759857, 0.029510,
    -0.004059, 0.208435, 0.763489, 0.031738,
    -0.003876, 0.202484, 0.766937, 0.034027,
    -0.003693, 0.196594, 0.770233, 0.036407,
    -0.003510, 0.190826, 0.773376, 0.038879,
    -0.003326, 0.185120, 0.776367, 0.041412,
    -0.003143, 0.179504, 0.779205, 0.044006,
    -0.002991, 0.173981, 0.781891, 0.046722,
    -0.002808, 0.168518, 0.784424, 0.049500,
    -0.002655, 0.163177, 0.786743, 0.052368,
    -0.002472, 0.157898, 0.788940, 0.055328,
    -0.002319, 0.152710, 0.790985, 0.058350,
    -0.002197, 0.147614, 0.792847, 0.061493,
    -0.002045, 0.142609, 0.794525, 0.064697,
    -0.001923, 0.137695, 0.796051, 0.067963,
    -0.001770, 0.132874, 0.797424, 0.071350,
    -0.001648, 0.128143, 0.798615, 0.074799,
    -0.001526, 0.123474, 0.799622, 0.078369,
    -0.001404, 0.118927, 0.800476, 0.082031,
    -0.001282, 0.114471, 0.801178, 0.085785,
    -0.001160, 0.110107, 0.801697, 0.089600,
    -0.001038, 0.105804, 0.802032, 0.093506,
    -0.000977, 0.101593, 0.802216, 0.097504,
};
/* clang-format on */

EffectChorus::EffectChorus(uint32_t baseDelay, uint32_t variation, uint32_t period)
: x90_baseDelay(clamp(5u, baseDelay, 15u)), x94_variation(clamp(0u, variation, 5u)), x98_period(clamp(500u, period, 10000u))
{
}

template <typename T>
EffectChorusImp<T>::EffectChorusImp(uint32_t baseDelay, uint32_t variation, uint32_t period, double sampleRate)
: EffectChorus(baseDelay, variation, period)
{
    _setup(sampleRate);
}

template <typename T>
void EffectChorusImp<T>::_setup(double sampleRate)
{
    m_sampsPerMs = std::ceil(sampleRate / 1000.0);
    m_blockSamples = m_sampsPerMs * 5;

    delete[] x0_lastChans[0][0];

    T* buf = new T[m_blockSamples * AMUSE_CHORUS_NUM_BLOCKS * 8];
    memset(buf, 0, m_blockSamples * AMUSE_CHORUS_NUM_BLOCKS * 8 * sizeof(T));
    size_t chanPitch = m_blockSamples * AMUSE_CHORUS_NUM_BLOCKS;

    for (int c = 0; c < 8; ++c)
        for (int i = 0; i < AMUSE_CHORUS_NUM_BLOCKS; ++i)
            x0_lastChans[c][i] = buf + chanPitch * c + m_blockSamples * i;

    x6c_src.x88_trigger = chanPitch;

    m_dirty = true;
}

template <typename T>
void EffectChorusImp<T>::_update()
{
    size_t chanPitch = m_blockSamples * AMUSE_CHORUS_NUM_BLOCKS;
    size_t fifteenSamps = 15 * m_sampsPerMs;

    x5c_currentPosHi = m_blockSamples * 2 - (x90_baseDelay - 5) * m_sampsPerMs;
    x58_currentPosLo = 0;
    uint32_t temp = (x5c_currentPosHi + (x24_currentLast - 1) * m_blockSamples);
    x5c_currentPosHi = temp % (chanPitch / fifteenSamps * fifteenSamps);

    x68_pitchOffsetPeriod = (x98_period / 5 + 1) & ~1;
    x64_pitchOffsetPeriodCount = x68_pitchOffsetPeriod / 2;
    x60_pitchOffset = x94_variation * 2048 / x68_pitchOffsetPeriod;

    m_dirty = false;
}

template <typename T>
EffectChorusImp<T>::~EffectChorusImp()
{
    delete[] x0_lastChans[0][0];
}

template <typename T>
void EffectChorusImp<T>::SrcInfo::doSrc1(size_t blockSamples, size_t chanCount)
{
    float old1 = x74_old[0];
    float old2 = x74_old[1];
    float old3 = x74_old[2];
    float cur = x70_smpBase[x7c_posHi];

    T* dest = x6c_dest;
    for (size_t i = 0; i < blockSamples; ++i)
    {
        const float* selTab = &rsmpTab12khz[x78_posLo >> 23 & 0x1fc];

        uint64_t ovrTest = uint64_t(x78_posLo) + uint64_t(x80_pitchLo);
        if (ovrTest > UINT32_MAX)
        {
            /* overflow */
            x78_posLo = ovrTest & 0xffffffff;
            ++x7c_posHi;
            if (x7c_posHi == x88_trigger)
                x7c_posHi = x8c_target;
            *dest = ClampFull<T>(selTab[0] * old1 + selTab[1] * old2 + selTab[2] * old3 + selTab[3] * cur);
            dest += chanCount;
            old1 = old2;
            old2 = old3;
            old3 = cur;
            cur = x70_smpBase[x7c_posHi];
        }
        else
        {
            x78_posLo = ovrTest;
            *dest = ClampFull<T>(selTab[0] * old1 + selTab[1] * old2 + selTab[2] * old3 + selTab[3] * cur);
            dest += chanCount;
        }
    }

    x74_old[0] = old1;
    x74_old[1] = old2;
    x74_old[2] = old3;
}

template <typename T>
void EffectChorusImp<T>::SrcInfo::doSrc2(size_t blockSamples, size_t chanCount)
{
    float old1 = x74_old[0];
    float old2 = x74_old[1];
    float old3 = x74_old[2];
    float cur = x70_smpBase[x7c_posHi];

    T* dest = x6c_dest;
    for (size_t i = 0; i < blockSamples; ++i)
    {
        const float* selTab = &rsmpTab12khz[x78_posLo >> 23 & 0x1fc];
        ++x7c_posHi;

        uint64_t ovrTest = uint64_t(x78_posLo) + uint64_t(x80_pitchLo);
        if (ovrTest > UINT32_MAX)
        {
            /* overflow */
            x78_posLo = ovrTest & 0xffffffff;

            if (x7c_posHi == x88_trigger)
                x7c_posHi = x8c_target;

            old1 = old3;
            old2 = cur;
            old3 = x70_smpBase[x7c_posHi];

            ++x7c_posHi;
            if (x7c_posHi == x88_trigger)
                x7c_posHi = x8c_target;

            *dest = ClampFull<T>(selTab[0] * old1 + selTab[1] * old2 + selTab[2] * old3 + selTab[3] * cur);
            dest += chanCount;

            cur = x70_smpBase[x7c_posHi];
        }
        else
        {
            x78_posLo = ovrTest;

            *dest = ClampFull<T>(selTab[0] * old1 + selTab[1] * old2 + selTab[2] * old3 + selTab[3] * cur);
            dest += chanCount;

            old1 = old2;
            old2 = old3;
            old3 = cur;

            if (x7c_posHi == x88_trigger)
                x7c_posHi = x8c_target;

            cur = x70_smpBase[x7c_posHi];
        }
    }

    x74_old[0] = old1;
    x74_old[1] = old2;
    x74_old[2] = old3;
}

template <typename T>
void EffectChorusImp<T>::applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap)
{
    if (m_dirty)
        _update();

    size_t remFrames = frameCount;
    for (size_t f = 0; f < frameCount;)
    {
        uint8_t next = x24_currentLast + 1;
        uint8_t buf = next % 3;
        T* bufs[8] = {
            x0_lastChans[0][buf], x0_lastChans[1][buf], x0_lastChans[2][buf], x0_lastChans[3][buf],
            x0_lastChans[4][buf], x0_lastChans[5][buf], x0_lastChans[6][buf], x0_lastChans[7][buf],
        };

        T* inBuf = audio;
        for (size_t s = 0; f < frameCount && s < m_blockSamples; ++s, ++f)
            for (size_t c = 0; c < chanMap.m_channelCount && c < 8; ++c)
                *bufs[c]++ = *inBuf++;

        x6c_src.x84_pitchHi = (x60_pitchOffset >> 16) + 1;
        x6c_src.x80_pitchLo = (x60_pitchOffset << 16);

        --x64_pitchOffsetPeriodCount;
        if (x64_pitchOffsetPeriodCount == 0)
        {
            x64_pitchOffsetPeriodCount = x68_pitchOffsetPeriod;
            x60_pitchOffset = -x60_pitchOffset;
        }

        T* outBuf = audio;
        size_t bs = std::min(remFrames, size_t(m_blockSamples));
        for (size_t c = 0; c < chanMap.m_channelCount && c < 8; ++c)
        {
            x6c_src.x7c_posHi = x5c_currentPosHi;
            x6c_src.x78_posLo = x58_currentPosLo;

            x6c_src.x6c_dest = outBuf++;
            x6c_src.x70_smpBase = x0_lastChans[c][0];
            x6c_src.x74_old = x28_oldChans[c];

            switch (x6c_src.x84_pitchHi)
            {
            case 0:
                x6c_src.doSrc1(bs, chanMap.m_channelCount);
                break;
            case 1:
                x6c_src.doSrc2(bs, chanMap.m_channelCount);
                break;
            default:
                break;
            }
        }

        audio += bs * chanMap.m_channelCount;
        remFrames -= bs;

        size_t chanPitch = m_blockSamples * AMUSE_CHORUS_NUM_BLOCKS;
        size_t fifteenSamps = 15 * m_sampsPerMs;

        x5c_currentPosHi = x6c_src.x7c_posHi % (chanPitch / fifteenSamps * fifteenSamps);
        x58_currentPosLo = x6c_src.x78_posLo;
        x24_currentLast = buf;
    }
}

template class EffectChorusImp<int16_t>;
template class EffectChorusImp<int32_t>;
template class EffectChorusImp<float>;
}

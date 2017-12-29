#include "amuse/EffectChorus.hpp"
#include "amuse/Common.hpp"
#include "amuse/IBackendVoice.hpp"
#include <cstring>
#include <cmath>

namespace amuse
{

/* clang-format off */
static const float rsmpTab12khz[] =
{
    0.097504f, 0.802216f, 0.101593f, -0.000977f,
    0.093506f, 0.802032f, 0.105804f, -0.001038f,
    0.089600f, 0.801697f, 0.110107f, -0.001160f,
    0.085785f, 0.801178f, 0.114471f, -0.001282f,
    0.082031f, 0.800476f, 0.118927f, -0.001404f,
    0.078369f, 0.799622f, 0.123474f, -0.001526f,
    0.074799f, 0.798615f, 0.128143f, -0.001648f,
    0.071350f, 0.797424f, 0.132874f, -0.001770f,
    0.067963f, 0.796051f, 0.137695f, -0.001923f,
    0.064697f, 0.794525f, 0.142609f, -0.002045f,
    0.061493f, 0.792847f, 0.147614f, -0.002197f,
    0.058350f, 0.790985f, 0.152710f, -0.002319f,
    0.055328f, 0.788940f, 0.157898f, -0.002472f,
    0.052368f, 0.786743f, 0.163177f, -0.002655f,
    0.049500f, 0.784424f, 0.168518f, -0.002808f,
    0.046722f, 0.781891f, 0.173981f, -0.002991f,
    0.044006f, 0.779205f, 0.179504f, -0.003143f,
    0.041412f, 0.776367f, 0.185120f, -0.003326f,
    0.038879f, 0.773376f, 0.190826f, -0.003510f,
    0.036407f, 0.770233f, 0.196594f, -0.003693f,
    0.034027f, 0.766937f, 0.202484f, -0.003876f,
    0.031738f, 0.763489f, 0.208435f, -0.004059f,
    0.029510f, 0.759857f, 0.214447f, -0.004272f,
    0.027374f, 0.756104f, 0.220551f, -0.004456f,
    0.025299f, 0.752197f, 0.226746f, -0.004669f,
    0.023315f, 0.748169f, 0.233002f, -0.004852f,
    0.021393f, 0.743988f, 0.239319f, -0.005066f,
    0.019562f, 0.739655f, 0.245728f, -0.005310f,
    0.017792f, 0.735199f, 0.252197f, -0.005524f,
    0.016052f, 0.730591f, 0.258728f, -0.005707f,
    0.014404f, 0.725861f, 0.265350f, -0.005920f,
    0.012817f, 0.721008f, 0.272034f, -0.006165f,
    0.011322f, 0.716003f, 0.278778f, -0.006378f,
    0.009888f, 0.710907f, 0.285583f, -0.006561f,
    0.008514f, 0.705658f, 0.292450f, -0.006775f,
    0.007202f, 0.700317f, 0.299347f, -0.007019f,
    0.005920f, 0.694855f, 0.306335f, -0.007233f,
    0.004700f, 0.689270f, 0.313385f, -0.007416f,
    0.003571f, 0.683563f, 0.320465f, -0.007629f,
    0.002472f, 0.677734f, 0.327606f, -0.007874f,
    0.001434f, 0.671844f, 0.334778f, -0.008087f,
    0.000458f, 0.665833f, 0.341980f, -0.008270f,
    -0.000488f, 0.659729f, 0.349243f, -0.008453f,
    -0.001343f, 0.653534f, 0.356567f, -0.008636f,
    -0.002167f, 0.647217f, 0.363892f, -0.008850f,
    -0.002960f, 0.640839f, 0.371277f, -0.009033f,
    -0.003693f, 0.634338f, 0.378693f, -0.009216f,
    -0.004364f, 0.627777f, 0.386139f, -0.009338f,
    -0.004974f, 0.621155f, 0.393616f, -0.009491f,
    -0.005585f, 0.614441f, 0.401093f, -0.009644f,
    -0.006134f, 0.607635f, 0.408600f, -0.009796f,
    -0.006653f, 0.600769f, 0.416107f, -0.009918f,
    -0.007141f, 0.593842f, 0.423645f, -0.010010f,
    -0.007568f, 0.586853f, 0.431213f, -0.010132f,
    -0.007965f, 0.579773f, 0.438751f, -0.010223f,
    -0.008331f, 0.572662f, 0.446320f, -0.010284f,
    -0.008667f, 0.565521f, 0.453888f, -0.010345f,
    -0.008972f, 0.558319f, 0.461456f, -0.010406f,
    -0.009216f, 0.551056f, 0.469025f, -0.010406f,
    -0.009460f, 0.543732f, 0.476593f, -0.010406f,
    -0.009674f, 0.536407f, 0.484131f, -0.010376f,
    -0.009857f, 0.529022f, 0.491669f, -0.010376f,
    -0.010010f, 0.521606f, 0.499176f, -0.010315f,
    -0.010132f, 0.514160f, 0.506683f, -0.010254f,
    -0.010254f, 0.506683f, 0.514160f, -0.010132f,
    -0.010315f, 0.499176f, 0.521606f, -0.010010f,
    -0.010376f, 0.491669f, 0.529022f, -0.009857f,
    -0.010376f, 0.484131f, 0.536407f, -0.009674f,
    -0.010406f, 0.476593f, 0.543732f, -0.009460f,
    -0.010406f, 0.469025f, 0.551056f, -0.009216f,
    -0.010406f, 0.461456f, 0.558319f, -0.008972f,
    -0.010345f, 0.453888f, 0.565521f, -0.008667f,
    -0.010284f, 0.446320f, 0.572662f, -0.008331f,
    -0.010223f, 0.438751f, 0.579773f, -0.007965f,
    -0.010132f, 0.431213f, 0.586853f, -0.007568f,
    -0.010010f, 0.423645f, 0.593842f, -0.007141f,
    -0.009918f, 0.416107f, 0.600769f, -0.006653f,
    -0.009796f, 0.408600f, 0.607635f, -0.006134f,
    -0.009644f, 0.401093f, 0.614441f, -0.005585f,
    -0.009491f, 0.393616f, 0.621155f, -0.004974f,
    -0.009338f, 0.386139f, 0.627777f, -0.004364f,
    -0.009216f, 0.378693f, 0.634338f, -0.003693f,
    -0.009033f, 0.371277f, 0.640839f, -0.002960f,
    -0.008850f, 0.363892f, 0.647217f, -0.002167f,
    -0.008636f, 0.356567f, 0.653534f, -0.001343f,
    -0.008453f, 0.349243f, 0.659729f, -0.000488f,
    -0.008270f, 0.341980f, 0.665833f, 0.000458f,
    -0.008087f, 0.334778f, 0.671844f, 0.001434f,
    -0.007874f, 0.327606f, 0.677734f, 0.002472f,
    -0.007629f, 0.320465f, 0.683563f, 0.003571f,
    -0.007416f, 0.313385f, 0.689270f, 0.004700f,
    -0.007233f, 0.306335f, 0.694855f, 0.005920f,
    -0.007019f, 0.299347f, 0.700317f, 0.007202f,
    -0.006775f, 0.292450f, 0.705658f, 0.008514f,
    -0.006561f, 0.285583f, 0.710907f, 0.009888f,
    -0.006378f, 0.278778f, 0.716003f, 0.011322f,
    -0.006165f, 0.272034f, 0.721008f, 0.012817f,
    -0.005920f, 0.265350f, 0.725861f, 0.014404f,
    -0.005707f, 0.258728f, 0.730591f, 0.016052f,
    -0.005524f, 0.252197f, 0.735199f, 0.017792f,
    -0.005310f, 0.245728f, 0.739655f, 0.019562f,
    -0.005066f, 0.239319f, 0.743988f, 0.021393f,
    -0.004852f, 0.233002f, 0.748169f, 0.023315f,
    -0.004669f, 0.226746f, 0.752197f, 0.025299f,
    -0.004456f, 0.220551f, 0.756104f, 0.027374f,
    -0.004272f, 0.214447f, 0.759857f, 0.029510f,
    -0.004059f, 0.208435f, 0.763489f, 0.031738f,
    -0.003876f, 0.202484f, 0.766937f, 0.034027f,
    -0.003693f, 0.196594f, 0.770233f, 0.036407f,
    -0.003510f, 0.190826f, 0.773376f, 0.038879f,
    -0.003326f, 0.185120f, 0.776367f, 0.041412f,
    -0.003143f, 0.179504f, 0.779205f, 0.044006f,
    -0.002991f, 0.173981f, 0.781891f, 0.046722f,
    -0.002808f, 0.168518f, 0.784424f, 0.049500f,
    -0.002655f, 0.163177f, 0.786743f, 0.052368f,
    -0.002472f, 0.157898f, 0.788940f, 0.055328f,
    -0.002319f, 0.152710f, 0.790985f, 0.058350f,
    -0.002197f, 0.147614f, 0.792847f, 0.061493f,
    -0.002045f, 0.142609f, 0.794525f, 0.064697f,
    -0.001923f, 0.137695f, 0.796051f, 0.067963f,
    -0.001770f, 0.132874f, 0.797424f, 0.071350f,
    -0.001648f, 0.128143f, 0.798615f, 0.074799f,
    -0.001526f, 0.123474f, 0.799622f, 0.078369f,
    -0.001404f, 0.118927f, 0.800476f, 0.082031f,
    -0.001282f, 0.114471f, 0.801178f, 0.085785f,
    -0.001160f, 0.110107f, 0.801697f, 0.089600f,
    -0.001038f, 0.105804f, 0.802032f, 0.093506f,
    -0.000977f, 0.101593f, 0.802216f, 0.097504f,
};
/* clang-format on */

EffectChorus::EffectChorus(uint32_t baseDelay, uint32_t variation, uint32_t period)
: x90_baseDelay(clamp(5u, baseDelay, 15u))
, x94_variation(clamp(0u, variation, 5u))
, x98_period(clamp(500u, period, 10000u))
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

#include "amuse/EffectReverb.hpp"
#include "amuse/IBackendVoice.hpp"
#include <cmath>
#include <string.h>

namespace amuse
{

/* Comb-filter delays */
static const size_t CTapDelays[] =
{
    1789,
    1999,
    2333
};

/* All-pass-filter delays */
static const size_t APTapDelays[] =
{
    433,
    149
};

/* Per-channel low-pass delays (Hi-quality reverb only) */
static const size_t LPTapDelays[] =
{
    47,
    73,
    67,
    57,
    43,
    57,
    83,
    73
};

void ReverbDelayLine::allocate(int32_t delay)
{
    delay += 2;
    x8_length = delay * sizeof(float);
    xc_inputs.reset(new float[delay]);
    memset(xc_inputs.get(), 0, x8_length);
    x10_lastInput = 0.f;
    setdelay(delay / 2);
    x0_inPoint = 0;
    x4_outPoint = 0;
}

void ReverbDelayLine::setdelay(int32_t delay)
{
    x4_outPoint = x0_inPoint - delay * sizeof(float);
    while (x4_outPoint < 0)
        x4_outPoint += x8_length;
}

EffectReverb::EffectReverb(float coloration, float mix, float time,
                           float damping, float preDelay)
: x140_x1c8_coloration(clamp(0.f, coloration, 1.f)),
  x144_x1cc_mix(clamp(0.f, mix, 1.f)),
  x148_x1d0_time(clamp(0.01f, time, 10.f)),
  x14c_x1d4_damping(clamp(0.f, damping, 1.f)),
  x150_x1d8_preDelay(clamp(0.f, preDelay, 0.1f))
{}

EffectReverbHi::EffectReverbHi(float coloration, float mix, float time,
                               float damping, float preDelay, float crosstalk)
: EffectReverb(coloration, mix, time, damping, preDelay),
  x1dc_crosstalk(clamp(0.f, crosstalk, 1.0f))
{}

template <typename T>
EffectReverbStdImp<T>::EffectReverbStdImp(float coloration, float mix, float time,
                                          float damping, float preDelay, double sampleRate)
: EffectReverb(coloration, mix, time, damping, preDelay),
  m_sampleRate(sampleRate)
{}

template <typename T>
void EffectReverbStdImp<T>::_update()
{
    float timeSamples = x148_x1d0_time * m_sampleRate;
    for (int c=0 ; c<8 ; ++c)
    {
        for (int t=0 ; t<2 ; ++t)
        {
            ReverbDelayLine& combLine = x78_C[c][t];
            size_t tapDelay = CTapDelays[t] * m_sampleRate / 32000.0;
            combLine.allocate(tapDelay);
            combLine.setdelay(tapDelay);
            xf4_combCoef[c][t] = std::pow(10.f, tapDelay * -3 / timeSamples);
        }

        for (int t=0 ; t<2 ; ++t)
        {
            ReverbDelayLine& allPassLine = x0_AP[c][t];
            size_t tapDelay = APTapDelays[t] * m_sampleRate / 32000.0;
            allPassLine.allocate(tapDelay);
            allPassLine.setdelay(tapDelay);
        }
    }

    xf0_allPassCoef = x140_x1c8_coloration;
    x118_level = x144_x1cc_mix;
    x11c_damping = x14c_x1d4_damping;

    if (x11c_damping < 0.05f)
        x11c_damping = 0.05f;

    x11c_damping = 1.f - (x11c_damping * 0.8f + 0.05);

    if (x150_x1d8_preDelay != 0.f)
    {
        x120_preDelayTime = m_sampleRate * x150_x1d8_preDelay;
        for (int i=0 ; i<8 ; ++i)
        {
            x124_preDelayLine[i].reset(new float[x120_preDelayTime]);
            memset(x124_preDelayLine[i].get(), 0, x120_preDelayTime * sizeof(float));
            x130_preDelayPtr[i] = x124_preDelayLine[i].get();
        }
    }
    else
    {
        x120_preDelayTime = 0;
        for (int i=0 ; i<8 ; ++i)
        {
            x124_preDelayLine[i] = nullptr;
            x130_preDelayPtr[i] = nullptr;
        }
    }

    m_dirty = false;
}

template <typename T>
void EffectReverbStdImp<T>::applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap)
{
    if (m_dirty)
        _update();

    float dampWet = x118_level * 0.6f;
    float dampDry = 0.6f - dampWet;

    for (size_t f=0 ; f<frameCount ;)
    {
        for (int c=0 ; c<chanMap.m_channelCount ; ++c)
        {
            float* combCoefs = xf4_combCoef[c];
            float& lpLastOut = x10c_lpLastout[c];
            float* preDelayLine = x124_preDelayLine[c].get();
            float* preDelayPtr = x130_preDelayPtr[c];
            float* lastPreDelaySamp = &preDelayLine[x120_preDelayTime - 1];

            ReverbDelayLine* linesC = x78_C[c];
            ReverbDelayLine* linesAP = x0_AP[c];

            float sample = audio[c];
            for (int s=1 ; s<160 && f<frameCount ; ++s, ++f)
            {
                /* Pre-delay stage */
                float sample2 = sample;
                if (x120_preDelayTime != 0)
                {
                    sample2 = *preDelayPtr;
                    *preDelayPtr = sample;
                    preDelayPtr += 4;
                    if (preDelayPtr == lastPreDelaySamp)
                        preDelayPtr = preDelayLine;
                }

                /* Comb filter stage */
                linesC[0].xc_inputs[linesC[0].x0_inPoint] = combCoefs[0] * linesC[0].x10_lastInput + sample2;
                linesC[0].x0_inPoint += 4;

                linesC[1].xc_inputs[linesC[1].x0_inPoint] = combCoefs[1] * linesC[1].x10_lastInput + sample2;
                linesC[1].x0_inPoint += 4;

                linesC[0].x10_lastInput = linesC[0].xc_inputs[linesC[0].x4_outPoint];
                linesC[0].x4_outPoint += 4;

                linesC[1].x10_lastInput = linesC[1].xc_inputs[linesC[1].x4_outPoint];
                linesC[1].x4_outPoint += 4;

                if (linesC[0].x0_inPoint == linesC[0].x8_length)
                    linesC[0].x0_inPoint = 0;

                if (linesC[1].x0_inPoint == linesC[1].x8_length)
                    linesC[1].x0_inPoint = 0;

                if (linesC[0].x4_outPoint == linesC[0].x8_length)
                    linesC[0].x4_outPoint = 0;

                if (linesC[1].x4_outPoint == linesC[1].x8_length)
                    linesC[1].x4_outPoint = 0;

                /* All-pass filter stage */
                linesAP[0].xc_inputs[linesAP[0].x0_inPoint] =
                    xf0_allPassCoef * linesAP[0].x10_lastInput + linesC[0].x10_lastInput + linesC[1].x10_lastInput;
                float lowPass = -(xf0_allPassCoef * linesAP[0].xc_inputs[linesAP[0].x0_inPoint] -
                                  linesAP[0].x10_lastInput);
                linesAP[0].x0_inPoint += 4;

                linesAP[0].x10_lastInput = linesAP[0].xc_inputs[linesAP[0].x4_outPoint];
                linesAP[0].x4_outPoint += 4;

                if (linesAP[0].x0_inPoint == linesAP[0].x8_length)
                    linesAP[0].x0_inPoint = 0;

                if (linesAP[0].x4_outPoint == linesAP[0].x8_length)
                    linesAP[0].x4_outPoint = 0;

                lpLastOut = x11c_damping * lpLastOut + lowPass * 0.3f;
                linesAP[1].xc_inputs[linesAP[1].x0_inPoint] = xf0_allPassCoef * linesAP[1].x10_lastInput + lpLastOut;
                float allPass = -(xf0_allPassCoef * linesAP[1].xc_inputs[linesAP[1].x0_inPoint] -
                                  linesAP[1].x10_lastInput);
                linesAP[1].x0_inPoint += 4;

                linesAP[1].x10_lastInput = linesAP[1].xc_inputs[linesAP[1].x4_outPoint];
                linesAP[1].x4_outPoint += 4;

                if (linesAP[1].x0_inPoint == linesAP[1].x8_length)
                    linesAP[1].x0_inPoint = 0;

                if (linesAP[1].x4_outPoint == linesAP[1].x8_length)
                    linesAP[1].x4_outPoint = 0;

                /* Mix out */
                audio[(s-1) * chanMap.m_channelCount + c] = ClampFull<T>(dampWet * allPass + dampDry * sample);
                sample = audio[s * chanMap.m_channelCount + c];
            }
            x130_preDelayPtr[c] = preDelayPtr;
        }
        audio += chanMap.m_channelCount * 160;
    }
}

template <typename T>
EffectReverbHiImp<T>::EffectReverbHiImp(float coloration, float mix, float time,
                                        float damping, float preDelay, float crosstalk,
                                        double sampleRate)
: EffectReverbHi(coloration, mix, time, damping, preDelay, crosstalk),
  m_sampleRate(sampleRate)
{
    _update();
}

template <typename T>
void EffectReverbHiImp<T>::_update()
{
    float timeSamples = x148_x1d0_time * m_sampleRate;
    for (int c=0 ; c<8 ; ++c)
    {
        for (int t=0 ; t<2 ; ++t)
        {
            ReverbDelayLine& combLine = xb4_C[c][t];
            size_t tapDelay = CTapDelays[t] * m_sampleRate / 32000.0;
            combLine.allocate(tapDelay);
            combLine.setdelay(tapDelay);
            x16c_combCoef[c][t] = std::pow(10.f, tapDelay * -3 / timeSamples);
        }

        for (int t=0 ; t<2 ; ++t)
        {
            ReverbDelayLine& allPassLine = x0_AP[c][t];
            size_t tapDelay = APTapDelays[t] * m_sampleRate / 32000.0;
            allPassLine.allocate(tapDelay);
            allPassLine.setdelay(tapDelay);
        }

        ReverbDelayLine& lpLine = x78_LP[c];
        size_t tapDelay = LPTapDelays[c] * m_sampleRate / 32000.0;
        lpLine.allocate(tapDelay);
        lpLine.setdelay(tapDelay);
    }

    x168_allPassCoef = x140_x1c8_coloration;
    x19c_level = x144_x1cc_mix;
    x1a0_damping = x14c_x1d4_damping;

    if (x1a0_damping < 0.05f)
        x1a0_damping = 0.05f;

    x1a0_damping = 1.f - (x1a0_damping * 0.8f + 0.05);

    if (x150_x1d8_preDelay != 0.f)
    {
        x1a4_preDelayTime = m_sampleRate * x150_x1d8_preDelay;
        for (int i=0 ; i<8 ; ++i)
        {
            x1ac_preDelayLine[i].reset(new float[x1a4_preDelayTime]);
            memset(x1ac_preDelayLine[i].get(), 0, x1a4_preDelayTime * sizeof(float));
            x1b8_preDelayPtr[i] = x1ac_preDelayLine[i].get();
        }
    }
    else
    {
        x1a4_preDelayTime = 0;
        for (int i=0 ; i<8 ; ++i)
        {
            x1ac_preDelayLine[i] = nullptr;
            x1b8_preDelayPtr[i] = nullptr;
        }
    }

    m_dirty = false;
}

template <typename T>
void EffectReverbHiImp<T>::_handleReverb(T* audio, int c, int chanCount, int sampleCount)
{
    float dampWet = x19c_level * 0.6f;
    float dampDry = 0.6f - dampWet;

    float* combCoefs = x16c_combCoef[c];
    float& lpLastOut = x190_lpLastout[c];
    float* preDelayLine = x1ac_preDelayLine[c].get();
    float* preDelayPtr = x1b8_preDelayPtr[c];
    float* lastPreDelaySamp = &preDelayLine[x1a4_preDelayTime - 1];

    ReverbDelayLine* linesC = xb4_C[c];
    ReverbDelayLine* linesAP = x0_AP[c];
    ReverbDelayLine& lineLP = x78_LP[c];

    float allPassCoef = x168_allPassCoef;
    float damping = x1a0_damping;
    int32_t preDelayTime = x1a4_preDelayTime;

    float sample = audio[c];
    for (int s=1 ; s<sampleCount ; ++s)
    {
        /* Pre-delay stage */
        float sample2 = sample;
        if (preDelayTime != 0)
        {
            sample2 = *preDelayPtr;
            *preDelayPtr = sample;
            preDelayPtr += 4;
            if (preDelayPtr == lastPreDelaySamp)
                preDelayPtr = preDelayLine;
        }

        /* Comb filter stage */
        linesC[0].xc_inputs[linesC[0].x0_inPoint] = combCoefs[0] * linesC[0].x10_lastInput + sample2;
        linesC[0].x0_inPoint += 4;

        linesC[1].xc_inputs[linesC[1].x0_inPoint] = combCoefs[1] * linesC[1].x10_lastInput + sample2;
        linesC[1].x0_inPoint += 4;

        linesC[0].x10_lastInput = linesC[0].xc_inputs[linesC[0].x4_outPoint];
        linesC[0].x4_outPoint += 4;

        linesC[1].x10_lastInput = linesC[1].xc_inputs[linesC[1].x4_outPoint];
        linesC[1].x4_outPoint += 4;

        if (linesC[0].x0_inPoint == linesC[0].x8_length)
            linesC[0].x0_inPoint = 0;

        if (linesC[1].x0_inPoint == linesC[1].x8_length)
            linesC[1].x0_inPoint = 0;

        if (linesC[0].x4_outPoint == linesC[0].x8_length)
            linesC[0].x4_outPoint = 0;

        if (linesC[1].x4_outPoint == linesC[1].x8_length)
            linesC[1].x4_outPoint = 0;

        /* All-pass filter stage */
        linesAP[0].xc_inputs[linesAP[0].x0_inPoint] =
            allPassCoef * linesAP[0].x10_lastInput + linesC[0].x10_lastInput + linesC[1].x10_lastInput;

        linesAP[1].xc_inputs[linesAP[1].x0_inPoint] =
            allPassCoef * linesAP[1].x10_lastInput -
            (allPassCoef * linesAP[0].xc_inputs[linesAP[0].x0_inPoint] - linesAP[0].x10_lastInput);

        float lowPass = -(allPassCoef * linesAP[1].xc_inputs[linesAP[1].x0_inPoint] - linesAP[1].x10_lastInput);
        linesAP[0].x0_inPoint += 4;
        linesAP[1].x0_inPoint += 4;

        if (linesAP[0].x0_inPoint == linesAP[0].x8_length)
            linesAP[0].x0_inPoint = 0;

        if (linesAP[1].x0_inPoint == linesAP[1].x8_length)
            linesAP[1].x0_inPoint = 0;

        linesAP[0].x10_lastInput = linesAP[0].xc_inputs[linesAP[0].x4_outPoint];
        linesAP[0].x4_outPoint += 4;

        linesAP[1].x10_lastInput = linesAP[1].xc_inputs[linesAP[1].x4_outPoint];
        linesAP[1].x4_outPoint += 4;

        if (linesAP[0].x4_outPoint == linesAP[0].x8_length)
            linesAP[0].x4_outPoint = 0;

        if (linesAP[1].x4_outPoint == linesAP[1].x8_length)
            linesAP[1].x4_outPoint = 0;

        lpLastOut = damping * lpLastOut + lowPass * 0.3f;
        lineLP.xc_inputs[lineLP.x0_inPoint] = allPassCoef * lineLP.x10_lastInput + lpLastOut;
        float allPass = -(allPassCoef * lineLP.xc_inputs[lineLP.x0_inPoint] - lineLP.x10_lastInput);
        lineLP.x0_inPoint += 4;

        lineLP.x10_lastInput = lineLP.xc_inputs[lineLP.x4_outPoint];
        lineLP.x4_outPoint += 4;

        if (lineLP.x0_inPoint == lineLP.x8_length)
            lineLP.x0_inPoint = 0;

        if (lineLP.x4_outPoint == lineLP.x8_length)
            lineLP.x4_outPoint = 0;

        /* Mix out */
        audio[(s-1) * chanCount + c] = ClampFull<T>(dampWet * allPass + dampDry * sample);
        sample = audio[s * chanCount + c];
    }

    x1b8_preDelayPtr[c] = preDelayPtr;
}

template <typename T>
void EffectReverbHiImp<T>::_doCrosstalk(T* audio, float wet, float dry, int chanCount, int sampleCount)
{
    for (int i=0 ; i<sampleCount ; ++i)
    {
        T* base = &audio[chanCount*i];
        float allWet = 0;
        for (int c=0 ; c<chanCount ; ++c)
        {
            allWet += base[c] * wet;
            base[c] *= dry;
        }
        for (int c=0 ; c<chanCount ; ++c)
            base[c] = ClampFull<T>(base[c] + allWet);
    }
}

template <typename T>
void EffectReverbHiImp<T>::applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap)
{
    if (m_dirty)
        _update();

    for (size_t f=0 ; f<frameCount ; f+=160)
    {
        size_t blockSamples = std::min(size_t(160), frameCount - f);
        for (int i=0 ; i<chanMap.m_channelCount ; ++i)
        {
            if (i == 0 && x1a8_internalCrosstalk != 0.f)
            {
                float crossWet = x1a8_internalCrosstalk * 0.5;
                _doCrosstalk(audio, crossWet, 1.f - crossWet, chanMap.m_channelCount, blockSamples);
            }
            _handleReverb(audio, i, chanMap.m_channelCount, blockSamples);
        }
        audio += chanMap.m_channelCount * 160;
    }
}

template class EffectReverbStdImp<int16_t>;
template class EffectReverbStdImp<int32_t>;
template class EffectReverbStdImp<float>;

template class EffectReverbHiImp<int16_t>;
template class EffectReverbHiImp<int32_t>;
template class EffectReverbHiImp<float>;

}

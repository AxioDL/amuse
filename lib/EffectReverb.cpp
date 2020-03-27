#include "amuse/EffectReverb.hpp"

#include <algorithm>
#include <cmath>

#include "amuse/IBackendVoice.hpp"

namespace amuse {

/* clang-format off */

/* Comb-filter delays */
constexpr std::array<size_t, 3> CTapDelays{
    1789,
    1999,
    2333
};

/* All-pass-filter delays */
constexpr std::array<size_t, 2> APTapDelays{
    433,
    149,
};

/* Per-channel low-pass delays (Hi-quality reverb only) */
constexpr std::array<size_t, 8> LPTapDelays{
    47,
    73,
    67,
    57,
    43,
    57,
    83,
    73
};

/* clang-format on */

void ReverbDelayLine::allocate(int32_t delay) {
  delay += 2;
  x8_length = delay;
  xc_inputs = std::make_unique<float[]>(delay);
  x10_lastInput = 0.f;
  setdelay(delay / 2);
  x0_inPoint = 0;
  x4_outPoint = 0;
}

void ReverbDelayLine::setdelay(int32_t delay) {
  x4_outPoint = x0_inPoint - delay;
  while (x4_outPoint < 0)
    x4_outPoint += x8_length;
}

EffectReverbStd::EffectReverbStd(float coloration, float mix, float time, float damping, float preDelay)
: x140_x1c8_coloration(std::clamp(coloration, 0.f, 1.f))
, x144_x1cc_mix(std::clamp(mix, 0.f, 1.f))
, x148_x1d0_time(std::clamp(time, 0.01f, 10.f))
, x14c_x1d4_damping(std::clamp(damping, 0.f, 1.f))
, x150_x1d8_preDelay(std::clamp(preDelay, 0.f, 0.1f)) {}

EffectReverbHi::EffectReverbHi(float coloration, float mix, float time, float damping, float preDelay, float crosstalk)
: EffectReverbStd(coloration, mix, time, damping, preDelay), x1dc_crosstalk(std::clamp(crosstalk, 0.f, 1.0f)) {}

template <typename T>
EffectReverbStdImp<T>::EffectReverbStdImp(float coloration, float mix, float time, float damping, float preDelay,
                                          double sampleRate)
: EffectReverbStd(coloration, mix, time, damping, preDelay) {
  _setup(sampleRate);
}

template <typename T>
void EffectReverbStdImp<T>::_setup(double sampleRate) {
  m_sampleRate = sampleRate;
  _update();
}

template <typename T>
void EffectReverbStdImp<T>::_update() {
  float timeSamples = x148_x1d0_time * m_sampleRate;
  double rateRatio = m_sampleRate / NativeSampleRate;
  for (size_t c = 0; c < NumChannels; ++c) {
    for (size_t t = 0; t < x78_C[c].size(); ++t) {
      ReverbDelayLine& combLine = x78_C[c][t];
      size_t tapDelay = CTapDelays[t] * rateRatio;
      combLine.allocate(tapDelay);
      combLine.setdelay(tapDelay);
      xf4_combCoef[c][t] = std::pow(10.f, tapDelay * -3.f / timeSamples);
    }

    for (size_t t = 0; t < x0_AP[c].size(); ++t) {
      ReverbDelayLine& allPassLine = x0_AP[c][t];
      size_t tapDelay = APTapDelays[t] * rateRatio;
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

  if (x150_x1d8_preDelay != 0.f) {
    x120_preDelayTime = m_sampleRate * x150_x1d8_preDelay;
    for (size_t i = 0; i < NumChannels; ++i) {
      x124_preDelayLine[i] = std::make_unique<float[]>(x120_preDelayTime);
      x130_preDelayPtr[i] = x124_preDelayLine[i].get();
    }
  } else {
    x120_preDelayTime = 0;
    for (auto& delayLine : x124_preDelayLine) {
      delayLine.reset();
    }
    x130_preDelayPtr.fill(nullptr);
  }

  m_dirty = false;
}

template <typename T>
void EffectReverbStdImp<T>::applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap) {
  if (m_dirty)
    _update();

  const float dampWet = x118_level * 0.6f;
  const float dampDry = 0.6f - dampWet;

  for (size_t f = 0; f < frameCount; f += 160) {
    for (unsigned c = 0; c < chanMap.m_channelCount; ++c) {
      const auto& combCoefs = xf4_combCoef[c];
      float& lpLastOut = x10c_lpLastout[c];
      float* preDelayLine = x124_preDelayLine[c].get();
      float* preDelayPtr = x130_preDelayPtr[c];
      float* lastPreDelaySamp = &preDelayLine[x120_preDelayTime - 1];

      auto& linesC = x78_C[c];
      auto& linesAP = x0_AP[c];

      const int procSamples = std::min(size_t(160), frameCount - f);
      for (int s = 0; s < procSamples; ++s) {
        const float sample = audio[s * chanMap.m_channelCount + c];

        /* Pre-delay stage */
        float sample2 = sample;
        if (x120_preDelayTime != 0) {
          sample2 = *preDelayPtr;
          *preDelayPtr = sample;
          preDelayPtr += 1;
          if (preDelayPtr == lastPreDelaySamp)
            preDelayPtr = preDelayLine;
        }

        /* Comb filter stage */
        linesC[0].xc_inputs[linesC[0].x0_inPoint] = combCoefs[0] * linesC[0].x10_lastInput + sample2;
        linesC[0].x0_inPoint += 1;

        linesC[1].xc_inputs[linesC[1].x0_inPoint] = combCoefs[1] * linesC[1].x10_lastInput + sample2;
        linesC[1].x0_inPoint += 1;

        linesC[0].x10_lastInput = linesC[0].xc_inputs[linesC[0].x4_outPoint];
        linesC[0].x4_outPoint += 1;

        linesC[1].x10_lastInput = linesC[1].xc_inputs[linesC[1].x4_outPoint];
        linesC[1].x4_outPoint += 1;

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
        const float lowPass =
            -(xf0_allPassCoef * linesAP[0].xc_inputs[linesAP[0].x0_inPoint] - linesAP[0].x10_lastInput);
        linesAP[0].x0_inPoint += 1;

        linesAP[0].x10_lastInput = linesAP[0].xc_inputs[linesAP[0].x4_outPoint];
        linesAP[0].x4_outPoint += 1;

        if (linesAP[0].x0_inPoint == linesAP[0].x8_length)
          linesAP[0].x0_inPoint = 0;

        if (linesAP[0].x4_outPoint == linesAP[0].x8_length)
          linesAP[0].x4_outPoint = 0;

        lpLastOut = x11c_damping * lpLastOut + lowPass * 0.3f;
        linesAP[1].xc_inputs[linesAP[1].x0_inPoint] = xf0_allPassCoef * linesAP[1].x10_lastInput + lpLastOut;
        const float allPass =
            -(xf0_allPassCoef * linesAP[1].xc_inputs[linesAP[1].x0_inPoint] - linesAP[1].x10_lastInput);
        linesAP[1].x0_inPoint += 1;

        linesAP[1].x10_lastInput = linesAP[1].xc_inputs[linesAP[1].x4_outPoint];
        linesAP[1].x4_outPoint += 1;

        if (linesAP[1].x0_inPoint == linesAP[1].x8_length)
          linesAP[1].x0_inPoint = 0;

        if (linesAP[1].x4_outPoint == linesAP[1].x8_length)
          linesAP[1].x4_outPoint = 0;

        /* Mix out */
        audio[s * chanMap.m_channelCount + c] = ClampFull<T>(dampWet * allPass + dampDry * sample);
      }
      x130_preDelayPtr[c] = preDelayPtr;
    }
    audio += chanMap.m_channelCount * 160;
  }
}

template <typename T>
EffectReverbHiImp<T>::EffectReverbHiImp(float coloration, float mix, float time, float damping, float preDelay,
                                        float crosstalk, double sampleRate)
: EffectReverbHi(coloration, mix, time, damping, preDelay, crosstalk) {
  _setup(sampleRate);
}

template <typename T>
void EffectReverbHiImp<T>::_setup(double sampleRate) {
  m_sampleRate = sampleRate;
  _update();
}

template <typename T>
void EffectReverbHiImp<T>::_update() {
  const float timeSamples = x148_x1d0_time * m_sampleRate;
  const double rateRatio = m_sampleRate / NativeSampleRate;

  for (size_t c = 0; c < NumChannels; ++c) {
    for (size_t t = 0; t < xb4_C[c].size(); ++t) {
      ReverbDelayLine& combLine = xb4_C[c][t];
      const size_t tapDelay = CTapDelays[t] * rateRatio;
      combLine.allocate(tapDelay);
      combLine.setdelay(tapDelay);
      x16c_combCoef[c][t] = std::pow(10.f, tapDelay * -3.f / timeSamples);
    }

    for (size_t t = 0; t < x0_AP[c].size(); ++t) {
      ReverbDelayLine& allPassLine = x0_AP[c][t];
      const size_t tapDelay = APTapDelays[t] * rateRatio;
      allPassLine.allocate(tapDelay);
      allPassLine.setdelay(tapDelay);
    }

    ReverbDelayLine& lpLine = x78_LP[c];
    const size_t tapDelay = LPTapDelays[c] * rateRatio;
    lpLine.allocate(tapDelay);
    lpLine.setdelay(tapDelay);
  }

  x168_allPassCoef = x140_x1c8_coloration;
  x19c_level = x144_x1cc_mix;
  x1a0_damping = x14c_x1d4_damping;

  if (x1a0_damping < 0.05f)
    x1a0_damping = 0.05f;

  x1a0_damping = 1.f - (x1a0_damping * 0.8f + 0.05);

  if (x150_x1d8_preDelay != 0.f) {
    x1a4_preDelayTime = m_sampleRate * x150_x1d8_preDelay;
    for (size_t i = 0; i < NumChannels; ++i) {
      x1ac_preDelayLine[i] = std::make_unique<float[]>(x1a4_preDelayTime);
      x1b8_preDelayPtr[i] = x1ac_preDelayLine[i].get();
    }
  } else {
    x1a4_preDelayTime = 0;
    for (auto& delayLine : x1ac_preDelayLine) {
      delayLine.reset();
    }
    x1b8_preDelayPtr.fill(nullptr);
  }

  x1a8_internalCrosstalk = x1dc_crosstalk;
  m_dirty = false;
}

template <typename T>
void EffectReverbHiImp<T>::_handleReverb(T* audio, int c, int chanCount, int sampleCount) {
  const float dampWet = x19c_level * 0.6f;
  const float dampDry = 0.6f - dampWet;

  const auto& combCoefs = x16c_combCoef[c];
  float& lpLastOut = x190_lpLastout[c];
  float* preDelayLine = x1ac_preDelayLine[c].get();
  float* preDelayPtr = x1b8_preDelayPtr[c];
  float* lastPreDelaySamp = &preDelayLine[x1a4_preDelayTime - 1];

  auto& linesC = xb4_C[c];
  auto& linesAP = x0_AP[c];
  ReverbDelayLine& lineLP = x78_LP[c];

  const float allPassCoef = x168_allPassCoef;
  const float damping = x1a0_damping;
  const int32_t preDelayTime = x1a4_preDelayTime;

  for (int s = 0; s < sampleCount; ++s) {
    const float sample = audio[s * chanCount + c];

    /* Pre-delay stage */
    float sample2 = sample;
    if (preDelayTime != 0) {
      sample2 = *preDelayPtr;
      *preDelayPtr = sample;
      preDelayPtr += 1;
      if (preDelayPtr == lastPreDelaySamp)
        preDelayPtr = preDelayLine;
    }

    /* Comb filter stage */
    linesC[0].xc_inputs[linesC[0].x0_inPoint] = combCoefs[0] * linesC[0].x10_lastInput + sample2;
    linesC[0].x0_inPoint += 1;

    linesC[1].xc_inputs[linesC[1].x0_inPoint] = combCoefs[1] * linesC[1].x10_lastInput + sample2;
    linesC[1].x0_inPoint += 1;

    linesC[2].xc_inputs[linesC[2].x0_inPoint] = combCoefs[2] * linesC[2].x10_lastInput + sample2;
    linesC[2].x0_inPoint += 1;

    linesC[0].x10_lastInput = linesC[0].xc_inputs[linesC[0].x4_outPoint];
    linesC[0].x4_outPoint += 1;

    linesC[1].x10_lastInput = linesC[1].xc_inputs[linesC[1].x4_outPoint];
    linesC[1].x4_outPoint += 1;

    linesC[2].x10_lastInput = linesC[2].xc_inputs[linesC[2].x4_outPoint];
    linesC[2].x4_outPoint += 1;

    if (linesC[0].x0_inPoint == linesC[0].x8_length)
      linesC[0].x0_inPoint = 0;

    if (linesC[1].x0_inPoint == linesC[1].x8_length)
      linesC[1].x0_inPoint = 0;

    if (linesC[2].x0_inPoint == linesC[2].x8_length)
      linesC[2].x0_inPoint = 0;

    if (linesC[0].x4_outPoint == linesC[0].x8_length)
      linesC[0].x4_outPoint = 0;

    if (linesC[1].x4_outPoint == linesC[1].x8_length)
      linesC[1].x4_outPoint = 0;

    if (linesC[2].x4_outPoint == linesC[2].x8_length)
      linesC[2].x4_outPoint = 0;

    /* All-pass filter stage */
    linesAP[0].xc_inputs[linesAP[0].x0_inPoint] = allPassCoef * linesAP[0].x10_lastInput + linesC[0].x10_lastInput +
                                                  linesC[1].x10_lastInput + linesC[2].x10_lastInput;

    linesAP[1].xc_inputs[linesAP[1].x0_inPoint] =
        allPassCoef * linesAP[1].x10_lastInput -
        (allPassCoef * linesAP[0].xc_inputs[linesAP[0].x0_inPoint] - linesAP[0].x10_lastInput);

    const float lowPass = -(allPassCoef * linesAP[1].xc_inputs[linesAP[1].x0_inPoint] - linesAP[1].x10_lastInput);
    linesAP[0].x0_inPoint += 1;
    linesAP[1].x0_inPoint += 1;

    if (linesAP[0].x0_inPoint == linesAP[0].x8_length)
      linesAP[0].x0_inPoint = 0;

    if (linesAP[1].x0_inPoint == linesAP[1].x8_length)
      linesAP[1].x0_inPoint = 0;

    linesAP[0].x10_lastInput = linesAP[0].xc_inputs[linesAP[0].x4_outPoint];
    linesAP[0].x4_outPoint += 1;

    linesAP[1].x10_lastInput = linesAP[1].xc_inputs[linesAP[1].x4_outPoint];
    linesAP[1].x4_outPoint += 1;

    if (linesAP[0].x4_outPoint == linesAP[0].x8_length)
      linesAP[0].x4_outPoint = 0;

    if (linesAP[1].x4_outPoint == linesAP[1].x8_length)
      linesAP[1].x4_outPoint = 0;

    lpLastOut = damping * lpLastOut + lowPass * 0.3f;
    lineLP.xc_inputs[lineLP.x0_inPoint] = allPassCoef * lineLP.x10_lastInput + lpLastOut;
    const float allPass = -(allPassCoef * lineLP.xc_inputs[lineLP.x0_inPoint] - lineLP.x10_lastInput);
    lineLP.x0_inPoint += 1;

    lineLP.x10_lastInput = lineLP.xc_inputs[lineLP.x4_outPoint];
    lineLP.x4_outPoint += 1;

    if (lineLP.x0_inPoint == lineLP.x8_length)
      lineLP.x0_inPoint = 0;

    if (lineLP.x4_outPoint == lineLP.x8_length)
      lineLP.x4_outPoint = 0;

    /* Mix out */
    audio[s * chanCount + c] = ClampFull<T>(dampWet * allPass + dampDry * sample);
  }

  x1b8_preDelayPtr[c] = preDelayPtr;
}

template <typename T>
void EffectReverbHiImp<T>::_doCrosstalk(T* audio, float wet, float dry, int chanCount, int sampleCount) {
  for (int i = 0; i < sampleCount; ++i) {
    T* base = &audio[chanCount * i];
    float allWet = 0;
    for (int c = 0; c < chanCount; ++c) {
      allWet += base[c] * wet;
      base[c] *= dry;
    }
    for (int c = 0; c < chanCount; ++c)
      base[c] = ClampFull<T>(base[c] + allWet);
  }
}

template <typename T>
void EffectReverbHiImp<T>::applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap) {
  if (m_dirty)
    _update();

  for (size_t f = 0; f < frameCount; f += 160) {
    size_t blockSamples = std::min(size_t(160), frameCount - f);
    for (unsigned i = 0; i < chanMap.m_channelCount; ++i) {
      if (i == 0 && x1a8_internalCrosstalk != 0.f) {
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
} // namespace amuse

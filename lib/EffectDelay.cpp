#include "amuse/EffectDelay.hpp"

#include <cmath>

#include "amuse/Common.hpp"
#include "amuse/IBackendVoice.hpp"

namespace amuse {

EffectDelayImp::EffectDelayImp(uint32_t initDelay, uint32_t initFeedback, uint32_t initOutput, double sampleRate) {
  initDelay = std::clamp(initDelay, 10u, 5000u);
  initFeedback = std::clamp(initFeedback, 0u, 100u);
  initOutput = std::clamp(initOutput, 0u, 100u);

  x3c_delay.fill(initDelay);
  x48_feedback.fill(initFeedback);
  x54_output.fill(initOutput);

  _setup(sampleRate);
}

EffectDelayImp::EffectDelayImp(const EffectDelayInfo& info, double sampleRate) {
  for (size_t i = 0; i < NumChannels; ++i) {
    x3c_delay[i] = std::clamp(info.delay[i], 10u, 5000u);
    x48_feedback[i] = std::clamp(info.feedback[i], 0u, 100u);
    x54_output[i] = std::clamp(info.output[i], 0u, 100u);
  }

  _setup(sampleRate);
}

void EffectDelayImp::_setup(double sampleRate) {
  m_sampsPerMs = std::ceil(sampleRate / 1000.0);
  m_blockSamples = m_sampsPerMs * 5;

  _update();
}

void EffectDelayImp::_update() {
  for (size_t i = 0; i < NumChannels; ++i) {
    x0_currentSize[i] = ((x3c_delay[i] - 5) * m_sampsPerMs + 159) / 160;
    xc_currentPos[i] = 0;
    x18_currentFeedback[i] = x48_feedback[i] * 128 / 100;
    x24_currentOutput[i] = x54_output[i] * 128 / 100;

    x30_chanLines[i] = std::make_unique<float[]>(m_blockSamples * x0_currentSize[i]);
  }

  m_dirty = false;
}

void EffectDelayImp::applyEffect(float* audio, size_t frameCount, const ChannelMap& chanMap) {
  if (m_dirty)
    _update();

  for (size_t f = 0; f < frameCount;) {
    for (unsigned c = 0; c < chanMap.m_channelCount; ++c) {
      auto* chanAud = audio + c;
      for (unsigned i = 0; i < m_blockSamples && f < frameCount; ++i, ++f) {
        auto& liveSamp = chanAud[chanMap.m_channelCount * i];
        auto& samp = x30_chanLines[c][xc_currentPos[c] * m_blockSamples + i];
        samp = samp * x18_currentFeedback[c] / 128 + liveSamp;
        liveSamp = samp * x24_currentOutput[c] / 128;
      }
      xc_currentPos[c] = (xc_currentPos[c] + 1) % x0_currentSize[c];
    }
    audio += chanMap.m_channelCount * m_blockSamples;
  }
}

} // namespace amuse

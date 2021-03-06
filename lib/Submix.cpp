#include "amuse/Submix.hpp"

namespace amuse {

Submix::Submix(Engine& engine) : m_root(engine) {}

EffectChorus& Submix::makeChorus(uint32_t baseDelay, uint32_t variation, uint32_t period) {
  return makeEffect<EffectChorus>(baseDelay, variation, period);
}

EffectChorus& Submix::makeChorus(const EffectChorusInfo& info) { return makeEffect<EffectChorus>(info); }

EffectDelay& Submix::makeDelay(uint32_t initDelay, uint32_t initFeedback, uint32_t initOutput) {
  return makeEffect<EffectDelay>(initDelay, initFeedback, initOutput);
}

EffectDelay& Submix::makeDelay(const EffectDelayInfo& info) { return makeEffect<EffectDelay>(info); }

EffectReverbStd& Submix::makeReverbStd(float coloration, float mix, float time, float damping, float preDelay) {
  return makeEffect<EffectReverbStd>(coloration, mix, time, damping, preDelay);
}

EffectReverbStd& Submix::makeReverbStd(const EffectReverbStdInfo& info) { return makeEffect<EffectReverbStd>(info); }

EffectReverbHi& Submix::makeReverbHi(float coloration, float mix, float time, float damping, float preDelay,
                                     float crosstalk) {
  return makeEffect<EffectReverbHi>(coloration, mix, time, damping, preDelay, crosstalk);
}

EffectReverbHi& Submix::makeReverbHi(const EffectReverbHiInfo& info) { return makeEffect<EffectReverbHi>(info); }

void Submix::applyEffect(int16_t* audio, size_t frameCount, const ChannelMap& chanMap) const {
  for (const std::unique_ptr<EffectBaseTypeless>& effect : m_effectStack)
    ((EffectBase<int16_t>&)*effect).applyEffect(audio, frameCount, chanMap);
}

void Submix::applyEffect(int32_t* audio, size_t frameCount, const ChannelMap& chanMap) const {
  for (const std::unique_ptr<EffectBaseTypeless>& effect : m_effectStack)
    ((EffectBase<int32_t>&)*effect).applyEffect(audio, frameCount, chanMap);
}

void Submix::applyEffect(float* audio, size_t frameCount, const ChannelMap& chanMap) const {
  for (const std::unique_ptr<EffectBaseTypeless>& effect : m_effectStack)
    ((EffectBase<float>&)*effect).applyEffect(audio, frameCount, chanMap);
}

void Submix::resetOutputSampleRate(double sampleRate) {
  for (const std::unique_ptr<EffectBaseTypeless>& effect : m_effectStack)
    effect->resetOutputSampleRate(sampleRate);
}
} // namespace amuse

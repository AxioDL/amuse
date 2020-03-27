#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <memory>

#include "amuse/Common.hpp"
#include "amuse/EffectBase.hpp"
#include "amuse/IBackendVoice.hpp"

namespace amuse {
template <typename T>
class EffectDelayImp;

/** Parameters needed to create EffectDelay */
struct EffectDelayInfo {
  std::array<uint32_t, NumChannels> delay; /**< [10, 5000] time in ms of each channel's delay */
  std::array<uint32_t, NumChannels> feedback{}; /**< [0, 100] percent to mix delayed signal with input signal */
  std::array<uint32_t, NumChannels> output{};   /**< [0, 100] total output percent */

  static uint32_t lerp(uint32_t v0, uint32_t v1, float t) { return (1.f - t) * v0 + t * v1; }

  static void Interp3To8(std::array<uint32_t, 8>& arr, uint32_t L, uint32_t R, uint32_t S) {
    arr[size_t(AudioChannel::FrontLeft)] = L;
    arr[size_t(AudioChannel::FrontRight)] = R;
    arr[size_t(AudioChannel::RearLeft)] = lerp(L, S, 0.75f);
    arr[size_t(AudioChannel::RearRight)] = lerp(R, S, 0.75f);
    arr[size_t(AudioChannel::FrontCenter)] = lerp(L, R, 0.5f);
    arr[size_t(AudioChannel::LFE)] = arr[size_t(AudioChannel::FrontCenter)];
    arr[size_t(AudioChannel::SideLeft)] = lerp(L, S, 0.5f);
    arr[size_t(AudioChannel::SideRight)] = lerp(R, S, 0.5f);
  }

  EffectDelayInfo() { delay.fill(10); }
  EffectDelayInfo(uint32_t delayL, uint32_t delayR, uint32_t delayS, uint32_t feedbackL, uint32_t feedbackR,
                  uint32_t feedbackS, uint32_t outputL, uint32_t outputR, uint32_t outputS) {
    Interp3To8(delay, delayL, delayR, delayS);
    Interp3To8(feedback, feedbackL, feedbackR, feedbackS);
    Interp3To8(output, outputL, outputR, outputS);
  }
};

/** Mixes the audio back into itself after specified delay */
class EffectDelay {
protected:
  std::array<uint32_t, NumChannels> x3c_delay;    /**< [10, 5000] time in ms of each channel's delay */
  std::array<uint32_t, NumChannels> x48_feedback; /**< [0, 100] percent to mix delayed signal with input signal */
  std::array<uint32_t, NumChannels> x54_output;   /**< [0, 100] total output percent */
  bool m_dirty = true;      /**< needs update of internal parameter data */

public:
  template <typename T>
  using ImpType = EffectDelayImp<T>;

  void setDelay(uint32_t delay) {
    delay = std::clamp(delay, 10u, 5000u);
    x3c_delay.fill(delay);
    m_dirty = true;
  }
  void setChanDelay(int chanIdx, uint32_t delay) {
    delay = std::clamp(delay, 10u, 5000u);
    x3c_delay[chanIdx] = delay;
    m_dirty = true;
  }
  uint32_t getChanDelay(int chanIdx) const { return x3c_delay[chanIdx]; }

  void setFeedback(uint32_t feedback) {
    feedback = std::clamp(feedback, 0u, 100u);
    x48_feedback.fill(feedback);
    m_dirty = true;
  }

  void setChanFeedback(int chanIdx, uint32_t feedback) {
    feedback = std::clamp(feedback, 0u, 100u);
    x48_feedback[chanIdx] = feedback;
    m_dirty = true;
  }
  uint32_t getChanFeedback(int chanIdx) const { return x48_feedback[chanIdx]; }

  void setOutput(uint32_t output) {
    output = std::clamp(output, 0u, 100u);
    x54_output.fill(output);
    m_dirty = true;
  }

  void setChanOutput(int chanIdx, uint32_t output) {
    output = std::clamp(output, 0u, 100u);
    x54_output[chanIdx] = output;
    m_dirty = true;
  }
  uint32_t getChanOutput(int chanIdx) const { return x54_output[chanIdx]; }

  void setParams(const EffectDelayInfo& info) {
    for (size_t i = 0; i < NumChannels; ++i) {
      x3c_delay[i] = std::clamp(info.delay[i], 10u, 5000u);
      x48_feedback[i] = std::clamp(info.feedback[i], 0u, 100u);
      x54_output[i] = std::clamp(info.output[i], 0u, 100u);
    }
    m_dirty = true;
  }
};

/** Type-specific implementation of delay effect */
template <typename T>
class EffectDelayImp : public EffectBase<T>, public EffectDelay {
  std::array<uint32_t, NumChannels> x0_currentSize;      /**< per-channel delay-line buffer sizes */
  std::array<uint32_t, NumChannels> xc_currentPos;       /**< per-channel block-index */
  std::array<uint32_t, NumChannels> x18_currentFeedback; /**< [0, 128] feedback attenuator */
  std::array<uint32_t, NumChannels> x24_currentOutput;   /**< [0, 128] total attenuator */

  std::array<std::unique_ptr<T[]>, NumChannels> x30_chanLines; /**< delay-line buffers for each channel */

  uint32_t m_sampsPerMs;   /**< canonical count of samples per ms for the current backend */
  uint32_t m_blockSamples; /**< count of samples in a 5ms block */
  void _setup(double sampleRate);
  void _update();

public:
  EffectDelayImp(uint32_t initDelay, uint32_t initFeedback, uint32_t initOutput, double sampleRate);
  EffectDelayImp(const EffectDelayInfo& info, double sampleRate);

  void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap) override;
  void resetOutputSampleRate(double sampleRate) override { _setup(sampleRate); }

  EffectType Isa() const override { return EffectType::Delay; }
};
} // namespace amuse

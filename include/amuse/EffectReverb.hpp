#pragma once

#include <array>
#include <cstdint>
#include <memory>

#include "amuse/Common.hpp"
#include "amuse/EffectBase.hpp"
#include "amuse/IBackendVoice.hpp"

namespace amuse {

/** Parameters needed to create EffectReverbStd */
struct EffectReverbStdInfo {
  float coloration = 0.f; /**< [0.0, 1.0] influences filter coefficients to define surface characteristics of a room */
  float mix = 0.f;        /**< [0.0, 1.0] dry/wet mix factor of reverb effect */
  float time = 0.01f;     /**< [0.01, 10.0] time in seconds for reflection decay */
  float damping = 0.f;    /**< [0.0, 1.0] damping factor influencing low-pass filter of reflections */
  float preDelay = 0.f;   /**< [0.0, 0.1] time in seconds before initial reflection heard */

  EffectReverbStdInfo() = default;
  EffectReverbStdInfo(float coloration, float mix, float time, float damping, float preDelay)
  : coloration(coloration), mix(mix), time(time), damping(damping), preDelay(preDelay) {}
};

/** Parameters needed to create EffectReverbHi */
struct EffectReverbHiInfo {
  float coloration = 0.f; /**< [0.0, 1.0] influences filter coefficients to define surface characteristics of a room */
  float mix = 0.f;        /**< [0.0, 1.0] dry/wet mix factor of reverb effect */
  float time = 0.01f;     /**< [0.01, 10.0] time in seconds for reflection decay */
  float damping = 0.f;    /**< [0.0, 1.0] damping factor influencing low-pass filter of reflections */
  float preDelay = 0.f;   /**< [0.0, 0.1] time in seconds before initial reflection heard */
  float crosstalk = 0.f;  /**< [0.0, 1.0] factor defining how much reflections are allowed to bleed to other channels */

  EffectReverbHiInfo() = default;
  EffectReverbHiInfo(float coloration, float mix, float time, float damping, float preDelay, float crosstalk)
  : coloration(coloration), mix(mix), time(time), damping(damping), preDelay(preDelay), crosstalk(crosstalk) {}
};

/** Delay state for one 'tap' of the reverb effect */
struct ReverbDelayLine {
  int32_t x0_inPoint = 0;
  int32_t x4_outPoint = 0;
  int32_t x8_length = 0;
  std::unique_ptr<float[]> xc_inputs;
  float x10_lastInput = 0.f;

  void allocate(int32_t delay);
  void setdelay(int32_t delay);
};

template <typename T>
class EffectReverbStdImp;

template <typename T>
class EffectReverbHiImp;

/** Reverb effect with configurable reflection filtering */
class EffectReverbStd {
protected:
  float x140_x1c8_coloration; /**< [0.0, 1.0] influences filter coefficients to define surface characteristics of a
                                 room */
  float x144_x1cc_mix;        /**< [0.0, 1.0] dry/wet mix factor of reverb effect */
  float x148_x1d0_time;       /**< [0.01, 10.0] time in seconds for reflection decay */
  float x14c_x1d4_damping;    /**< [0.0, 1.0] damping factor influencing low-pass filter of reflections */
  float x150_x1d8_preDelay;   /**< [0.0, 0.1] time in seconds before initial reflection heard */
  bool m_dirty = true;        /**< needs update of internal parameter data */

  template <typename T>
  friend class EffectReverbStdImp;
  template <typename T>
  friend class EffectReverbHiImp;
  EffectReverbStd(float coloration, float mix, float time, float damping, float preDelay);

public:
  template <typename T>
  using ImpType = EffectReverbStdImp<T>;

  void setColoration(float coloration) {
    x140_x1c8_coloration = std::clamp(coloration, 0.f, 1.f);
    m_dirty = true;
  }
  float getColoration() const { return x140_x1c8_coloration; }

  void setMix(float mix) {
    x144_x1cc_mix = std::clamp(mix, 0.f, 1.f);
    m_dirty = true;
  }
  float getMix() const { return x144_x1cc_mix; }

  void setTime(float time) {
    x148_x1d0_time = std::clamp(time, 0.01f, 10.f);
    m_dirty = true;
  }
  float getTime() const { return x148_x1d0_time; }

  void setDamping(float damping) {
    x14c_x1d4_damping = std::clamp(damping, 0.f, 1.f);
    m_dirty = true;
  }
  float getDamping() const { return x14c_x1d4_damping; }

  void setPreDelay(float preDelay) {
    x150_x1d8_preDelay = std::clamp(preDelay, 0.f, 0.1f);
    m_dirty = true;
  }
  float getPreDelay() const { return x150_x1d8_preDelay; }

  void setParams(const EffectReverbStdInfo& info) {
    setColoration(info.coloration);
    setMix(info.mix);
    setTime(info.time);
    setDamping(info.damping);
    setPreDelay(info.preDelay);
  }
};

/** Reverb effect with configurable reflection filtering, adds per-channel low-pass and crosstalk */
class EffectReverbHi : public EffectReverbStd {
  float x1dc_crosstalk; /**< [0.0, 1.0] factor defining how much reflections are allowed to bleed to other channels */

  template <typename T>
  friend class EffectReverbHiImp;
  EffectReverbHi(float coloration, float mix, float time, float damping, float preDelay, float crosstalk);

public:
  template <typename T>
  using ImpType = EffectReverbHiImp<T>;

  void setCrosstalk(float crosstalk) {
    x1dc_crosstalk = std::clamp(crosstalk, 0.f, 1.f);
    m_dirty = true;
  }
  float getCrosstalk() const { return x1dc_crosstalk; }

  void setParams(const EffectReverbHiInfo& info) {
    setColoration(info.coloration);
    setMix(info.mix);
    setTime(info.time);
    setDamping(info.damping);
    setPreDelay(info.preDelay);
    setCrosstalk(info.crosstalk);
  }
};

/** Standard-quality 2-stage reverb */
template <typename T>
class EffectReverbStdImp : public EffectBase<T>, public EffectReverbStd {
  using CombCoeffArray = std::array<std::array<float, 2>, NumChannels>;
  using ReverbDelayArray = std::array<std::array<ReverbDelayLine, 2>, NumChannels>;
  using PreDelayArray = std::array<std::unique_ptr<float[]>, NumChannels>;

  ReverbDelayArray x0_AP{};                           /**< All-pass delay lines */
  ReverbDelayArray x78_C{};                           /**< Comb delay lines */
  float xf0_allPassCoef = 0.f;                        /**< All-pass mix coefficient */
  CombCoeffArray xf4_combCoef{};                      /**< Comb mix coefficients */
  std::array<float, NumChannels> x10c_lpLastout{};    /**< Last low-pass results */
  float x118_level = 0.f;                             /**< Internal wet/dry mix factor */
  float x11c_damping = 0.f;                           /**< Low-pass damping */
  int32_t x120_preDelayTime = 0;                      /**< Sample count of pre-delay */
  PreDelayArray x124_preDelayLine;                    /**< Dedicated pre-delay buffers */
  std::array<float*, NumChannels> x130_preDelayPtr{}; /**< Current pre-delay pointers */

  double m_sampleRate; /**< copy of sample rate */
  void _setup(double sampleRate);
  void _update();

public:
  EffectReverbStdImp(float coloration, float mix, float time, float damping, float preDelay, double sampleRate);
  EffectReverbStdImp(const EffectReverbStdInfo& info, double sampleRate)
  : EffectReverbStdImp(info.coloration, info.mix, info.time, info.damping, info.preDelay, sampleRate) {}

  void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap) override;
  void resetOutputSampleRate(double sampleRate) override { _setup(sampleRate); }

  EffectType Isa() const override { return EffectType::ReverbStd; }
};

/** High-quality 3-stage reverb with per-channel low-pass and crosstalk */
template <typename T>
class EffectReverbHiImp : public EffectBase<T>, public EffectReverbHi {
  using AllPassDelayLines = std::array<std::array<ReverbDelayLine, 2>, NumChannels>;
  using CombCoefficients = std::array<std::array<float, 3>, NumChannels>;
  using CombDelayLines = std::array<std::array<ReverbDelayLine, 3>, NumChannels>;
  using LowPassDelayLines = std::array<ReverbDelayLine, 8>;
  using PreDelayLines = std::array<std::unique_ptr<float[]>, 8>;

  AllPassDelayLines x0_AP{};                          /**< All-pass delay lines */
  LowPassDelayLines x78_LP{};                         /**< Per-channel low-pass delay-lines */
  CombDelayLines xb4_C{};                             /**< Comb delay lines */
  float x168_allPassCoef = 0.f;                       /**< All-pass mix coefficient */
  CombCoefficients x16c_combCoef{};                   /**< Comb mix coefficients */
  std::array<float, 8> x190_lpLastout{};              /**< Last low-pass results */
  float x19c_level = 0.f;                             /**< Internal wet/dry mix factor */
  float x1a0_damping = 0.f;                           /**< Low-pass damping */
  int32_t x1a4_preDelayTime = 0;                      /**< Sample count of pre-delay */
  PreDelayLines x1ac_preDelayLine;                    /**< Dedicated pre-delay buffers */
  std::array<float*, NumChannels> x1b8_preDelayPtr{}; /**< Current pre-delay pointers */
  float x1a8_internalCrosstalk = 0.f;

  double m_sampleRate; /**< copy of sample rate */
  void _setup(double sampleRate);
  void _update();
  void _handleReverb(T* audio, int chanIdx, int chanCount, int sampleCount);
  void _doCrosstalk(T* audio, float wet, float dry, int chanCount, int sampleCount);

public:
  EffectReverbHiImp(float coloration, float mix, float time, float damping, float preDelay, float crosstalk,
                    double sampleRate);
  EffectReverbHiImp(const EffectReverbHiInfo& info, double sampleRate)
  : EffectReverbHiImp(info.coloration, info.mix, info.time, info.damping, info.preDelay, info.crosstalk, sampleRate) {}

  void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap) override;
  void resetOutputSampleRate(double sampleRate) override { _setup(sampleRate); }

  EffectType Isa() const override { return EffectType::ReverbHi; }
};
} // namespace amuse

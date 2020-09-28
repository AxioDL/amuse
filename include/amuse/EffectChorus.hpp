#pragma once

#include <array>
#include <cstdint>

#include "amuse/Common.hpp"
#include "amuse/EffectBase.hpp"
#include "amuse/IBackendVoice.hpp"

namespace amuse {
class EffectChorusImp;

#define AMUSE_CHORUS_NUM_BLOCKS 3

/** Parameters needed to create EffectChorus */
struct EffectChorusInfo {
  uint32_t baseDelay = 5; /**< [5, 15] minimum value (in ms) for computed delay */
  uint32_t variation = 0; /**< [0, 5] time error (in ms) to set delay within */
  uint32_t period = 500;  /**< [500, 10000] time (in ms) of one delay-shift cycle */

  EffectChorusInfo() = default;
  EffectChorusInfo(uint32_t baseDelay, uint32_t variation, uint32_t period)
  : baseDelay(baseDelay), variation(variation), period(period) {}
};

/** Mixes the audio back into itself after continuously-varying delay */
class EffectChorus {
  uint32_t x90_baseDelay; /**< [5, 15] minimum value (in ms) for computed delay */
  uint32_t x94_variation; /**< [0, 5] time error (in ms) to set delay within */
  uint32_t x98_period;    /**< [500, 10000] time (in ms) of one delay-shift cycle */
  bool m_dirty = true;    /**< needs update of internal parameter data */

  friend class EffectChorusImp;
  EffectChorus(uint32_t baseDelay, uint32_t variation, uint32_t period);

public:
  using ImpType = EffectChorusImp;

  void setBaseDelay(uint32_t baseDelay) {
    baseDelay = std::clamp(baseDelay, 5u, 15u);
    x90_baseDelay = baseDelay;
    m_dirty = true;
  }
  uint32_t getBaseDelay() const { return x90_baseDelay; }

  void setVariation(uint32_t variation) {
    variation = std::clamp(variation, 0u, 5u);
    x94_variation = variation;
    m_dirty = true;
  }
  uint32_t getVariation() const { return x94_variation; }

  void setPeriod(uint32_t period) {
    period = std::clamp(period, 500u, 10000u);
    x98_period = period;
    m_dirty = true;
  }
  uint32_t getPeriod() const { return x98_period; }

  void updateParams(const EffectChorusInfo& info) {
    setBaseDelay(info.baseDelay);
    setVariation(info.variation);
    setPeriod(info.period);
  }
};

/** Type-specific implementation of chorus effect */
class EffectChorusImp : public EffectBase, public EffectChorus {
  /** Evenly-allocated pointer-table for each channel's delay */
  std::array<std::array<float*, AMUSE_CHORUS_NUM_BLOCKS>, NumChannels> x0_lastChans{};

  uint8_t x24_currentLast = 1; /**< Last 5ms block-idx to be processed */
  std::array<std::array<float, 4>, NumChannels> x28_oldChans{}; /**< Unprocessed history of previous 4 samples */

  uint32_t x58_currentPosLo = 0; /**< 16.7 fixed-point low-part of sample index */
  uint32_t x5c_currentPosHi = 0; /**< 16.7 fixed-point high-part of sample index */

  int32_t x60_pitchOffset;             /**< packed 16.16 fixed-point value of pitchHi and pitchLo quantities */
  uint32_t x64_pitchOffsetPeriodCount; /**< trigger value for flipping SRC state */
  uint32_t x68_pitchOffsetPeriod;      /**< intermediate block window quantity for calculating SRC state */

  struct SrcInfo {
    float* x6c_dest;             /**< selected channel's live buffer */
    float* x70_smpBase;          /**< selected channel's delay buffer */
    float* x74_old;              /**< selected channel's 4-sample history buffer */
    uint32_t x78_posLo;      /**< 16.7 fixed-point low-part of sample index */
    uint32_t x7c_posHi;      /**< 16.7 fixed-point high-part of sample index */
    uint32_t x80_pitchLo;    /**< 16.7 fixed-point low-part of sample-rate conversion differential */
    uint32_t x84_pitchHi;    /**< 16.7 fixed-point low-part of sample-rate conversion differential */
    uint32_t x88_trigger;    /**< total count of samples per channel across all blocks */
    uint32_t x8c_target = 0; /**< value to reset to when trigger hit */

    void doSrc1(size_t blockSamples, size_t chanCount);
    void doSrc2(size_t blockSamples, size_t chanCount);
  };
  SrcInfo x6c_src;

  uint32_t m_sampsPerMs;   /**< canonical count of samples per ms for the current backend */
  uint32_t m_blockSamples; /**< count of samples in a 5ms block */

  void _setup(double sampleRate);
  void _update();

public:
  ~EffectChorusImp() override;
  EffectChorusImp(uint32_t baseDelay, uint32_t variation, uint32_t period, double sampleRate);
  EffectChorusImp(const EffectChorusInfo& info, double sampleRate)
  : EffectChorusImp(info.baseDelay, info.variation, info.period, sampleRate) {}

  void applyEffect(float* audio, size_t frameCount, const ChannelMap& chanMap) override;
  void resetOutputSampleRate(double sampleRate) override { _setup(sampleRate); }

  EffectType Isa() const override { return EffectType::Chorus; }
};
} // namespace amuse

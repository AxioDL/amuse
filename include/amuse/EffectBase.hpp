#pragma once

namespace amuse {
struct ChannelMap;

enum class EffectType { Invalid, ReverbStd, ReverbHi, Delay, Chorus, EffectTypeMAX };

class EffectBase {
public:
  virtual ~EffectBase() = default;
  virtual void resetOutputSampleRate(double sampleRate) = 0;
  virtual EffectType Isa() const = 0;
  virtual void applyEffect(float* audio, size_t frameCount, const ChannelMap& chanMap) = 0;
};
} // namespace amuse

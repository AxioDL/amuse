#pragma once

namespace amuse {
struct ChannelMap;

enum class EffectType { Invalid, ReverbStd, ReverbHi, Delay, Chorus, EffectTypeMAX };

class EffectBaseTypeless {
public:
  virtual ~EffectBaseTypeless() = default;
  virtual void resetOutputSampleRate(double sampleRate) = 0;
  virtual EffectType Isa() const = 0;
};

template <typename T>
class EffectBase : public EffectBaseTypeless {
public:
  virtual void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap) = 0;
};
} // namespace amuse

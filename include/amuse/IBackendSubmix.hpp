#pragma once

namespace amuse {

/** Client-implemented submix instance */
class IBackendSubmix {
public:
  virtual ~IBackendSubmix() = default;

  /** Set send level for submix (AudioChannel enum for array index) */
  virtual void setSendLevel(IBackendSubmix* submix, float level, bool slew) = 0;

  /** Amuse gets fixed sample rate of submix this way */
  virtual double getSampleRate() const = 0;
};
} // namespace amuse

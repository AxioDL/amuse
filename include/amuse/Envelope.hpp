#pragma once

#include "AudioGroupPool.hpp"

namespace amuse {
class Voice;

/** Per-sample state tracker for ADSR envelope data */
class Envelope {
public:
  enum class State { Attack, Decay, Sustain, Release, Complete };

private:
  State m_phase = State::Attack;     /**< Current envelope state */
  double m_attackTime = 0.01;        /**< Time of attack in seconds */
  double m_decayTime = 0.0;          /**< Time of decay in seconds */
  double m_sustainFactor = 1.0;      /**< Evaluated sustain percentage */
  double m_releaseTime = 0.01;       /**< Time of release in seconds */
  double m_releaseStartFactor = 0.0; /**< Level at whenever release event occurs */
  double m_curTime = 0.0;            /**< Current time of envelope stage in seconds */
  bool m_adsrSet = false;

public:
  void reset(const ADSR* adsr);
  void reset(const ADSRDLS* adsr, int8_t note, int8_t vel);
  void keyOff(const Voice& vox);
  void keyOff();
  float advance(double dt, const Voice& vox);
  float advance(double dt);
  bool isComplete(const Voice& vox) const;
  bool isAdsrSet() const { return m_adsrSet; }
};
} // namespace amuse

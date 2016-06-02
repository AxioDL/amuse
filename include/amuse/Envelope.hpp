#ifndef __AMUSE_ENVELOPE_HPP__
#define __AMUSE_ENVELOPE_HPP__

#include "AudioGroupPool.hpp"

namespace amuse
{

/** Per-sample state tracker for ADSR envelope data */
class Envelope
{
public:
    enum class State
    {
        Attack,
        Decay,
        Sustain,
        Release,
        Complete
    };
private:
    State m_phase = State::Attack; /**< Current envelope state */
    double m_attackTime = 0.02; /**< Time of attack in seconds */
    double m_decayTime = 0.0; /**< Time of decay in seconds */
    double m_sustainFactor = 1.0; /**< Evaluated sustain percentage */
    double m_releaseTime = 0.02; /**< Time of release in seconds */
    double m_releaseStartFactor = 0.0; /**< Level at whenever release event occurs */
    double m_curTime = 0.0; /**< Current time of envelope stage in seconds */
    bool m_adsrSet = false;
public:
    void reset(const ADSR* adsr);
    void reset(const ADSRDLS* adsr, int8_t note, int8_t vel);
    void keyOff();
    float advance(double dt);
    bool isComplete() const {return m_phase == State::Complete;}
    bool isAdsrSet() const {return m_adsrSet;}
};

}

#endif // __AMUSE_ENVELOPE_HPP__

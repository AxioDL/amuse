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
        Release
    };
private:
    State m_phase = State::Attack; /**< Current envelope state */
    const ADSR* m_curADSR = nullptr; /**< Current timing envelope */
    double m_sustainFactor; /**< Evaluated sustain percentage as a double */
    double m_releaseStartFactor; /**< Level at whenever release event occurs */
    unsigned m_curMs; /**< Current time of envelope stage */
public:
    void reset(const ADSR* adsr);
    void keyOff();
    float nextSample(double sampleRate);
};

}

#endif // __AMUSE_ENVELOPE_HPP__

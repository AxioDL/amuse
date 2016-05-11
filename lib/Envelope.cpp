#include "amuse/Envelope.hpp"

namespace amuse
{

void Envelope::reset(const ADSR* adsr)
{
    m_phase = State::Attack;
    m_curADSR = adsr;
    m_curMs = 0;
    m_sustainFactor = (adsr->sustainCoarse * 6.25 + adsr->sustainFine * 0.0244) / 100.0;
    m_releaseStartFactor = 0.0;
}

void Envelope::keyOff()
{
    m_phase = State::Release;
    m_curMs = 0;
}

float Envelope::nextSample(double sampleRate)
{
    if (!m_curADSR)
    {
        if (m_phase == State::Release)
            return 0.f;
        return 1.f;
    }

    m_curMs += 1.0 / (sampleRate / 1000.0);

    switch (m_phase)
    {
    case State::Attack:
    {
        double attackFac = m_curMs / (m_curADSR->attackCoarse * 255 + m_curADSR->attackFine);
        if (attackFac >= 1.0)
        {
            m_phase = State::Decay;
            m_curMs = 0;
            return 1.f;
        }
        m_releaseStartFactor = attackFac;
        return attackFac;
    }
    case State::Decay:
    {
        double decayFac = m_curMs / (m_curADSR->decayCoarse * 255 + m_curADSR->decayFine);
        if (decayFac >= 1.0)
        {
            m_phase = State::Sustain;
            m_curMs = 0;
            m_releaseStartFactor = m_sustainFactor;
            return m_sustainFactor;
        }
        m_releaseStartFactor = (1.0 - decayFac) + decayFac * m_sustainFactor;
        return m_releaseStartFactor;
    }
    case State::Sustain:
    {
        return m_sustainFactor;
    }
    case State::Release:
    {
        double releaseFac = m_curMs / (m_curADSR->releaseCoarse * 255 + m_curADSR->releaseFine);
        if (releaseFac >= 1.0)
            return 0.f;
        return (1.0 - releaseFac) * m_releaseStartFactor;
    }
    }
}

}

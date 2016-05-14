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
        uint16_t attack = m_curADSR->attackCoarse * 255 + m_curADSR->attackFine;
        if (attack == 0)
        {
            m_phase = State::Decay;
            m_curMs = 0;
            return 1.f;
        }
        double attackFac = m_curMs / double(attack);
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
        uint16_t decay = m_curADSR->decayCoarse * 255 + m_curADSR->decayFine;
        if (decay == 0)
        {
            m_phase = State::Sustain;
            m_curMs = 0;
            m_releaseStartFactor = m_sustainFactor;
            return m_sustainFactor;
        }
        double decayFac = m_curMs / double(decay);
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
        uint16_t release = m_curADSR->releaseCoarse * 255 + m_curADSR->releaseFine;
        if (release == 0)
            return 0.f;
        double releaseFac = m_curMs / double(release);
        if (releaseFac >= 1.0)
            return 0.f;
        return (1.0 - releaseFac) * m_releaseStartFactor;
    }
    }
}

}

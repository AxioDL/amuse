#include "amuse/Envelope.hpp"

namespace amuse
{

void Envelope::reset(const ADSR* adsr)
{
    m_phase = State::Attack;
    m_curTime = 0.0;
    m_attackTime = adsr->getAttack();
    m_decayTime = adsr->getDecay();
    m_sustainFactor = adsr->getSustain();
    m_releaseTime = adsr->getRelease();
    m_releaseStartFactor = 0.0;
    m_adsrSet = true;
}

void Envelope::reset(const ADSRDLS* adsr, int8_t note, int8_t vel)
{
    m_phase = State::Attack;
    m_curTime = 0.0;
    m_attackTime = adsr->getVelToAttack(vel);
    m_decayTime = adsr->getKeyToDecay(note);
    m_sustainFactor = adsr->getSustain();
    m_releaseTime = adsr->getRelease();
    m_releaseStartFactor = 0.0;
    m_adsrSet = true;
}

void Envelope::keyOff()
{
    m_phase = (m_releaseTime != 0.0) ? State::Release : State::Complete;
    m_curTime = 0.0;
}

float Envelope::advance(double dt)
{
    double thisTime = m_curTime;
    m_curTime += dt;

    switch (m_phase)
    {
    case State::Attack:
    {
        if (m_attackTime == 0.0)
        {
            m_phase = State::Decay;
            m_curTime = 0.0;
            m_releaseStartFactor = 1.f;
            return 1.f;
        }
        double attackFac = thisTime / m_attackTime;
        if (attackFac >= 1.0)
        {
            m_phase = State::Decay;
            m_curTime = 0.0;
            m_releaseStartFactor = 1.f;
            return 1.f;
        }
        m_releaseStartFactor = attackFac;
        return attackFac;
    }
    case State::Decay:
    {
        if (m_decayTime == 0.0)
        {
            m_phase = State::Sustain;
            m_curTime = 0.0;
            m_releaseStartFactor = m_sustainFactor;
            return m_sustainFactor;
        }
        double decayFac = thisTime / m_decayTime;
        if (decayFac >= 1.0)
        {
            m_phase = State::Sustain;
            m_curTime = 0.0;
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
        if (m_releaseTime == 0.0)
        {
            m_phase = State::Complete;
            return 0.f;
        }
        double releaseFac = thisTime / m_releaseTime;
        if (releaseFac >= 1.0)
        {
            m_phase = State::Complete;
            return 0.f;
        }
        return std::min(m_releaseStartFactor, 1.0 - releaseFac);
    }
    case State::Complete:
    default:
        return 0.f;
    }
}

}

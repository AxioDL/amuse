#include "amuse/Envelope.hpp"
#include "amuse/Voice.hpp"

namespace amuse
{

static int32_t MIDItoTIME[104] = {/* [0..103] -> milliseconds */
                                  0,     10,    20,    30,    40,    50,    60,    70,    80,    90,    100,   110,
                                  110,   120,   130,   140,   150,   160,   170,   190,   200,   220,   230,   250,
                                  270,   290,   310,   330,   350,   380,   410,   440,   470,   500,   540,   580,
                                  620,   660,   710,   760,   820,   880,   940,   1000,  1000,  1100,  1200,  1300,
                                  1400,  1500,  1600,  1700,  1800,  2000,  2100,  2300,  2400,  2600,  2800,  3000,
                                  3200,  3500,  3700,  4000,  4300,  4600,  4900,  5300,  5700,  6100,  6500,  7000,
                                  7500,  8100,  8600,  9300,  9900,  10000, 11000, 12000, 13000, 14000, 15000, 16000,
                                  17000, 18000, 19000, 21000, 22000, 24000, 26000, 28000, 30000, 32000, 34000, 37000,
                                  39000, 42000, 45000, 49000, 50000, 55000, 60000, 65000};

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

void Envelope::keyOff(const Voice& vox)
{
    double releaseTime = m_releaseTime;
    if (vox.m_state.m_useAdsrControllers)
        releaseTime = MIDItoTIME[clamp(0, int(vox.getCtrlValue(vox.m_state.m_midiRelease)), 103)] / 1000.0;

    m_phase = (releaseTime != 0.0) ? State::Release : State::Complete;
    m_curTime = 0.0;
}

void Envelope::keyOff()
{
    m_phase = (m_releaseTime != 0.0) ? State::Release : State::Complete;
    m_curTime = 0.0;
}

float Envelope::advance(double dt, const Voice& vox)
{
    double thisTime = m_curTime;
    m_curTime += dt;

    switch (m_phase)
    {
    case State::Attack:
    {
        double attackTime = m_attackTime;
        if (vox.m_state.m_useAdsrControllers)
            attackTime = MIDItoTIME[clamp(0, int(vox.getCtrlValue(vox.m_state.m_midiAttack)), 103)] / 1000.0;

        if (attackTime == 0.0)
        {
            m_phase = State::Decay;
            m_curTime = 0.0;
            m_releaseStartFactor = 1.f;
            return 1.f;
        }
        double attackFac = thisTime / attackTime;
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
        double decayTime = m_decayTime;
        if (vox.m_state.m_useAdsrControllers)
            decayTime = MIDItoTIME[clamp(0, int(vox.getCtrlValue(vox.m_state.m_midiDecay)), 103)] / 1000.0;

        double sustainFactor = m_sustainFactor;
        if (vox.m_state.m_useAdsrControllers)
            sustainFactor = clamp(0, int(vox.getCtrlValue(vox.m_state.m_midiSustain)), 127) / 127.0;

        if (decayTime == 0.0)
        {
            m_phase = State::Sustain;
            m_curTime = 0.0;
            m_releaseStartFactor = sustainFactor;
            return sustainFactor;
        }
        double decayFac = thisTime / decayTime;
        if (decayFac >= 1.0)
        {
            m_phase = State::Sustain;
            m_curTime = 0.0;
            m_releaseStartFactor = sustainFactor;
            return sustainFactor;
        }
        m_releaseStartFactor = (1.0 - decayFac) + decayFac * sustainFactor;
        return m_releaseStartFactor;
    }
    case State::Sustain:
    {
        double sustainFactor = m_sustainFactor;
        if (vox.m_state.m_useAdsrControllers)
            sustainFactor = clamp(0, int(vox.getCtrlValue(vox.m_state.m_midiSustain)), 127) / 127.0;

        return sustainFactor;
    }
    case State::Release:
    {
        double releaseTime = m_releaseTime;
        if (vox.m_state.m_useAdsrControllers)
            releaseTime = MIDItoTIME[clamp(0, int(vox.getCtrlValue(vox.m_state.m_midiRelease)), 103)] / 1000.0;

        if (releaseTime == 0.0)
        {
            m_phase = State::Complete;
            return 0.f;
        }
        double releaseFac = thisTime / releaseTime;
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

float Envelope::advance(double dt)
{
    double thisTime = m_curTime;
    m_curTime += dt;

    switch (m_phase)
    {
    case State::Attack:
    {
        double attackTime = m_attackTime;

        if (attackTime == 0.0)
        {
            m_phase = State::Decay;
            m_curTime = 0.0;
            m_releaseStartFactor = 1.f;
            return 1.f;
        }
        double attackFac = thisTime / attackTime;
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
        double decayTime = m_decayTime;
        double sustainFactor = m_sustainFactor;

        if (decayTime == 0.0)
        {
            m_phase = State::Sustain;
            m_curTime = 0.0;
            m_releaseStartFactor = sustainFactor;
            return sustainFactor;
        }
        double decayFac = thisTime / decayTime;
        if (decayFac >= 1.0)
        {
            m_phase = State::Sustain;
            m_curTime = 0.0;
            m_releaseStartFactor = sustainFactor;
            return sustainFactor;
        }
        m_releaseStartFactor = (1.0 - decayFac) + decayFac * sustainFactor;
        return m_releaseStartFactor;
    }
    case State::Sustain:
    {
        return m_sustainFactor;
    }
    case State::Release:
    {
        double releaseTime = m_releaseTime;

        if (releaseTime == 0.0)
        {
            m_phase = State::Complete;
            return 0.f;
        }
        double releaseFac = thisTime / releaseTime;
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

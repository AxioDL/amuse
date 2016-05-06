#include "amuse/SoundMacroState.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Engine.hpp"
#include "amuse/Common.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupPool.hpp"
#include <string.h>

#ifndef M_PIF
#define M_PIF 3.14159265358979323846f /* pi */
#endif

namespace amuse
{

void SoundMacroState::Header::swapBig()
{
    m_size = SBig(m_size);
}

void SoundMacroState::Command::swapBig()
{
    uint32_t* words = reinterpret_cast<uint32_t*>(this);
    words[0] = SBig(words[0]);
    words[1] = SBig(words[1]);
}

void SoundMacroState::LFOSel::addComponent(uint8_t midiCtrl, float scale,
                                           Combine combine, VarType varType)
{
    m_comps.push_back({midiCtrl, scale, combine, varType});
}

float SoundMacroState::LFOSel::evaluate(Voice& vox, const SoundMacroState& st)
{
    float value = 0.f;

    /* Iterate each component */
    for (auto it=m_comps.cbegin() ; it != m_comps.cend() ; ++it)
    {
        const Component& comp = *it;
        float thisValue = 0.f;

        /* Load selected data */
        if (comp.m_varType == VarType::Ctrl)
        {
            switch (comp.m_midiCtrl)
            {
            case 128:
                /* Pitchbend */
                thisValue = vox.getPitchWheel();
                break;
            case 129:
                /* Aftertouch */
                thisValue = vox.getAftertouch();
                break;
            case 130:
                /* LFO1 */
                if (st.m_lfoPeriods[0])
                    thisValue = std::sin(st.m_execTime / st.m_lfoPeriods[0] * 2.f * M_PIF);
                break;
            case 131:
                /* LFO2 */
                if (st.m_lfoPeriods[1])
                    thisValue = std::sin(st.m_execTime / st.m_lfoPeriods[1] * 2.f * M_PIF);
                break;
            case 132:
                /* Surround panning */
                thisValue = st.m_curSpan * 64.f + 64.f;
                break;
            case 133:
                /* Macro-starting key */
                thisValue = st.m_initKey;
                break;
            case 134:
                /* Macro-starting velocity */
                thisValue = st.m_initVel;
                break;
            case 135:
                /* Time since macro-start (ms) */
                thisValue = st.m_execTime * 1000.f;
                break;
            default:
                thisValue = vox.getCtrlValue(comp.m_midiCtrl);
                break;
            }
        }
        else if (comp.m_varType == VarType::Var)
            thisValue = st.m_variables[std::max(0, std::min(255, int(comp.m_midiCtrl)))];

        /* Apply scale */
        thisValue *= comp.m_scale;

        /* Combine */
        if (it != m_comps.cbegin())
        {
            switch (comp.m_combine)
            {
            case Combine::Add:
                value += thisValue;
                break;
            case Combine::Mult:
                value *= thisValue;
                break;
            default: break;
            }
        }
        else
            value = thisValue;
    }

    return value;
}

void SoundMacroState::initialize(const unsigned char* ptr)
{
    initialize(ptr, 1000.f, 0, 0, 0);
}

void SoundMacroState::initialize(const unsigned char* ptr, float ticksPerSec,
                                 uint8_t midiKey, uint8_t midiVel, uint8_t midiMod)
{
    m_ptr = ptr;
    m_curVol = 1.f;
    m_volDirty = true;
    m_curPan = 0.f;
    m_panDirty = true;
    m_curSpan = 0.f;
    m_spanDirty = true;
    m_ticksPerSec = ticksPerSec;
    m_initKey = 0;
    m_initVel = 0;
    m_initMod = 0;
    m_curVel = 0;
    m_curMod = 0;
    m_curKey = 0;
    m_pitchSweep1 = 0;
    m_pitchSweep1Times = 0;
    m_pitchSweep2 = 0;
    m_pitchSweep2Times = 0;
    m_pitchDirty = true;
    m_random.seed();
    m_pc.clear();
    m_pc.push_back(-1);
    m_execTime = 0.f;
    m_keyoff = false;
    m_sampleEnd = false;
    m_envelopeTime = -1.f;
    m_panningTime = -1.f;
    m_loopCountdown = -1;
    m_lastPlayMacroVid = -1;
    m_useAdsrControllers = false;
    m_portamentoMode = 0;
    m_vibratoLevel = 0;
    m_vibratoModLevel = 0;
    m_vibratoPeriod = 0.f;
    m_tremoloScale = 0.f;
    m_tremoloModScale = 0.f;
    m_lfoPeriods[0] = 0.f;
    m_lfoPeriods[1] = 0.f;
    m_header = *reinterpret_cast<const Header*>(ptr);
    m_header.swapBig();
}

bool SoundMacroState::advance(Voice& vox, float dt)
{
    /* Nothing if uninitialized or finished */
    if (m_pc.back() == -1)
        return true;

    /* Process active envelope */
    if (m_envelopeTime >= 0.f)
    {
        m_envelopeTime += dt;
        float start = m_envelopeStart / 127.f;
        float end = m_envelopeEnd / 127.f;
        float t = std::max(0.f, std::min(1.f, m_envelopeTime / m_envelopeDur));
        if (m_envelopeCurve)
            t = (*m_envelopeCurve)[int(t*127.f)] / 127.f;
        m_curVol = (start * (1.0f - t)) + (end * t);
        m_volDirty = true;

        /* Done with envelope */
        if (m_envelopeTime > m_envelopeDur)
            m_envelopeTime = -1.f;
    }

    /* Apply tremolo */
    float totalVol = m_curVol;
    if (m_tremoloSel && (m_tremoloScale || m_tremoloModScale))
    {
        float t = m_tremoloSel.evaluate(vox, *this);
        if (m_tremoloScale && m_tremoloModScale)
        {
            float fac = (1.0f - t) + (m_tremoloScale * t);
            float modT = vox.getModWheel() / 127.f;
            float modFac = (1.0f - modT) + (m_tremoloModScale * modT);
            totalVol *= fac * modFac;
        }
        else if (m_tremoloScale)
        {
            float fac = (1.0f - t) + (m_tremoloScale * t);
            totalVol *= fac;
        }
        else if (m_tremoloModScale)
        {
            float modT = vox.getModWheel() / 127.f;
            float modFac = (1.0f - modT) + (m_tremoloModScale * modT);
            totalVol *= modFac;
        }
        m_volDirty = true;
    }

    /* Apply total volume */
    if (m_volDirty)
    {
        vox.setVolume(totalVol);
        m_volDirty = false;
    }

    /* Process active pan-sweep */
    if (m_panningTime >= 0.f)
    {
        m_panningTime += dt;
        float start = (m_panPos - 64) / 64.f;
        float end = (m_panPos + m_panWidth - 64) / 64.f;
        float t = std::max(0.f, std::min(1.f, m_panningTime / m_panningDur));
        vox.setPanning((start * (1.0f - t)) + (end * t));

        /* Done with panning */
        if (m_panningTime > m_panningDur)
            m_panningTime = -1.f;
    }

    /* Process active span-sweep */
    if (m_spanningTime >= 0.f)
    {
        m_spanningTime += dt;
        float start = (m_spanPos - 64) / 64.f;
        float end = (m_spanPos + m_spanWidth - 64) / 64.f;
        float t = std::max(0.f, std::min(1.f, m_spanningTime / m_spanningDur));
        vox.setSurroundPanning((start * (1.0f - t)) + (end * t));

        /* Done with spanning */
        if (m_spanningTime > m_spanningDur)
            m_spanningTime = -1.f;
    }

    /* Process pitch sweep 1 */
    if (m_pitchSweep1Times)
    {
        m_pitchSweep1 += m_pitchSweep1Add;
        --m_pitchSweep1Times;
        m_pitchDirty = true;
    }

    /* Process pitch sweep 2 */
    if (m_pitchSweep2Times)
    {
        m_pitchSweep2 += m_pitchSweep2Add;
        --m_pitchSweep2Times;
        m_pitchDirty = true;
    }

    /* Apply total pitch */
    if (m_pitchDirty)
    {
        vox.setPitchKey(m_curKey + m_pitchSweep1 + m_pitchSweep2);
        m_pitchDirty = false;
    }

    /* Loop through as many commands as we can for this time period */
    while (true)
    {
        /* Advance wait timer if active, returning if waiting */
        if (m_inWait)
        {
            m_waitCountdown -= dt;
            if (m_waitCountdown < 0.f)
                m_inWait = false;
            else
            {
                m_execTime += dt;
                return false;
            }
        }

        /* Load next command based on counter */
        const Command* commands = reinterpret_cast<const Command*>(m_ptr + sizeof(Header));
        Command cmd = commands[m_pc.back()++];
        cmd.swapBig();

        /* Perform function of command */
        switch (cmd.m_op)
        {
        case Op::End:
        case Op::Stop:
            m_pc.back() = -1;
            return true;
        case Op::SplitKey:
        {
            uint8_t keyNumber = cmd.m_data[0];
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            if (m_initKey >= keyNumber)
            {
                /* Do Branch */
                if (macroId == m_header.m_macroId)
                    m_pc.back() = macroStep;
                else
                    vox.loadSoundMacro(macroId, macroStep);
            }

            break;
        }
        case Op::SplitVel:
        {
            uint8_t velocity = cmd.m_data[0];
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            if (m_curVel >= velocity)
            {
                /* Do Branch */
                if (macroId == m_header.m_macroId)
                    m_pc.back() = macroStep;
                else
                    vox.loadSoundMacro(macroId, macroStep);
            }

            break;
        }
        case Op::WaitTicks:
        {
            bool keyRelease = cmd.m_data[0];
            bool random = cmd.m_data[1];
            bool sampleEnd = cmd.m_data[2];
            bool absolute = cmd.m_data[3];
            bool ms = cmd.m_data[4];
            int16_t time = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            /* Set wait state */
            float q = ms ? 1000.f : m_ticksPerSec;
            float secTime = time / q;
            if (absolute)
            {
                if (secTime <= m_execTime)
                    break;
                m_waitCountdown = secTime - m_execTime;
            }
            else
                m_waitCountdown = secTime;

            /* Randomize at the proper resolution */
            if (random)
                secTime = std::fmod(m_random() / q, secTime);

            m_inWait = true;
            m_keyoffWait = keyRelease;
            m_sampleEndWait = sampleEnd;
            break;
        }
        case Op::Loop:
        {
            bool keyRelease = cmd.m_data[0];
            bool random = cmd.m_data[1];
            bool sampleEnd = cmd.m_data[2];
            int16_t step = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);
            int16_t times = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            if ((keyRelease && m_keyoff) || (sampleEnd && m_sampleEnd))
            {
                /* Break out of loop */
                m_loopCountdown = -1;
                break;
            }

            if (random)
                times = m_random() % times;

            if (m_loopCountdown == -1 && times != -1)
                m_loopCountdown = times;

            if (m_loopCountdown > 0)
            {
                /* Loop back to step */
                --m_loopCountdown;
                m_pc.back() = step;
            }
            else /* Break out of loop */
                m_loopCountdown = -1;

            break;
        }
        case Op::Goto:
        {
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            /* Do Branch */
            if (macroId == m_header.m_macroId)
                m_pc.back() = macroStep;
            else
                vox.loadSoundMacro(macroId, macroStep);

            break;
        }
        case Op::WaitMs:
        {
            bool keyRelease = cmd.m_data[0];
            bool random = cmd.m_data[1];
            bool sampleEnd = cmd.m_data[2];
            bool absolute = cmd.m_data[3];
            int16_t time = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            /* Set wait state */
            float secTime = time / 1000.f;
            if (absolute)
            {
                if (secTime <= m_execTime)
                    break;
                m_waitCountdown = secTime - m_execTime;
            }
            else
                m_waitCountdown = secTime;

            /* Randomize at the proper resolution */
            if (random)
                secTime = std::fmod(m_random() / 1000.f, secTime);

            m_inWait = true;
            m_keyoffWait = keyRelease;
            m_sampleEndWait = sampleEnd;
            break;
        }
        case Op::PlayMacro:
        {
            int8_t addNote = cmd.m_data[0];
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);
            //int8_t priority = cmd.m_data[5];
            //int8_t maxVoices = cmd.m_data[6];

            Voice* sibVox = vox.startSiblingMacro(addNote, macroId, macroStep);
            if (sibVox)
                m_lastPlayMacroVid = sibVox->vid();

            break;
        }
        case Op::SendKeyOff:
        {
            uint8_t vid = cmd.m_data[0];
            bool lastStarted = cmd.m_data[1];

            if (lastStarted)
            {
                if (m_lastPlayMacroVid != -1)
                {
                    Voice* otherVox = vox.getEngine().findVoice(m_lastPlayMacroVid);
                    if (otherVox)
                        otherVox->keyOff();
                }
            }
            else
            {
                Voice* otherVox = vox.getEngine().findVoice(m_variables[vid]);
                if (otherVox)
                    otherVox->keyOff();
            }

            break;
        }
        case Op::SplitMod:
        {
            uint8_t mod = cmd.m_data[0];
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            if (m_curMod >= mod)
            {
                /* Do Branch */
                if (macroId == m_header.m_macroId)
                    m_pc.back() = macroStep;
                else
                    vox.loadSoundMacro(macroId, macroStep);
            }

            break;
        }
        case Op::PianoPan:
        {
            int8_t scale = cmd.m_data[0];
            int8_t cenKey = cmd.m_data[1];
            int8_t cenPan = cmd.m_data[2];

            int32_t pan = int32_t(m_initKey - cenKey) * scale / 127 + cenPan;
            pan = std::max(-127, std::min(127, pan));
            vox.setPanning(pan / 127.f);
            break;
        }
        case Op::SetAdsr:
        {
            ObjectId tableId = *reinterpret_cast<ObjectId*>(&cmd.m_data[0]);
            vox.setAdsr(tableId);
            break;
        }
        case Op::ScaleVolume:
        {
            int8_t scale = cmd.m_data[0];
            int8_t add = cmd.m_data[1];
            ObjectId curve = *reinterpret_cast<ObjectId*>(&cmd.m_data[2]);
            bool orgVel = cmd.m_data[4];

            int32_t eval = int32_t(orgVel ? m_initVel : m_curVel) * scale / 127 + add;
            eval = std::max(0, std::min(127, eval));

            if (curve.id != 0)
            {
                const Curve* curveData = vox.getAudioGroup().getPool().tableAsCurves(curve);
                if (curveData)
                {
                    m_curVol = (*curveData)[eval] / 127.f;
                    m_volDirty = true;
                    break;
                }
            }

            m_curVol = eval / 127.f;
            m_volDirty = true;
            break;
        }
        case Op::Panning:
        {
            int8_t panPos = cmd.m_data[0];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            int8_t width = cmd.m_data[3];

            m_panningTime = 0.f;
            m_panningDur = timeMs / 1000.f;
            m_panPos = panPos;
            m_panWidth = width;

            break;
        }
        case Op::Envelope:
        {
            int8_t scale = cmd.m_data[0];
            int8_t add = cmd.m_data[1];
            ObjectId curve = *reinterpret_cast<ObjectId*>(&cmd.m_data[2]);
            bool ms = cmd.m_data[4];
            int16_t fadeTime = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            float q = ms ? 1000.f : m_ticksPerSec;
            float secTime = fadeTime / q;

            int32_t eval = int32_t(m_curVel) * scale / 127 + add;
            eval = std::max(0, std::min(127, eval));

            m_envelopeTime = 0.f;
            m_envelopeDur = secTime;
            m_envelopeStart = m_curVel;
            m_envelopeEnd = eval;

            if (curve.id != 0)
                m_envelopeCurve = vox.getAudioGroup().getPool().tableAsCurves(curve);
            else
                m_envelopeCurve = nullptr;

            break;
        }
        case Op::StartSample:
        {
            int16_t smpId = *reinterpret_cast<int16_t*>(&cmd.m_data[0]);
            int8_t mode = cmd.m_data[2];
            int32_t offset = *reinterpret_cast<int32_t*>(&cmd.m_data[3]);

            switch (mode)
            {
            case 1:
                offset = offset * (127 - m_curVel) / 127;
                break;
            case 2:
                offset = offset * m_curVel / 127;
                break;
            default:
                break;
            }

            vox.startSample(smpId, offset);
            break;
        }
        case Op::StopSample:
        {
            vox.stopSample();
            break;
        }
        case Op::KeyOff:
        {
            vox.keyOff();
            break;
        }
        case Op::SplitRnd:
        {
            uint8_t rndVal = cmd.m_data[0];
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            if (rndVal <= m_random() % 256)
            {
                /* Do branch */
                if (macroId == m_header.m_macroId)
                    m_pc.back() = macroStep;
                else
                    vox.loadSoundMacro(macroId, macroStep);
            }

            break;
        }
        case Op::FadeIn:
        {
            int8_t scale = cmd.m_data[0];
            int8_t add = cmd.m_data[1];
            ObjectId curve = *reinterpret_cast<ObjectId*>(&cmd.m_data[2]);
            bool ms = cmd.m_data[4];
            int16_t fadeTime = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            float q = ms ? 1000.f : m_ticksPerSec;
            float secTime = fadeTime / q;

            int32_t eval = int32_t(m_curVel) * scale / 127 + add;
            eval = std::max(0, std::min(127, eval));

            m_envelopeTime = 0.f;
            m_envelopeDur = secTime;
            m_envelopeStart = 0.f;
            m_envelopeEnd = eval;

            if (curve.id != 0)
                m_envelopeCurve = vox.getAudioGroup().getPool().tableAsCurves(curve);
            else
                m_envelopeCurve = nullptr;

            break;
        }
        case Op::Spanning:
        {
            int8_t panPos = cmd.m_data[0];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            int8_t width = cmd.m_data[3];

            m_spanningTime = 0.f;
            m_spanningDur = timeMs / 1000.f;
            m_spanPos = panPos;
            m_spanWidth = width;

            break;
        }
        case Op::SetAdsrCtrl:
        {
            m_useAdsrControllers = true;
            m_midiAttack = cmd.m_data[0];
            m_midiDecay = cmd.m_data[1];
            m_midiSustain = cmd.m_data[2];
            m_midiRelease = cmd.m_data[3];
            break;
        }
        case Op::RndNote:
        {
            int32_t noteLo = int32_t(cmd.m_data[0]);
            int8_t detune = cmd.m_data[1];
            int32_t noteHi = int32_t(cmd.m_data[2]);
            int8_t free = cmd.m_data[3];
            int8_t rel = cmd.m_data[4];

            if (rel)
            {
                noteLo = m_initKey - noteLo;
                noteHi = noteLo + noteHi;
            }

            noteLo *= 100;
            noteHi *= 100;

            m_curKey = m_random() % (noteHi - noteLo) + noteLo;
            if (!free)
                m_curKey = m_curKey / 100 * 100 + detune;

            m_pitchDirty = true;
            break;
        }
        case Op::AddNote:
        {
            int32_t add = int32_t(cmd.m_data[0]);
            int8_t detune = cmd.m_data[1];
            int8_t orgKey = int32_t(cmd.m_data[2]);
            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            m_curKey = (orgKey ? m_initKey : m_curKey) + add * 100 + detune;

            /* Set wait state */
            if (timeMs)
            {
                float q = ms ? 1000.f : m_ticksPerSec;
                float secTime = timeMs / q;
                m_waitCountdown = secTime;
                m_inWait = true;
            }

            m_pitchDirty = true;
            break;
        }
        case Op::SetNote:
        {
            int32_t key = int32_t(cmd.m_data[0]);
            int8_t detune = cmd.m_data[1];
            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            m_curKey = key * 100 + detune;

            /* Set wait state */
            if (timeMs)
            {
                float q = ms ? 1000.f : m_ticksPerSec;
                float secTime = timeMs / q;
                m_waitCountdown = secTime;
                m_inWait = true;
            }

            m_pitchDirty = true;
            break;
        }
        case Op::LastNote:
        {
            int32_t add = int32_t(cmd.m_data[0]);
            int8_t detune = cmd.m_data[1];
            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            m_curKey = (add + vox.getLastNote()) * 100 + detune;

            /* Set wait state */
            if (timeMs)
            {
                float q = ms ? 1000.f : m_ticksPerSec;
                float secTime = timeMs / q;
                m_waitCountdown = secTime;
                m_inWait = true;
            }

            m_pitchDirty = true;
            break;
        }
        case Op::Portamento:
        {
            m_portamentoMode = cmd.m_data[0];
            m_portamentoType = cmd.m_data[1];
            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            float q = ms ? 1000.f : m_ticksPerSec;
            m_portamentoTime = timeMs / q;
            break;
        }
        case Op::Vibrato:
        {
            m_vibratoModLevel = m_vibratoLevel = cmd.m_data[0] * 100 + cmd.m_data[1];
            m_vibratoModWheel = cmd.m_data[2];
            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            float q = ms ? 1000.f : m_ticksPerSec;
            m_vibratoPeriod = timeMs / q;
            break;
        }
        case Op::PitchSweep1:
        {
            m_pitchSweep1 = 0;
            m_pitchSweep1Times = int32_t(cmd.m_data[0]);
            m_pitchSweep1Add = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);

            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            /* Set wait state */
            if (timeMs)
            {
                float q = ms ? 1000.f : m_ticksPerSec;
                float secTime = timeMs / q;
                m_waitCountdown = secTime;
                m_inWait = true;
            }

            break;
        }
        case Op::PitchSweep2:
        {
            m_pitchSweep2 = 0;
            m_pitchSweep2Times = int32_t(cmd.m_data[0]);
            m_pitchSweep2Add = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);

            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            /* Set wait state */
            if (timeMs)
            {
                float q = ms ? 1000.f : m_ticksPerSec;
                float secTime = timeMs / q;
                m_waitCountdown = secTime;
                m_inWait = true;
            }

            break;
        }
        case Op::SetPitch:
        {
            uint32_t hz = *reinterpret_cast<uint32_t*>(&cmd.m_data[0]) >> 8;
            uint16_t fine = *reinterpret_cast<uint16_t*>(&cmd.m_data[3]);
            vox.setPitchFrequency(hz, fine);
            break;
        }
        case Op::SetPitchAdsr:
        {
            ObjectId adsr = *reinterpret_cast<ObjectId*>(&cmd.m_data[0]);
            int8_t keys = cmd.m_data[3];
            int8_t cents = cmd.m_data[4];
            vox.setPitchAdsr(adsr, keys * 100 + cents);
            break;
        }
        case Op::ScaleVolumeDLS:
        {
            int16_t scale = *reinterpret_cast<int16_t*>(&cmd.m_data[0]);
            bool orgVel = cmd.m_data[2];
            m_curVol = int32_t(orgVel ? m_initVel : m_curVel) * scale / 4096.f / 127.f;
            m_volDirty = true;
            break;
        }
        case Op::Mod2Vibrange:
        {
            int8_t keys = cmd.m_data[0];
            int8_t cents = cmd.m_data[1];
            m_vibratoModLevel = keys * 100 + cents;
            break;
        }
        case Op::SetupTremolo:
        {
            int16_t scale = *reinterpret_cast<int16_t*>(&cmd.m_data[0]);
            int16_t modScale = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);
            m_tremoloScale = scale / 4096.f;
            m_tremoloModScale = modScale / 4096.f;
            break;
        }
        case Op::Return:
        case Op::GoSub:
        case Op::TrapEvent:
        case Op::SendMessage:
        case Op::GetMessage:
        case Op::GetVid:
        case Op::AddAgeCount:
        case Op::SetAgeCount:
        case Op::SendFlag:
        case Op::PitchWheelR:
        case Op::SetPriority:
        case Op::AddPriority:
        case Op::AgeCntSpeed:
        case Op::AgeCntVel:
            break;
        case Op::VolSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_volumeSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PanSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_panSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PitchWheelSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_pitchWheelSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::ModWheelSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_modWheelSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PedalSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_pedalSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PortASelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_portASel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::ReverbSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_reverbSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::SpanSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_spanSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::DopplerSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_dopplerSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::TremoloSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_tremoloSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PreASelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_preAuxASel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PreBSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_preAuxBSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PostBSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_postAuxB.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::AuxAFXSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_auxAFxSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::AuxBFXSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            LFOSel::Combine combine = LFOSel::Combine(cmd.m_data[3]);
            LFOSel::VarType vtype = LFOSel::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_auxBFxSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::SetupLFO:
        {
            uint8_t number = cmd.m_data[0];
            int16_t period = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            if (number <= 1)
                m_lfoPeriods[number] = period / 1000.f;
            break;
        }
        case Op::ModeSelect:
        case Op::SetKeygroup:
        case Op::SRCmodeSelect:
        case Op::AddVars:
        case Op::SubVars:
        case Op::MulVars:
        case Op::DivVars:
        case Op::AddIVars:
        case Op::IfEqual:
        case Op::IfLess:
        default:
            break;
        }
    }

    m_execTime += dt;
    return false;
}

void SoundMacroState::keyoff()
{
    m_keyoff = true;
    if (m_inWait && m_keyoffWait)
        m_inWait = false;
}

void SoundMacroState::sampleEnd()
{
    m_sampleEnd = true;
    if (m_inWait && m_sampleEndWait)
        m_inWait = false;
}

}

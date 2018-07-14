#include "amuse/SoundMacroState.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Engine.hpp"
#include "amuse/Common.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupPool.hpp"
#include <cstring>

/* Squelch Win32 macro pollution >.< */
#undef SendMessage
#undef GetMessage

namespace amuse
{

void SoundMacroState::Evaluator::addComponent(uint8_t midiCtrl, float scale, Combine combine, VarType varType)
{
    m_comps.push_back({midiCtrl, scale, combine, varType});
}

float SoundMacroState::Evaluator::evaluate(double time, const Voice& vox, const SoundMacroState& st) const
{
    float value = 0.f;

    /* Iterate each component */
    for (auto it = m_comps.cbegin(); it != m_comps.cend(); ++it)
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
                thisValue = vox.getAftertouch() * (2.f / 127.f);
                break;
            case 130:
                /* LFO1 */
                if (vox.m_lfoPeriods[0])
                    thisValue = std::sin(time / vox.m_lfoPeriods[0] * 2.f * M_PIF);
                break;
            case 131:
                /* LFO2 */
                if (vox.m_lfoPeriods[1])
                    thisValue = std::sin(time / vox.m_lfoPeriods[1] * 2.f * M_PIF);
                break;
            case 132:
                /* Surround panning */
                thisValue = vox.m_curSpan;
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
                thisValue = clamp(0.f, float(st.m_execTime * 1000.f), 16383.f);
                break;
            default:
                if (comp.m_midiCtrl == 10) /* Centered pan computation */
                    thisValue = vox.getCtrlValue(comp.m_midiCtrl) * (2.f / 127.f) - 1.f;
                else
                    thisValue = vox.getCtrlValue(comp.m_midiCtrl) * (2.f / 127.f);
                break;
            }
        }
        else if (comp.m_varType == VarType::Var)
            thisValue = st.m_variables[clamp(0, int(comp.m_midiCtrl), 255)];

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
            default:
                value = thisValue;
                break;
            }
        }
        else
            value = thisValue;
    }

    return value;
}

void SoundMacroState::initialize(ObjectId id, const SoundMacro* macro, int step)
{
    initialize(id, macro, step, 1000.f, 0, 0, 0);
}

void SoundMacroState::initialize(ObjectId id, const SoundMacro* macro, int step, double ticksPerSec,
                                 uint8_t midiKey, uint8_t midiVel, uint8_t midiMod)
{
    m_ticksPerSec = ticksPerSec;
    m_initKey = midiKey;
    m_initVel = midiVel;
    m_initMod = midiMod;
    m_curVel = midiVel;
    m_curMod = midiMod;
    m_curPitch = midiKey * 100;
    m_pc.clear();
    m_pc.emplace_back(id, macro, macro->assertPC(step));
    m_inWait = false;
    m_execTime = 0.f;
    m_keyoff = false;
    m_sampleEnd = false;
    m_loopCountdown = -1;
    m_lastPlayMacroVid = -1;
    m_useAdsrControllers = false;
    m_portamentoMode = 2;
    m_portamentoTime = 0.5f;
}

bool SoundMacro::CmdEnd::Do(SoundMacroState& st, Voice& vox) const
{
    st._setPC(-1);
    return true;
}

bool SoundMacro::CmdStop::Do(SoundMacroState& st, Voice& vox) const
{
    st._setPC(-1);
    return true;
}

bool SoundMacro::CmdSplitKey::Do(SoundMacroState& st, Voice& vox) const
{
    if (st.m_initKey >= key)
    {
        /* Do Branch */
        if (macro.id == std::get<0>(st.m_pc.back()))
            st._setPC(macroStep);
        else
            vox.loadSoundObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);
    }

    return false;
}

bool SoundMacro::CmdSplitVel::Do(SoundMacroState& st, Voice& vox) const
{
    if (st.m_curVel >= velocity)
    {
        /* Do Branch */
        if (macro.id == std::get<0>(st.m_pc.back()))
            st._setPC(macroStep);
        else
            vox.loadSoundObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);
    }

    return false;
}

bool SoundMacro::CmdWaitTicks::Do(SoundMacroState& st, Voice& vox) const
{
    /* Set wait state */
    if (ticksPerMs >= 0)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksPerMs / q;
        /* Randomize at the proper resolution */
        if (random)
            secTime = std::fmod(vox.getEngine().nextRandom() / q, secTime);

        if (absolute)
        {
            if (secTime <= st.m_execTime)
                return false;
            st.m_waitCountdown = secTime - st.m_execTime;
        }
        else
            st.m_waitCountdown = secTime;

        st.m_indefiniteWait = false;
    }
    else
        st.m_indefiniteWait = true;

    st.m_inWait = true;
    st.m_keyoffWait = keyOff;
    st.m_sampleEndWait = sampleEnd;

    return false;
}

bool SoundMacro::CmdLoop::Do(SoundMacroState& st, Voice& vox) const
{
    if ((keyOff && st.m_keyoff) || (sampleEnd && st.m_sampleEnd))
    {
        /* Break out of loop */
        st.m_loopCountdown = -1;
        return false;
    }

    uint32_t useTimes = times;
    if (random)
        useTimes = vox.getEngine().nextRandom() % times;

    if (st.m_loopCountdown == -1 && useTimes != -1)
        st.m_loopCountdown = useTimes;

    if (st.m_loopCountdown > 0)
    {
        /* Loop back to step */
        --st.m_loopCountdown;
        st._setPC(macroStep);
    }
    else /* Break out of loop */
        st.m_loopCountdown = -1;

    return false;
}

bool SoundMacro::CmdGoto::Do(SoundMacroState& st, Voice& vox) const
{
    /* Do Branch */
    if (macro.id == std::get<0>(st.m_pc.back()))
        st._setPC(macroStep);
    else
        vox.loadSoundObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);

    return false;
}

bool SoundMacro::CmdWaitMs::Do(SoundMacroState& st, Voice& vox) const
{
    /* Set wait state */
    if (ms >= 0)
    {
        float secTime = ms / 1000.f;
        /* Randomize at the proper resolution */
        if (random)
            secTime = std::fmod(vox.getEngine().nextRandom() / 1000.f, secTime);

        if (absolute)
        {
            if (secTime <= st.m_execTime)
                return false;
            st.m_waitCountdown = secTime - st.m_execTime;
        }
        else
            st.m_waitCountdown = secTime;

        st.m_indefiniteWait = false;
    }
    else
        st.m_indefiniteWait = true;

    st.m_inWait = true;
    st.m_keyoffWait = keyOff;
    st.m_sampleEndWait = sampleEnd;

    return false;
}

bool SoundMacro::CmdPlayMacro::Do(SoundMacroState& st, Voice& vox) const
{
    std::shared_ptr<Voice> sibVox = vox.startChildMacro(addNote, macro.id, macroStep);
    if (sibVox)
        st.m_lastPlayMacroVid = sibVox->vid();

    return false;
}

bool SoundMacro::CmdSendKeyOff::Do(SoundMacroState& st, Voice& vox) const
{
    if (lastStarted)
    {
        if (st.m_lastPlayMacroVid != -1)
        {
            std::shared_ptr<Voice> otherVox = vox.getEngine().findVoice(st.m_lastPlayMacroVid);
            if (otherVox)
                otherVox->keyOff();
        }
    }
    else
    {
        std::shared_ptr<Voice> otherVox = vox.getEngine().findVoice(st.m_variables[variable]);
        if (otherVox)
            otherVox->keyOff();
    }

    return false;
}

bool SoundMacro::CmdSplitMod::Do(SoundMacroState& st, Voice& vox) const
{
    if (st.m_curMod >= modValue)
    {
        /* Do Branch */
        if (macro.id == std::get<0>(st.m_pc.back()))
            st._setPC(macroStep);
        else
            vox.loadSoundObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);
    }

    return false;
}

bool SoundMacro::CmdPianoPan::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t pan = int32_t(st.m_initKey - centerKey) * scale / 127 + centerPan;
    pan = std::max(-127, std::min(127, pan));
    vox.setPan(pan / 127.f);

    return false;
}

bool SoundMacro::CmdSetAdsr::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setAdsr(table.id, dlsMode);
    return false;
}

bool SoundMacro::CmdScaleVolume::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t eval = int32_t(originalVol ? st.m_initVel : st.m_curVel) * scale / 127 + add;
    eval = clamp(0, eval, 127);

    if (table.id != 0)
    {
        const Curve* curveData = vox.getAudioGroup().getPool().tableAsCurves(table.id);
        if (curveData)
        {
            vox.m_curVol = curveData->data.at(eval) / 127.f;
            return false;
        }
    }

    vox.m_curVol = eval / 127.f;

    return false;
}

bool SoundMacro::CmdPanning::Do(SoundMacroState& st, Voice& vox) const
{
    vox.startPanning(timeMs / 1000.0, panPosition, width);
    return false;
}

bool SoundMacro::CmdEnvelope::Do(SoundMacroState& st, Voice& vox) const
{
    double q = msSwitch ? 1000.0 : st.m_ticksPerSec;
    double secTime = fadeTime / q;

    int32_t eval = int32_t(st.m_curVel) * scale / 127 + add;
    eval = clamp(0, eval, 127);

    const Curve* curveData;
    if (table.id != 0)
        curveData = vox.getAudioGroup().getPool().tableAsCurves(table.id);
    else
        curveData = nullptr;

    vox.startEnvelope(secTime, eval, curveData);

    return false;
}

bool SoundMacro::CmdStartSample::Do(SoundMacroState& st, Voice& vox) const
{
    uint32_t useOffset = offset;

    switch (mode)
    {
    case 1:
        useOffset = offset * (127 - st.m_curVel) / 127;
        break;
    case 2:
        useOffset = offset * st.m_curVel / 127;
        break;
    default:
        break;
    }

    vox.startSample(sample.id, useOffset);
    vox.setPitchKey(st.m_curPitch);

    return false;
}

bool SoundMacro::CmdStopSample::Do(SoundMacroState& st, Voice& vox) const
{
    vox.stopSample();
    return false;
}

bool SoundMacro::CmdKeyOff::Do(SoundMacroState& st, Voice& vox) const
{
    vox._macroKeyOff();
    return false;
}

bool SoundMacro::CmdSplitRnd::Do(SoundMacroState& st, Voice& vox) const
{
    if (rnd <= vox.getEngine().nextRandom() % 256)
    {
        /* Do branch */
        if (macro.id == std::get<0>(st.m_pc.back()))
            st._setPC(macroStep);
        else
            vox.loadSoundObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);
    }

    return false;
}

bool SoundMacro::CmdFadeIn::Do(SoundMacroState& st, Voice& vox) const
{
    float q = msSwitch ? 1000.f : st.m_ticksPerSec;
    float secTime = ticksPerMs / q;

    int32_t eval = int32_t(st.m_curVel) * scale / 127 + add;
    eval = clamp(0, eval, 127);

    const Curve* curveData;
    if (table.id != 0)
        curveData = vox.getAudioGroup().getPool().tableAsCurves(table.id);
    else
        curveData = nullptr;

    vox.startFadeIn(secTime, eval, curveData);

    return false;
}

bool SoundMacro::CmdSpanning::Do(SoundMacroState& st, Voice& vox) const
{
    vox.startSpanning(timeMs / 1000.0, spanPosition, width);
    return false;
}

bool SoundMacro::CmdSetAdsrCtrl::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_useAdsrControllers = true;
    st.m_midiAttack = attack;
    st.m_midiDecay = decay;
    st.m_midiSustain = sustain;
    st.m_midiRelease = release;

    /* Bootstrap ADSR defaults here */
    if (!vox.getCtrlValue(st.m_midiSustain))
    {
        vox.setCtrlValue(st.m_midiAttack, 10);
        vox.setCtrlValue(st.m_midiSustain, 127);
        vox.setCtrlValue(st.m_midiRelease, 10);
    }

    return false;
}

bool SoundMacro::CmdRndNote::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useNoteLo = noteLo;
    int32_t useNoteHi = noteHi;

    if (absRel)
    {
        useNoteLo = st.m_initKey - noteLo;
        useNoteHi = noteLo + noteHi;
    }

    useNoteLo *= 100;
    useNoteHi *= 100;

    if (useNoteHi == useNoteLo)
        st.m_curPitch = useNoteHi;
    else
        st.m_curPitch = (vox.getEngine().nextRandom() % (useNoteHi - useNoteLo)) + useNoteLo;

    if (!fixedFree)
        st.m_curPitch = st.m_curPitch / 100 * 100 + detune;

    vox.setPitchKey(st.m_curPitch);

    return false;
}

bool SoundMacro::CmdAddNote::Do(SoundMacroState& st, Voice& vox) const
{
    /* Set wait state */
    if (msSwitch)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksPerMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchKey(st.m_curPitch);

    return false;
}

bool SoundMacro::CmdSetNote::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_curPitch = key * 100 + detune;

    /* Set wait state */
    if (ticksPerMs)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksPerMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchKey(st.m_curPitch);

    return false;
}

bool SoundMacro::CmdLastNote::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_curPitch = (add + vox.getLastNote()) * 100 + detune;

    /* Set wait state */
    if (msSwitch)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksPerMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchKey(st.m_curPitch);

    return false;
}

bool SoundMacro::CmdPortamento::Do(SoundMacroState& st, Voice& vox) const
{
    float q = msSwitch ? 1000.f : st.m_ticksPerSec;
    st.m_portamentoTime = ticksPerMs / q;
    return false;
}

bool SoundMacro::CmdVibrato::Do(SoundMacroState& st, Voice& vox) const
{
    float q = msSwitch ? 1000.f : st.m_ticksPerSec;
    vox.setVibrato(levelNote * 100 + levelFine, modwheelFlag, ticksPerMs / q);
    return false;
}

bool SoundMacro::CmdPitchSweep1::Do(SoundMacroState& st, Voice& vox) const
{
    /* Set wait state */
    if (msSwitch)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksPerMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchSweep1(times, add);

    return false;
}

bool SoundMacro::CmdPitchSweep2::Do(SoundMacroState& st, Voice& vox) const
{
    /* Set wait state */
    if (msSwitch)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksPerMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchSweep2(times, add);

    return false;
}

bool SoundMacro::CmdSetPitch::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setPitchFrequency(hz, fine);
    return false;
}

bool SoundMacro::CmdSetPitchAdsr::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setPitchAdsr(table.id, keys * 100 + cents);
    return false;
}

bool SoundMacro::CmdScaleVolumeDLS::Do(SoundMacroState& st, Voice& vox) const
{
    vox.m_curVol = int32_t(originalVol ? st.m_initVel : st.m_curVel) * scale / 4096.f / 127.f;
    return false;
}

bool SoundMacro::CmdMod2Vibrange::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setMod2VibratoRange(keys * 100 + cents);
    return false;
}

bool SoundMacro::CmdSetupTremolo::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setTremolo(scale / 4096.f, scale / 4096.f);
    return false;
}

bool SoundMacro::CmdReturn::Do(SoundMacroState& st, Voice& vox) const
{
    if (st.m_pc.size() > 1)
    {
        st.m_pc.pop_back();
        vox._setObjectId(std::get<0>(st.m_pc.back()));
    }
    return false;
}

bool SoundMacro::CmdGoSub::Do(SoundMacroState& st, Voice& vox) const
{
    if (macro.id == std::get<0>(st.m_pc.back()))
        st.m_pc.emplace_back(std::get<0>(st.m_pc.back()), std::get<1>(st.m_pc.back()),
            std::get<1>(st.m_pc.back())->assertPC(macroStep));
    else
        vox.loadSoundObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod, true);


    vox._setObjectId(std::get<0>(st.m_pc.back()));

    return false;
}

bool SoundMacro::CmdTrapEvent::Do(SoundMacroState& st, Voice& vox) const
{
    switch (event)
    {
    case 0:
        vox.m_keyoffTrap.macroId = macro.id;
        vox.m_keyoffTrap.macroStep = macroStep;
        break;
    case 1:
        vox.m_sampleEndTrap.macroId = macro.id;
        vox.m_sampleEndTrap.macroStep = macroStep;
        break;
    case 2:
        vox.m_messageTrap.macroId = macro.id;
        vox.m_messageTrap.macroStep = macroStep;
        break;
    default:
        break;
    }

    return false;
}

bool SoundMacro::CmdUntrapEvent::Do(SoundMacroState& st, Voice& vox) const
{
    switch (event)
    {
    case 0:
        vox.m_keyoffTrap.macroId = 0xffff;
        vox.m_keyoffTrap.macroStep = 0xffff;
        break;
    case 1:
        vox.m_sampleEndTrap.macroId = 0xffff;
        vox.m_sampleEndTrap.macroStep = 0xffff;
        break;
    case 2:
        vox.m_messageTrap.macroId = 0xffff;
        vox.m_messageTrap.macroStep = 0xffff;
        break;
    default:
        break;
    }

    return false;
}

bool SoundMacro::CmdSendMessage::Do(SoundMacroState& st, Voice& vox) const
{
    if (isVar)
    {
        std::shared_ptr<Voice> findVox = vox.getEngine().findVoice(st.m_variables[vid]);
        if (findVox)
            findVox->message(variable);
    }
    else
        vox.getEngine().sendMacroMessage(macro.id, variable);

    return false;
}

bool SoundMacro::CmdGetMessage::Do(SoundMacroState& st, Voice& vox) const
{
    if (vox.m_messageQueue.size())
    {
        st.m_variables[variable] = vox.m_messageQueue.front();
        vox.m_messageQueue.pop_front();
    }
    else
        st.m_variables[variable] = 0;

    return false;
}

bool SoundMacro::CmdGetVid::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_variables[variable] = playMacro ? st.m_lastPlayMacroVid : vox.vid();
    return false;
}

bool SoundMacro::CmdAddAgeCount::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

bool SoundMacro::CmdSetAgeCount::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

bool SoundMacro::CmdSendFlag::Do(SoundMacroState& st, Voice& vox) const
{
    /* TODO: figure out a good API */
    return false;
}

bool SoundMacro::CmdPitchWheelR::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setPitchWheelRange(rangeUp, rangeDown);
    return false;
}

bool SoundMacro::CmdSetPriority::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

bool SoundMacro::CmdAddPriority::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

bool SoundMacro::CmdAgeCntSpeed::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

bool SoundMacro::CmdAgeCntVel::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

bool SoundMacro::CmdVolSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_volumeSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                SoundMacroState::Evaluator::Combine(combine),
                                SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdPanSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_panSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                             SoundMacroState::Evaluator::Combine(combine),
                             SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdPitchWheelSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_pitchWheelSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                    SoundMacroState::Evaluator::Combine(combine),
                                    SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdModWheelSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_modWheelSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                  SoundMacroState::Evaluator::Combine(combine),
                                  SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdPedalSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_pedalSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                               SoundMacroState::Evaluator::Combine(combine),
                               SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdPortamentoSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_portamentoSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                    SoundMacroState::Evaluator::Combine(combine),
                                    SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdReverbSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_reverbSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                SoundMacroState::Evaluator::Combine(combine),
                                SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdSpanSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_spanSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                              SoundMacroState::Evaluator::Combine(combine),
                              SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdDopplerSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_dopplerSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                 SoundMacroState::Evaluator::Combine(combine),
                                 SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdTremoloSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_tremoloSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                 SoundMacroState::Evaluator::Combine(combine),
                                 SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdPreASelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_preAuxASel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                 SoundMacroState::Evaluator::Combine(combine),
                                 SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdPreBSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_preAuxBSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                 SoundMacroState::Evaluator::Combine(combine),
                                 SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdPostBSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_postAuxB.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                               SoundMacroState::Evaluator::Combine(combine),
                               SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdAuxAFXSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_auxAFxSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                SoundMacroState::Evaluator::Combine(combine),
                                SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdAuxBFXSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_auxBFxSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                SoundMacroState::Evaluator::Combine(combine),
                                SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

bool SoundMacro::CmdSetupLFO::Do(SoundMacroState& st, Voice& vox) const
{
    if (lfoNumber == 0)
        vox.setLFO1Period(periodInMs / 1000.f);
    else if (lfoNumber == 1)
        vox.setLFO2Period(periodInMs / 1000.f);
    return false;
}

bool SoundMacro::CmdModeSelect::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

bool SoundMacro::CmdSetKeygroup::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setKeygroup(0);
    if (group)
    {
        vox.getEngine().killKeygroup(group, killNow);
        vox.setKeygroup(group);
    }
    return false;
}

bool SoundMacro::CmdSRCmodeSelect::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

bool SoundMacro::CmdAddVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB, useC;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b];

    if (varCtrlC)
        useC = vox.getCtrlValue(c);
    else
        useC = st.m_variables[c];

    if (varCtrlA)
        vox.setCtrlValue(a, useB + useC);
    else
        st.m_variables[a] = useB + useC;

    return false;
}

bool SoundMacro::CmdSubVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB, useC;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b];

    if (varCtrlC)
        useC = vox.getCtrlValue(c);
    else
        useC = st.m_variables[c];

    if (varCtrlA)
        vox.setCtrlValue(a, useB - useC);
    else
        st.m_variables[a] = useB - useC;

    return false;
}

bool SoundMacro::CmdMulVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB, useC;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b];

    if (varCtrlC)
        useC = vox.getCtrlValue(c);
    else
        useC = st.m_variables[c];

    if (varCtrlA)
        vox.setCtrlValue(a, useB * useC);
    else
        st.m_variables[a] = useB * useC;

    return false;
}

bool SoundMacro::CmdDivVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB, useC;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b];

    if (varCtrlC)
        useC = vox.getCtrlValue(c);
    else
        useC = st.m_variables[c];

    if (varCtrlA)
        vox.setCtrlValue(a, useB / useC);
    else
        st.m_variables[a] = useB / useC;

    return false;
}

bool SoundMacro::CmdAddIVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b];

    if (varCtrlA)
        vox.setCtrlValue(a, useB + imm);
    else
        st.m_variables[a] = useB + imm;

    return false;
}

bool SoundMacro::CmdIfEqual::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useA, useB;

    if (varCtrlA)
        useA = vox.getCtrlValue(a);
    else
        useA = st.m_variables[a];

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b];

    if ((useA == useB) ^ notEq)
        st._setPC(macroStep);

    return false;
}

bool SoundMacro::CmdIfLess::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useA, useB;

    if (varCtrlA)
        useA = vox.getCtrlValue(a);
    else
        useA = st.m_variables[a];

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b];

    if ((useA < useB) ^ notLt)
        st._setPC(macroStep);

    return false;
}

bool SoundMacroState::advance(Voice& vox, double dt)
{
    /* Nothing if uninitialized or finished */
    if (m_pc.empty() || std::get<1>(m_pc.back()) == nullptr || std::get<2>(m_pc.back()) == -1)
        return true;

    /* Loop through as many commands as we can for this time period */
    while (true)
    {
        /* Advance wait timer if active, returning if waiting */
        if (m_inWait)
        {
            if (m_keyoffWait && m_keyoff)
                m_inWait = false;
            else if (m_sampleEndWait && m_sampleEnd)
                m_inWait = false;
            else if (!m_indefiniteWait)
            {
                m_waitCountdown -= dt;
                if (m_waitCountdown < 0.f)
                    m_inWait = false;
            }

            if (m_inWait)
            {
                m_execTime += dt;
                return false;
            }
        }

        /* Load next command based on counter */
        const SoundMacro::ICmd& cmd = std::get<1>(m_pc.back())->getCmd(std::get<2>(m_pc.back())++);

        /* Perform function of command */
        if (cmd.Do(*this, vox))
            return true;
    }

    m_execTime += dt;
    return false;
}

void SoundMacroState::keyoffNotify(Voice& vox) { m_keyoff = true; }

void SoundMacroState::sampleEndNotify(Voice& vox) { m_sampleEnd = true; }
}

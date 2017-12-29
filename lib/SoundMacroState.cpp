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

int SoundMacroState::_assertPC(int pc, uint32_t size)
{
    if (pc == -1)
        return -1;
    int cmdCount = (size - sizeof(Header)) / sizeof(Command);
    if (pc >= cmdCount)
    {
        fprintf(stderr, "SoundMacro PC bounds exceeded [%d/%d]\n", pc, cmdCount);
        abort();
    }
    return pc;
}

void SoundMacroState::Header::swapBig()
{
    m_size = SBig(m_size);
    m_macroId = SBig(m_macroId);
}

void SoundMacroState::Command::swapBig()
{
    uint32_t* words = reinterpret_cast<uint32_t*>(this);
    words[0] = SBig(words[0]);
    words[1] = SBig(words[1]);
}

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

void SoundMacroState::initialize(const unsigned char* ptr, int step, bool swapData)
{
    initialize(ptr, step, 1000.f, 0, 0, 0, swapData);
}

void SoundMacroState::initialize(const unsigned char* ptr, int step, double ticksPerSec, uint8_t midiKey,
                                 uint8_t midiVel, uint8_t midiMod, bool swapData)
{
    m_ticksPerSec = ticksPerSec;
    m_initKey = midiKey;
    m_initVel = midiVel;
    m_initMod = midiMod;
    m_curVel = midiVel;
    m_curMod = midiMod;
    m_curPitch = midiKey * 100;
    m_pc.clear();
    const Header& header = reinterpret_cast<const Header&>(ptr);
    m_pc.push_back({ptr, _assertPC(step, header.m_size, swapData)});
    m_inWait = false;
    m_execTime = 0.f;
    m_keyoff = false;
    m_sampleEnd = false;
    m_loopCountdown = -1;
    m_lastPlayMacroVid = -1;
    m_useAdsrControllers = false;
    m_portamentoMode = 2;
    m_portamentoTime = 0.5f;
    m_header = header;
    if (swapData)
        m_header.swapBig();
}

bool SoundMacroState::advance(Voice& vox, double dt)
{
    /* Nothing if uninitialized or finished */
    if (m_pc.empty() || m_pc.back().first == nullptr || m_pc.back().second == -1)
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
        const Command* commands = reinterpret_cast<const Command*>(m_pc.back().first + sizeof(Header));
        _assertPC(m_pc.back().second, m_header.m_size);
        Command cmd = commands[m_pc.back().second++];
        if (vox.getAudioGroup().getDataFormat() != DataFormat::PC)
            cmd.swapBig();

        /* Perform function of command */
        switch (cmd.m_op)
        {
        case Op::End:
        case Op::Stop:
            _setPC(-1);
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
                    _setPC(macroStep);
                else
                    vox.loadSoundObject(macroId, macroStep, m_ticksPerSec, m_initKey, m_initVel, m_initMod);
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
                    _setPC(macroStep);
                else
                    vox.loadSoundObject(macroId, macroStep, m_ticksPerSec, m_initKey, m_initVel, m_initMod);
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
            if (time >= 0)
            {
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
                    secTime = std::fmod(vox.getEngine().nextRandom() / q, secTime);

                m_indefiniteWait = false;
            }
            else
                m_indefiniteWait = true;

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
                times = vox.getEngine().nextRandom() % times;

            if (m_loopCountdown == -1 && times != -1)
                m_loopCountdown = times;

            if (m_loopCountdown > 0)
            {
                /* Loop back to step */
                --m_loopCountdown;
                _setPC(step);
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
                _setPC(macroStep);
            else
                vox.loadSoundObject(macroId, macroStep, m_ticksPerSec, m_initKey, m_initVel, m_initMod);

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
            if (time >= 0)
            {
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
                    secTime = std::fmod(vox.getEngine().nextRandom() / 1000.f, secTime);

                m_indefiniteWait = false;
            }
            else
                m_indefiniteWait = true;

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
            // int8_t priority = cmd.m_data[5];
            // int8_t maxVoices = cmd.m_data[6];

            std::shared_ptr<Voice> sibVox = vox.startChildMacro(addNote, macroId, macroStep);
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
                    std::shared_ptr<Voice> otherVox = vox.getEngine().findVoice(m_lastPlayMacroVid);
                    if (otherVox)
                        otherVox->keyOff();
                }
            }
            else
            {
                std::shared_ptr<Voice> otherVox = vox.getEngine().findVoice(m_variables[vid]);
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
                    _setPC(macroStep);
                else
                    vox.loadSoundObject(macroId, macroStep, m_ticksPerSec, m_initKey, m_initVel, m_initMod);
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
            vox.setPan(pan / 127.f);
            break;
        }
        case Op::SetAdsr:
        {
            ObjectId tableId = *reinterpret_cast<ObjectId*>(&cmd.m_data[0]);
            bool dlsMode = cmd.m_data[2];
            vox.setAdsr(tableId, dlsMode);
            break;
        }
        case Op::ScaleVolume:
        {
            int8_t scale = cmd.m_data[0];
            int8_t add = cmd.m_data[1];
            ObjectId curve = *reinterpret_cast<ObjectId*>(&cmd.m_data[2]);
            bool orgVel = cmd.m_data[4];

            int32_t eval = int32_t(orgVel ? m_initVel : m_curVel) * scale / 127 + add;
            eval = clamp(0, eval, 127);

            if (curve != 0)
            {
                const Curve* curveData = vox.getAudioGroup().getPool().tableAsCurves(curve);
                if (curveData)
                {
                    vox.m_curVol = (*curveData)[eval] / 127.f;
                    break;
                }
            }

            vox.m_curVol = eval / 127.f;
            break;
        }
        case Op::Panning:
        {
            int8_t panPos = cmd.m_data[0];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            int8_t width = cmd.m_data[3];

            vox.startPanning(timeMs / 1000.0, panPos, width);
            break;
        }
        case Op::Envelope:
        {
            int8_t scale = cmd.m_data[0];
            int8_t add = cmd.m_data[1];
            ObjectId curve = *reinterpret_cast<ObjectId*>(&cmd.m_data[2]);
            bool ms = cmd.m_data[4];
            int16_t fadeTime = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            double q = ms ? 1000.0 : m_ticksPerSec;
            double secTime = fadeTime / q;

            int32_t eval = int32_t(m_curVel) * scale / 127 + add;
            eval = clamp(0, eval, 127);

            const Curve* curveData;
            if (curve != 0)
                curveData = vox.getAudioGroup().getPool().tableAsCurves(curve);
            else
                curveData = nullptr;

            vox.startEnvelope(secTime, eval, curveData);
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
            vox.setPitchKey(m_curPitch);
            break;
        }
        case Op::StopSample:
        {
            vox.stopSample();
            break;
        }
        case Op::KeyOff:
        {
            vox._macroKeyOff();
            break;
        }
        case Op::SplitRnd:
        {
            uint8_t rndVal = cmd.m_data[0];
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            if (rndVal <= vox.getEngine().nextRandom() % 256)
            {
                /* Do branch */
                if (macroId == m_header.m_macroId)
                    _setPC(macroStep);
                else
                    vox.loadSoundObject(macroId, macroStep, m_ticksPerSec, m_initKey, m_initVel, m_initMod);
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
            eval = clamp(0, eval, 127);

            const Curve* curveData;
            if (curve != 0)
                curveData = vox.getAudioGroup().getPool().tableAsCurves(curve);
            else
                curveData = nullptr;

            vox.startFadeIn(secTime, eval, curveData);
            break;
        }
        case Op::Spanning:
        {
            int8_t panPos = cmd.m_data[0];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            int8_t width = cmd.m_data[3];

            vox.startSpanning(timeMs / 1000.0, panPos, width);
            break;
        }
        case Op::SetAdsrCtrl:
        {
            m_useAdsrControllers = true;
            m_midiAttack = cmd.m_data[0];
            m_midiDecay = cmd.m_data[1];
            m_midiSustain = cmd.m_data[2];
            m_midiRelease = cmd.m_data[3];

            /* Bootstrap ADSR defaults here */
            if (!vox.getCtrlValue(m_midiSustain))
            {
                vox.setCtrlValue(m_midiAttack, 10);
                vox.setCtrlValue(m_midiSustain, 127);
                vox.setCtrlValue(m_midiRelease, 10);
            }

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

            if (noteHi == noteLo)
                m_curPitch = noteHi;
            else
                m_curPitch = (vox.getEngine().nextRandom() % (noteHi - noteLo)) + noteLo;

            if (!free)
                m_curPitch = m_curPitch / 100 * 100 + detune;

            vox.setPitchKey(m_curPitch);
            break;
        }
        case Op::AddNote:
        {
            int32_t add = int32_t(cmd.m_data[0]);
            int8_t detune = cmd.m_data[1];
            int8_t orgKey = cmd.m_data[2];
            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            m_curPitch = (orgKey ? (m_initKey * 100) : m_curPitch) + add * 100 + detune;

            /* Set wait state */
            if (timeMs)
            {
                float q = ms ? 1000.f : m_ticksPerSec;
                float secTime = timeMs / q;
                m_waitCountdown = secTime;
                m_inWait = true;
            }

            vox.setPitchKey(m_curPitch);
            break;
        }
        case Op::SetNote:
        {
            int32_t key = int32_t(cmd.m_data[0]);
            int8_t detune = cmd.m_data[1];
            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            m_curPitch = key * 100 + detune;

            /* Set wait state */
            if (timeMs)
            {
                float q = ms ? 1000.f : m_ticksPerSec;
                float secTime = timeMs / q;
                m_waitCountdown = secTime;
                m_inWait = true;
            }

            vox.setPitchKey(m_curPitch);
            break;
        }
        case Op::LastNote:
        {
            int32_t add = int32_t(cmd.m_data[0]);
            int8_t detune = cmd.m_data[1];
            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            m_curPitch = (add + vox.getLastNote()) * 100 + detune;

            /* Set wait state */
            if (timeMs)
            {
                float q = ms ? 1000.f : m_ticksPerSec;
                float secTime = timeMs / q;
                m_waitCountdown = secTime;
                m_inWait = true;
            }

            vox.setPitchKey(m_curPitch);
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
            int32_t level = cmd.m_data[0] * 100 + cmd.m_data[1];
            int32_t modLevel = cmd.m_data[2];
            int8_t ms = cmd.m_data[4];
            int16_t timeMs = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            float q = ms ? 1000.f : m_ticksPerSec;
            vox.setVibrato(level, modLevel, timeMs / q);
            break;
        }
        case Op::PitchSweep1:
        {
            int32_t times = int32_t(cmd.m_data[0]);
            int16_t add = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);

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

            vox.setPitchSweep1(times, add);
            break;
        }
        case Op::PitchSweep2:
        {
            int32_t times = int32_t(cmd.m_data[0]);
            int16_t add = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);

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

            vox.setPitchSweep2(times, add);
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
            vox.m_curVol = int32_t(orgVel ? m_initVel : m_curVel) * scale / 4096.f / 127.f;
            break;
        }
        case Op::Mod2Vibrange:
        {
            int8_t keys = cmd.m_data[0];
            int8_t cents = cmd.m_data[1];
            vox.setMod2VibratoRange(keys * 100 + cents);
            break;
        }
        case Op::SetupTremolo:
        {
            int16_t scale = *reinterpret_cast<int16_t*>(&cmd.m_data[0]);
            int16_t modScale = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);
            vox.setTremolo(scale / 4096.f, modScale / 4096.f);
            break;
        }
        case Op::Return:
        {
            if (m_pc.size() > 1)
            {
                m_pc.pop_back();
                m_header = *reinterpret_cast<const Header*>(m_pc.back().first);
                if (vox.getAudioGroup().getDataFormat() != DataFormat::PC)
                    m_header.swapBig();
                vox.m_objectId = m_header.m_macroId;
            }
            break;
        }
        case Op::GoSub:
        {
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            if (macroId == m_header.m_macroId)
                m_pc.push_back({m_pc.back().first, _assertPC(macroStep, m_header.m_size)});
            else
                vox.loadSoundObject(macroId, macroStep, m_ticksPerSec, m_initKey, m_initVel, m_initMod, true);

            m_header = *reinterpret_cast<const Header*>(m_pc.back().first);
            if (vox.getAudioGroup().getDataFormat() != DataFormat::PC)
                m_header.swapBig();
            vox.m_objectId = m_header.m_macroId;

            break;
        }
        case Op::TrapEvent:
        {
            uint8_t event = cmd.m_data[0];
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            switch (event)
            {
            case 0:
                vox.m_keyoffTrap.macroId = macroId;
                vox.m_keyoffTrap.macroStep = macroStep;
                break;
            case 1:
                vox.m_sampleEndTrap.macroId = macroId;
                vox.m_sampleEndTrap.macroStep = macroStep;
                break;
            case 2:
                vox.m_messageTrap.macroId = macroId;
                vox.m_messageTrap.macroStep = macroStep;
                break;
            default:
                break;
            }

            break;
        }
        case Op::UntrapEvent:
        {
            uint8_t event = cmd.m_data[0];

            switch (event)
            {
            case 0:
                vox.m_keyoffTrap.macroId = 0xffff;
                vox.m_keyoffTrap.macroStep = -1;
                break;
            case 1:
                vox.m_sampleEndTrap.macroId = 0xffff;
                vox.m_sampleEndTrap.macroStep = -1;
                break;
            case 2:
                vox.m_messageTrap.macroId = 0xffff;
                vox.m_messageTrap.macroStep = -1;
                break;
            default:
                break;
            }

            break;
        }
        case Op::SendMessage:
        {
            bool isVar = cmd.m_data[0];
            ObjectId macroId = *reinterpret_cast<ObjectId*>(&cmd.m_data[1]);
            uint8_t vid = cmd.m_data[3];
            uint8_t val = cmd.m_data[4];

            if (isVar)
            {
                std::shared_ptr<Voice> findVox = vox.getEngine().findVoice(m_variables[vid]);
                if (findVox)
                    findVox->message(val);
            }
            else
                vox.getEngine().sendMacroMessage(macroId, val);

            break;
        }
        case Op::GetMessage:
        {
            uint8_t vid = cmd.m_data[0];
            if (vox.m_messageQueue.size())
            {
                m_variables[vid] = vox.m_messageQueue.front();
                vox.m_messageQueue.pop_front();
            }
            else
                m_variables[vid] = 0;
            break;
        }
        case Op::GetVid:
        {
            uint8_t vid = cmd.m_data[0];
            bool lastPlayMacro = cmd.m_data[1];
            m_variables[vid] = lastPlayMacro ? m_lastPlayMacroVid : vox.vid();
            break;
        }
        case Op::SendFlag:
        {
            int8_t id = cmd.m_data[0];
            int8_t val = cmd.m_data[1];
            break; /* TODO: figure out a good API */
        }
        case Op::PitchWheelR:
        {
            int8_t up = cmd.m_data[0];
            int8_t down = cmd.m_data[1];
            vox.setPitchWheelRange(up, down);
            break;
        }
        case Op::VolSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_volumeSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PanSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_panSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PitchWheelSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_pitchWheelSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::ModWheelSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_modWheelSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PedalSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_pedalSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PortamentoSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_portamentoSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::ReverbSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_reverbSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::SpanSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_spanSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::DopplerSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_dopplerSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::TremoloSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_tremoloSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PreASelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_preAuxASel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PreBSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_preAuxBSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::PostBSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_postAuxB.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::AuxAFXSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_auxAFxSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::AuxBFXSelect:
        {
            uint8_t ctrl = cmd.m_data[0];
            int16_t perc = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            Evaluator::Combine combine = Evaluator::Combine(cmd.m_data[3]);
            Evaluator::VarType vtype = Evaluator::VarType(cmd.m_data[4]);
            uint8_t fine = cmd.m_data[5];
            m_auxBFxSel.addComponent(ctrl, (perc + fine / 100.f) / 100.f, combine, vtype);
            break;
        }
        case Op::SetupLFO:
        {
            uint8_t number = cmd.m_data[0];
            int16_t period = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            if (number == 0)
                vox.setLFO1Period(period / 1000.f);
            else if (number == 1)
                vox.setLFO2Period(period / 1000.f);
            break;
        }
        case Op::SetKeygroup:
        {
            uint8_t id = cmd.m_data[0];
            uint8_t flag = cmd.m_data[1];

            vox.setKeygroup(0);
            if (id)
            {
                vox.getEngine().killKeygroup(id, flag);
                vox.setKeygroup(id);
            }

            break;
        }
        case Op::AddVars:
        {
            bool aCtrl = cmd.m_data[0];
            int8_t a = cmd.m_data[1];
            bool bCtrl = cmd.m_data[2];
            int8_t b = cmd.m_data[3];
            bool cCtrl = cmd.m_data[4];
            int8_t c = cmd.m_data[5];

            if (bCtrl)
                b = vox.getCtrlValue(b);
            else
                b = m_variables[b];

            if (cCtrl)
                c = vox.getCtrlValue(c);
            else
                c = m_variables[c];

            if (aCtrl)
                vox.setCtrlValue(a, b + c);
            else
                m_variables[a] = b + c;

            break;
        }
        case Op::SubVars:
        {
            bool aCtrl = cmd.m_data[0];
            int8_t a = cmd.m_data[1];
            bool bCtrl = cmd.m_data[2];
            int8_t b = cmd.m_data[3];
            bool cCtrl = cmd.m_data[4];
            int8_t c = cmd.m_data[5];

            if (bCtrl)
                b = vox.getCtrlValue(b);
            else
                b = m_variables[b];

            if (cCtrl)
                c = vox.getCtrlValue(c);
            else
                c = m_variables[c];

            if (aCtrl)
                vox.setCtrlValue(a, b - c);
            else
                m_variables[a] = b - c;

            break;
        }
        case Op::MulVars:
        {
            bool aCtrl = cmd.m_data[0];
            int8_t a = cmd.m_data[1];
            bool bCtrl = cmd.m_data[2];
            int8_t b = cmd.m_data[3];
            bool cCtrl = cmd.m_data[4];
            int8_t c = cmd.m_data[5];

            if (bCtrl)
                b = vox.getCtrlValue(b);
            else
                b = m_variables[b];

            if (cCtrl)
                c = vox.getCtrlValue(c);
            else
                c = m_variables[c];

            if (aCtrl)
                vox.setCtrlValue(a, b * c);
            else
                m_variables[a] = b * c;

            break;
        }
        case Op::DivVars:
        {
            bool aCtrl = cmd.m_data[0];
            int8_t a = cmd.m_data[1];
            bool bCtrl = cmd.m_data[2];
            int8_t b = cmd.m_data[3];
            bool cCtrl = cmd.m_data[4];
            int8_t c = cmd.m_data[5];

            if (bCtrl)
                b = vox.getCtrlValue(b);
            else
                b = m_variables[b];

            if (cCtrl)
                c = vox.getCtrlValue(c);
            else
                c = m_variables[c];

            if (aCtrl)
                vox.setCtrlValue(a, b / c);
            else
                m_variables[a] = b / c;

            break;
        }
        case Op::AddIVars:
        {
            bool aCtrl = cmd.m_data[0];
            int8_t a = cmd.m_data[1];
            bool bCtrl = cmd.m_data[2];
            int8_t b = cmd.m_data[3];
            int16_t imm = *reinterpret_cast<int16_t*>(&cmd.m_data[4]);

            if (bCtrl)
                b = vox.getCtrlValue(b);
            else
                b = m_variables[b];

            if (aCtrl)
                vox.setCtrlValue(a, b + imm);
            else
                m_variables[a] = b + imm;

            break;
        }
        case Op::IfEqual:
        {
            bool aCtrl = cmd.m_data[0];
            int8_t a = cmd.m_data[1];
            bool bCtrl = cmd.m_data[2];
            int8_t b = cmd.m_data[3];
            bool lnot = cmd.m_data[4];
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            if (aCtrl)
                a = vox.getCtrlValue(a);
            else
                a = m_variables[a];

            if (bCtrl)
                b = vox.getCtrlValue(b);
            else
                b = m_variables[b];

            if ((a == b) ^ lnot)
                _setPC(macroStep);

            break;
        }
        case Op::IfLess:
        {
            bool aCtrl = cmd.m_data[0];
            int8_t a = cmd.m_data[1];
            bool bCtrl = cmd.m_data[2];
            int8_t b = cmd.m_data[3];
            bool lnot = cmd.m_data[4];
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[5]);

            if (aCtrl)
                a = vox.getCtrlValue(a);
            else
                a = m_variables[a];

            if (bCtrl)
                b = vox.getCtrlValue(b);
            else
                b = m_variables[b];

            if ((a < b) ^ lnot)
                _setPC(macroStep);

            break;
        }
        default:
            break;
        }
    }

    m_execTime += dt;
    return false;
}

void SoundMacroState::keyoffNotify(Voice& vox) { m_keyoff = true; }

void SoundMacroState::sampleEndNotify(Voice& vox) { m_sampleEnd = true; }
}

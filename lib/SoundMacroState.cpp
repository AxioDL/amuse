#include "amuse/SoundMacroState.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Engine.hpp"
#include "amuse/Common.hpp"
#include <string.h>

namespace amuse
{

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

void SoundMacroState::initialize(const unsigned char* ptr)
{
    m_ptr = ptr;
    m_ticksPerSec = 1000.f;
    m_midiKey = 0;
    m_midiVel = 0;
    m_midiMod = 0;
    m_random.seed();
    m_pc.clear();
    m_pc.push_back(-1);
    m_execTime = 0.f;
    m_keyoff = false;
    m_sampleEnd = false;
    m_loopCountdown = -1;
    m_lastPlayMacroVid = -1;
    memcpy(&m_header, ptr, sizeof(Header));
    m_header.swapBig();
}

void SoundMacroState::initialize(const unsigned char* ptr, float ticksPerSec,
                                 uint8_t midiKey, uint8_t midiVel, uint8_t midiMod)
{
    m_ptr = ptr;
    m_ticksPerSec = ticksPerSec;
    m_midiKey = midiKey;
    m_midiVel = midiVel;
    m_midiMod = midiMod;
    m_random.seed();
    m_pc.clear();
    m_pc.push_back(-1);
    m_execTime = 0.f;
    m_keyoff = false;
    m_sampleEnd = false;
    m_loopCountdown = -1;
    m_lastPlayMacroVid = -1;
    memcpy(&m_header, ptr, sizeof(Header));
    m_header.swapBig();
}

bool SoundMacroState::advance(Voice& vox, float dt)
{
    /* Nothing if uninitialized or finished */
    if (m_pc.back() == -1)
        return true;

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
            int16_t macroId = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            if (m_midiKey >= keyNumber)
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
            int16_t macroId = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            if (m_midiVel >= velocity)
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
            int16_t macroId = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
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
            int16_t macroId = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);
            int8_t priority = cmd.m_data[5];
            int8_t maxVoices = cmd.m_data[6];

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
                    otherVox->keyOff();
                }
            }
            else
            {
                Voice* otherVox = vox.getEngine().findVoice(m_variables[vid]);
                otherVox->keyOff();
            }

            break;
        }
        case Op::SplitMod:
        {
            uint8_t mod = cmd.m_data[0];
            int16_t macroId = *reinterpret_cast<int16_t*>(&cmd.m_data[1]);
            int16_t macroStep = *reinterpret_cast<int16_t*>(&cmd.m_data[3]);

            if (m_midiMod >= mod)
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
        case Op::SetAdsr:
        case Op::ScaleVolume:
        case Op::Panning:
        case Op::Envelope:
        case Op::StartSample:
        case Op::StopSample:
        case Op::KeyOff:
        case Op::SplitRnd:
        case Op::FadeIn:
        case Op::Spanning:
        case Op::SetAdsrCtrl:
        case Op::RndNote:
        case Op::AddNote:
        case Op::SetNote:
        case Op::LastNote:
        case Op::Portamento:
        case Op::Vibrato:
        case Op::PitchSweep1:
        case Op::PitchSweep2:
        case Op::SetPitch:
        case Op::SetPitchAdsr:
        case Op::ScaleVolumeDLS:
        case Op::Mod2Vibrange:
        case Op::SetupTremolo:
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
        case Op::VolSelect:
        case Op::PanSelect:
        case Op::PitchWheelSelect:
        case Op::ModWheelSelect:
        case Op::PedalSelect:
        case Op::PortaSelect:
        case Op::ReverbSelect:
        case Op::SpanSelect:
        case Op::DopplerSelect:
        case Op::TremoloSelect:
        case Op::PreASelect:
        case Op::PreBSelect:
        case Op::PostBSelect:
        case Op::AuxAFXSelect:
        case Op::AuxBFXSelect:
        case Op::SetupLFO:
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

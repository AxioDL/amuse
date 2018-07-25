#include "amuse/SoundMacroState.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Engine.hpp"
#include "amuse/Common.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupPool.hpp"
#include <cstring>

using namespace std::literals;

/* C++17 will error out if an offsetof cannot be computed, so ignore this warning */
#pragma GCC diagnostic ignored "-Winvalid-offsetof"

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
            thisValue = st.m_variables[comp.m_midiCtrl & 0x1f];

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
    m_portamentoMode = SoundMacro::CmdPortamento::PortState::MIDIControlled;
    m_portamentoTime = 0.5f;
}

template <class T, std::enable_if_t<!std::is_enum_v<T>, int> = 0>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType()
{ return SoundMacro::CmdIntrospection::Field::Type::Invalid; }
template <class T, std::enable_if_t<std::is_enum_v<T>, int> = 0>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType()
{ static_assert(sizeof(T) == 1, "Enum must be an 8-bit type");
  return SoundMacro::CmdIntrospection::Field::Type::Choice; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<bool>()
{ return SoundMacro::CmdIntrospection::Field::Type::Bool; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<atInt8>()
{ return SoundMacro::CmdIntrospection::Field::Type::Int8; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<atUint8>()
{ return SoundMacro::CmdIntrospection::Field::Type::UInt8; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<atInt16>()
{ return SoundMacro::CmdIntrospection::Field::Type::Int16; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<atUint16>()
{ return SoundMacro::CmdIntrospection::Field::Type::UInt16; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<atInt32>()
{ return SoundMacro::CmdIntrospection::Field::Type::Int32; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<atUint32>()
{ return SoundMacro::CmdIntrospection::Field::Type::UInt32; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<SoundMacroIdDNA<athena::Little>>()
{ return SoundMacro::CmdIntrospection::Field::Type::SoundMacroId; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<TableIdDNA<athena::Little>>()
{ return SoundMacro::CmdIntrospection::Field::Type::TableId; }
template <>
constexpr SoundMacro::CmdIntrospection::Field::Type GetFieldType<SampleIdDNA<athena::Little>>()
{ return SoundMacro::CmdIntrospection::Field::Type::SampleId; }

#define FIELD_HEAD(tp, var) GetFieldType<decltype(var)>(), offsetof(tp, var)

const SoundMacro::CmdIntrospection SoundMacro::CmdEnd::Introspective =
{
    CmdType::Structure,
    "End"sv,
    "End of the macro. This always appears at the end of a given SoundMacro."sv,
};
bool SoundMacro::CmdEnd::Do(SoundMacroState& st, Voice& vox) const
{
    st._setPC(-1);
    return true;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdStop::Introspective =
{
    CmdType::Structure,
     "Stop"sv,
    "Stops the macro at any point."sv,
};
bool SoundMacro::CmdStop::Do(SoundMacroState& st, Voice& vox) const
{
    st._setPC(-1);
    return true;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSplitKey::Introspective =
{
    CmdType::Structure,
    "Split Key"sv,
    "Conditionally branches macro execution based on MIDI key."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSplitKey, key),
            "Key"sv,
            0, 127, 60
        },
        {
            FIELD_HEAD(SoundMacro::CmdSplitKey, macro),
            "SoundMacro"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSplitKey, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        }
    }
};
bool SoundMacro::CmdSplitKey::Do(SoundMacroState& st, Voice& vox) const
{
    if (st.m_initKey >= key)
    {
        /* Do Branch */
        if (macro.id == std::get<0>(st.m_pc.back()))
            st._setPC(macroStep);
        else
            vox.loadMacroObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);
    }

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSplitVel::Introspective =
{
    CmdType::Structure,
    "Split Velocity"sv,
    "Conditionally branches macro execution based on velocity."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSplitVel, velocity),
            "Key"sv,
            0, 127, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdSplitVel, macro),
            "SoundMacro"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSplitVel, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        }
    }
};
bool SoundMacro::CmdSplitVel::Do(SoundMacroState& st, Voice& vox) const
{
    if (st.m_curVel >= velocity)
    {
        /* Do Branch */
        if (macro.id == std::get<0>(st.m_pc.back()))
            st._setPC(macroStep);
        else
            vox.loadMacroObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);
    }

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdWaitTicks::Introspective =
{
    CmdType::Structure,
    "Wait Ticks"sv,
    "Suspend SoundMacro execution for specified length of time. Value of 65535 "
    "will wait indefinitely, relying on Key Off or Sample End to signal stop."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdWaitTicks, keyOff),
            "Key Off"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdWaitTicks, random),
            "Random"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdWaitTicks, sampleEnd),
            "Sample End"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdWaitTicks, absolute),
            "Absolute"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdWaitTicks, msSwitch),
            "Use Millisec"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdWaitTicks, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 96
        }
    }
};
bool SoundMacro::CmdWaitTicks::Do(SoundMacroState& st, Voice& vox) const
{
    /* Set wait state */
    if (ticksOrMs >= 0)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksOrMs / q;
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

const SoundMacro::CmdIntrospection SoundMacro::CmdLoop::Introspective =
{
    CmdType::Structure,
    "Loop"sv,
    "Branch to specified location in a loop for a specified number of Times. "
    "65535 will cause an endless loop, relying on Key Off or Sample End to signal stop."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdLoop, keyOff),
            "Key Off"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdLoop, random),
            "Random"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdLoop, sampleEnd),
            "Sample End"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdLoop, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdLoop, times),
            "Times"sv,
            0, 65535, 0
        }
    }
};
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

const SoundMacro::CmdIntrospection SoundMacro::CmdGoto::Introspective =
{
    CmdType::Structure,
    "Goto"sv,
    "Unconditional branch to specified SoundMacro location."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdGoto, macro),
            "SoundMacro"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdGoto, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        }
    }
};
bool SoundMacro::CmdGoto::Do(SoundMacroState& st, Voice& vox) const
{
    /* Do Branch */
    if (macro.id == std::get<0>(st.m_pc.back()))
        st._setPC(macroStep);
    else
        vox.loadMacroObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdWaitMs::Introspective =
{
    CmdType::Structure,
    "Wait Millisec"sv,
    "Suspend SoundMacro execution for specified length of time. Value of 65535 "
    "will wait indefinitely, relying on Key Off or Sample End to signal stop."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdWaitMs, keyOff),
            "Key Off"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdWaitMs, random),
            "Random"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdWaitMs, sampleEnd),
            "Sample End"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdWaitMs, absolute),
            "Absolute"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdWaitMs, ms),
            "Ticks/Millisec"sv,
            0, 65535, 96
        }
    }
};
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

const SoundMacro::CmdIntrospection SoundMacro::CmdPlayMacro::Introspective =
{
    CmdType::Structure,
    "Play Macro"sv,
    "Play a SoundMacro in parallel to this one. Add Note is added to the "
    "current SoundMacro note to evaluate the new note."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPlayMacro, addNote),
            "Add Note"sv,
            -128, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPlayMacro, macro),
            "SoundMacro"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPlayMacro, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPlayMacro, priority),
            "Priority"sv,
            0, 127, 50
        },
        {
            FIELD_HEAD(SoundMacro::CmdPlayMacro, maxVoices),
            "Max Voices"sv,
            0, 255, 255
        }
    }
};
bool SoundMacro::CmdPlayMacro::Do(SoundMacroState& st, Voice& vox) const
{
    std::shared_ptr<Voice> sibVox = vox.startChildMacro(addNote, macro.id, macroStep);
    if (sibVox)
        st.m_lastPlayMacroVid = sibVox->vid();

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSendKeyOff::Introspective =
{
    CmdType::Structure,
    "Send Key Off"sv,
    "Send Key Off to voice specified by VID stored in a variable or the last started voice."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSendKeyOff, variable),
            "Variable"sv,
            0, 31, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSendKeyOff, lastStarted),
            "Last Started"sv,
            0, 1, 0
        }
    }
};
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
        std::shared_ptr<Voice> otherVox = vox.getEngine().findVoice(st.m_variables[variable & 0x1f]);
        if (otherVox)
            otherVox->keyOff();
    }

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSplitMod::Introspective =
{
    CmdType::Structure,
    "Split Mod"sv,
    "Conditionally branch if mod wheel is greater than or equal to specified value."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSplitMod, modValue),
            "Mod Value"sv,
            0, 127, 64
        },
        {
            FIELD_HEAD(SoundMacro::CmdSplitMod, macro),
            "SoundMacro"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSplitMod, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        }
    }
};
bool SoundMacro::CmdSplitMod::Do(SoundMacroState& st, Voice& vox) const
{
    if (st.m_curMod >= modValue)
    {
        /* Do Branch */
        if (macro.id == std::get<0>(st.m_pc.back()))
            st._setPC(macroStep);
        else
            vox.loadMacroObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);
    }

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPianoPan::Introspective =
{
    CmdType::Control,
    "Piano Pan"sv,
    "Gives piano-like sounds a natural-sounding stereo spread. The current key delta "
    "from Center Key is scaled with Scale and biased with Center Pan to evaluate panning."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPianoPan, scale),
            "Scale"sv,
            0, 127, 127
        },
        {
            FIELD_HEAD(SoundMacro::CmdPianoPan, centerKey),
            "Center Key"sv,
            0, 127, 36
        },
        {
            FIELD_HEAD(SoundMacro::CmdPianoPan, centerPan),
            "Center Pan"sv,
            0, 127, 64
        }
    }
};
bool SoundMacro::CmdPianoPan::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t pan = int32_t(st.m_initKey - centerKey) * scale / 127 + centerPan;
    pan = std::max(-127, std::min(127, pan));
    vox.setPan(pan / 127.f);

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSetAdsr::Introspective =
{
    CmdType::Volume,
    "Set ADSR"sv,
    "Specify ADSR envelope using a pool object. DLS mode must match setting in ADSR."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetAdsr, table),
            "ADSR"sv,
            0, 16383, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetAdsr, dlsMode),
            "DLS Mode"sv,
            0, 1, 0
        }
    }
};
bool SoundMacro::CmdSetAdsr::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setAdsr(table.id, dlsMode);
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdScaleVolume::Introspective =
{
    CmdType::Volume,
    "Scale Volume"sv,
    "Calculates volume by scaling and biasing velocity. "
    "The result may be passed through an optional Curve."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdScaleVolume, scale),
            "Scale"sv,
            0, 127, 127
        },
        {
            FIELD_HEAD(SoundMacro::CmdScaleVolume, add),
            "Add"sv,
            0, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdScaleVolume, table),
            "Curve"sv,
            0, 16383, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdScaleVolume, originalVol),
            "Original Vol"sv,
            0, 1, 1
        }
    }
};
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

const SoundMacro::CmdIntrospection SoundMacro::CmdPanning::Introspective =
{
    CmdType::Control,
    "Panning"sv,
    "Start pan-sweep from Pan Position offset by Width over specified time period."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPanning, panPosition),
            "Pan Position"sv,
            0, 127, 64
        },
        {
            FIELD_HEAD(SoundMacro::CmdPanning, timeMs),
            "Time Millisec"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPanning, width),
            "Width"sv,
            -128, 127, 0
        }
    }
};
bool SoundMacro::CmdPanning::Do(SoundMacroState& st, Voice& vox) const
{
    vox.startPanning(timeMs / 1000.0, panPosition, width);
    return false;
}


const SoundMacro::CmdIntrospection SoundMacro::CmdEnvelope::Introspective =
{
    CmdType::Volume,
    "Envelope"sv,
    "Start a velocity envelope by fading the current velocity to the one "
    "evaluated by Scale and Add. The result is optionally transformed with a Curve object."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdEnvelope, scale),
            "Scale"sv,
            0, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdEnvelope, add),
            "Add"sv,
            0, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdEnvelope, table),
            "Curve"sv,
            0, 16383, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdEnvelope, msSwitch),
            "Use Millisec"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdEnvelope, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 500
        }
    }
};
bool SoundMacro::CmdEnvelope::Do(SoundMacroState& st, Voice& vox) const
{
    double q = msSwitch ? 1000.0 : st.m_ticksPerSec;
    double secTime = ticksOrMs / q;

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

const SoundMacro::CmdIntrospection SoundMacro::CmdStartSample::Introspective =
{
    CmdType::Sample,
    "Start Sample"sv,
    "Start a Sample playing on the voice. An Offset in samples may be applied. "
    "This offset may be scaled with the current velocity."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdStartSample, sample),
            "Sample"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdStartSample, mode),
            "Velocity Scale"sv,
            0, 2, 0,
            {
                "No Scale"sv,
                "Negative"sv,
                "Positive"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdStartSample, offset),
            "Offset"sv,
            0, 0xffffff, 0
        }
    }
};
bool SoundMacro::CmdStartSample::Do(SoundMacroState& st, Voice& vox) const
{
    uint32_t useOffset = offset;

    switch (mode)
    {
    case Mode::Negative:
        useOffset = offset * (127 - st.m_curVel) / 127;
        break;
    case Mode::Positive:
        useOffset = offset * st.m_curVel / 127;
        break;
    default:
        break;
    }

    vox.startSample(sample.id, useOffset);
    vox.setPitchKey(st.m_curPitch);

    return false;
}


const SoundMacro::CmdIntrospection SoundMacro::CmdStopSample::Introspective =
{
    CmdType::Sample,
    "Stop Sample"sv,
    "Stops the sample playing on the voice."sv
};
bool SoundMacro::CmdStopSample::Do(SoundMacroState& st, Voice& vox) const
{
    vox.stopSample();
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdKeyOff::Introspective =
{
    CmdType::Control,
    "Key Off"sv,
    "Sends a Key Off to the current voice."sv
};
bool SoundMacro::CmdKeyOff::Do(SoundMacroState& st, Voice& vox) const
{
    vox._macroKeyOff();
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSplitRnd::Introspective =
{
    CmdType::Structure,
    "Split Rnd"sv,
    "Conditionally branch if a random value is greater than or equal to RND. "
    "A lower RND will cause a higher probability of branching."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSplitRnd, rnd),
            "RND"sv,
            0, 255, 128
        },
        {
            FIELD_HEAD(SoundMacro::CmdSplitRnd, macro),
            "SoundMacro"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSplitRnd, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        },
    }
};
bool SoundMacro::CmdSplitRnd::Do(SoundMacroState& st, Voice& vox) const
{
    if (rnd <= vox.getEngine().nextRandom() % 256)
    {
        /* Do branch */
        if (macro.id == std::get<0>(st.m_pc.back()))
            st._setPC(macroStep);
        else
            vox.loadMacroObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod);
    }

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdFadeIn::Introspective =
{
    CmdType::Volume,
    "Fade In"sv,
    "Start a velocity envelope by fading from silence to the velocity "
    "evaluated by Scale and Add. The result is optionally transformed with a Curve object."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdFadeIn, scale),
            "Scale"sv,
            0, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdFadeIn, add),
            "Add"sv,
            0, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdFadeIn, table),
            "Curve"sv,
            0, 16383, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdFadeIn, msSwitch),
            "Use Millisec"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdFadeIn, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 500
        }
    }
};
bool SoundMacro::CmdFadeIn::Do(SoundMacroState& st, Voice& vox) const
{
    float q = msSwitch ? 1000.f : st.m_ticksPerSec;
    float secTime = ticksOrMs / q;

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

const SoundMacro::CmdIntrospection SoundMacro::CmdSpanning::Introspective =
{
    CmdType::Control,
    "Spanning"sv,
    "Start span-sweep from Span Position offset by Width over specified time period."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSpanning, spanPosition),
            "Span Position"sv,
            0, 127, 64
        },
        {
            FIELD_HEAD(SoundMacro::CmdSpanning, timeMs),
            "Time Millisec"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSpanning, width),
            "Width"sv,
            -128, 127, 0
        }
    }
};
bool SoundMacro::CmdSpanning::Do(SoundMacroState& st, Voice& vox) const
{
    vox.startSpanning(timeMs / 1000.0, spanPosition, width);
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSetAdsrCtrl::Introspective =
{
    CmdType::Volume,
    "Set ADSR Ctrl"sv,
    "Bind MIDI controls to ADSR parameters."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetAdsrCtrl, attack),
            "Attack Ctrl"sv,
            0, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetAdsrCtrl, decay),
            "Decay Ctrl"sv,
            0, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetAdsrCtrl, sustain),
            "Sustain Ctrl"sv,
            0, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetAdsrCtrl, release),
            "Release Ctrl"sv,
            0, 127, 0
        },
    }
};
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

const SoundMacro::CmdIntrospection SoundMacro::CmdRndNote::Introspective =
{
    CmdType::Pitch,
    "Random Note"sv,
    "Sets random pitch between Note Lo and Note Hi, biased by Detune in cents. "
    "If Free is set, the note will not snap to key steps."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdRndNote, noteLo),
            "Note Lo"sv,
            -127, 127, 48
        },
        {
            FIELD_HEAD(SoundMacro::CmdRndNote, detune),
            "Detune"sv,
            0, 99, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdRndNote, noteHi),
            "Note Hi"sv,
            -127, 127, 72
        },
        {
            FIELD_HEAD(SoundMacro::CmdRndNote, fixedFree),
            "Free"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdRndNote, absRel),
            "Absolute"sv,
            0, 1, 0
        }
    }
};
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


const SoundMacro::CmdIntrospection SoundMacro::CmdAddNote::Introspective =
{
    CmdType::Pitch,
    "Add Note"sv,
    "Sets new pitch by adding Add, biased by Detune in cents. "
    "The time parameters behave like a WAIT command when non-zero."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdAddNote, add),
            "Add"sv,
            -128, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddNote, detune),
            "Detune"sv,
            -99, 99, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddNote, originalKey),
            "Original Key"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddNote, msSwitch),
            "Use Millisec"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddNote, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 0
        }
    }
};
bool SoundMacro::CmdAddNote::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_curPitch += add * 100 + detune;

    /* Set wait state */
    if (msSwitch)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksOrMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchKey(st.m_curPitch);

    return false;
}


const SoundMacro::CmdIntrospection SoundMacro::CmdSetNote::Introspective =
{
    CmdType::Pitch,
    "Set Note"sv,
    "Sets new pitch to Key, biased by Detune in cents. "
    "The time parameters behave like a WAIT command when non-zero."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetNote, key),
            "Key"sv,
            0, 127, 60
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetNote, detune),
            "Detune"sv,
            -99, 99, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetNote, msSwitch),
            "Use Millisec"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetNote, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 0
        }
    }
};
bool SoundMacro::CmdSetNote::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_curPitch = key * 100 + detune;

    /* Set wait state */
    if (ticksOrMs)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksOrMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchKey(st.m_curPitch);

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdLastNote::Introspective =
{
    CmdType::Pitch,
    "Last Note"sv,
    "Sets new pitch by adding Add to last played MIDI note, biased by Detune in cents. "
    "The time parameters behave like a WAIT command when non-zero."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdLastNote, add),
            "Key"sv,
            -128, 127, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdLastNote, detune),
            "Detune"sv,
            -99, 99, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdLastNote, msSwitch),
            "Use Millisec"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdLastNote, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 0
        }
    }
};
bool SoundMacro::CmdLastNote::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_curPitch = (add + vox.getLastNote()) * 100 + detune;

    /* Set wait state */
    if (msSwitch)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksOrMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchKey(st.m_curPitch);

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPortamento::Introspective =
{
    CmdType::Setup,
    "Portamento"sv,
    "Setup portamento mode for this voice."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPortamento, portState),
            "Port. State"sv,
            0, 2, 1,
            {
                "Enable"sv,
                "Disable"sv,
                "MIDI Controlled"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdPortamento, portType),
            "Port. Type"sv,
            0, 1, 0,
            {
                "Last Pressed"sv,
                "Always"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdPortamento, msSwitch),
            "Use Millisec"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdPortamento, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 0
        }
    }
};
bool SoundMacro::CmdPortamento::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_portamentoMode = portState;
    st.m_portamentoType = portType;
    float q = msSwitch ? 1000.f : st.m_ticksPerSec;
    st.m_portamentoTime = ticksOrMs / q;
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdVibrato::Introspective =
{
    CmdType::Pitch,
    "Vibrato"sv,
    "Setup vibrato mode for this voice. Voice pitch will be "
    "modulated using the Level magnitude or the modwheel."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdVibrato, levelNote),
            "Level Note"sv,
            -127, 127, 0,
        },
        {
            FIELD_HEAD(SoundMacro::CmdVibrato, levelFine),
            "Level Fine"sv,
            -99, 99, 15,
        },
        {
            FIELD_HEAD(SoundMacro::CmdVibrato, modwheelFlag),
            "Use Modwheel"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdVibrato, msSwitch),
            "Use Millisec"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdVibrato, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 250
        }
    }
};
bool SoundMacro::CmdVibrato::Do(SoundMacroState& st, Voice& vox) const
{
    float q = msSwitch ? 1000.f : st.m_ticksPerSec;
    vox.setVibrato(levelNote * 100 + levelFine, modwheelFlag, ticksOrMs / q);
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPitchSweep1::Introspective =
{
    CmdType::Pitch,
    "Pitch Sweep 1"sv,
    "Setup pitch sweep 1 for this voice. Voice pitch will accumulate Add for Times frames. "
    "If the time values are non-zero, this command also functions as a WAIT."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPitchSweep1, times),
            "Level Note"sv,
            0, 127, 100,
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchSweep1, add),
            "Level Fine"sv,
            -32768, 32767, 100,
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchSweep1, msSwitch),
            "Use Millisec"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchSweep1, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 1000
        }
    }
};
bool SoundMacro::CmdPitchSweep1::Do(SoundMacroState& st, Voice& vox) const
{
    /* Set wait state */
    if (msSwitch)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksOrMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchSweep1(times, add);

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPitchSweep2::Introspective =
{
    CmdType::Pitch,
    "Pitch Sweep 2"sv,
    "Setup pitch sweep 2 for this voice. Voice pitch will accumulate Add for Times frames. "
    "If the time values are non-zero, this command also functions as a WAIT."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPitchSweep2, times),
            "Level Note"sv,
            0, 127, 100,
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchSweep2, add),
            "Level Fine"sv,
            -32768, 32767, 100,
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchSweep2, msSwitch),
            "Use Millisec"sv,
            0, 1, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchSweep2, ticksOrMs),
            "Ticks/Millisec"sv,
            0, 65535, 1000
        }
    }
};
bool SoundMacro::CmdPitchSweep2::Do(SoundMacroState& st, Voice& vox) const
{
    /* Set wait state */
    if (msSwitch)
    {
        float q = msSwitch ? 1000.f : st.m_ticksPerSec;
        float secTime = ticksOrMs / q;
        st.m_waitCountdown = secTime;
        st.m_inWait = true;
    }

    vox.setPitchSweep2(times, add);

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSetPitch::Introspective =
{
    CmdType::Pitch,
    "Set Pitch"sv,
    "Set the playback sample rate directly."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetPitch, hz.val),
            "Hz"sv,
            0, 0xffffff, 22050,
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetPitch, fine),
            "Level Fine"sv,
            0, 65535, 0,
        }
    }
};
bool SoundMacro::CmdSetPitch::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setPitchFrequency(hz, fine);
    return false;
}


const SoundMacro::CmdIntrospection SoundMacro::CmdSetPitchAdsr::Introspective =
{
    CmdType::Pitch,
    "Set Pitch ADSR"sv,
    "Define the pitch ADSR from a pool object. The pitch range is "
    "specified using Note and Cents parameters."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetPitchAdsr, table),
            "ADSR"sv,
            0, 16383, 0,
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetPitchAdsr, keys),
            "Note range"sv,
            -128, 127, 0,
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetPitchAdsr, cents),
            "Cents range"sv,
            -99, 99, 0,
        }
    }
};
bool SoundMacro::CmdSetPitchAdsr::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setPitchAdsr(table.id, keys * 100 + cents);
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdScaleVolumeDLS::Introspective =
{
    CmdType::Volume,
    "Scale Volume DLS"sv,
    "Sets new volume by scaling the velocity. A value of 4096 == 100%."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdScaleVolumeDLS, scale),
            "Scale"sv,
            0, 16383, 4096,
        },
        {
            FIELD_HEAD(SoundMacro::CmdScaleVolumeDLS, originalVol),
            "Original Vol"sv,
            0, 1, 1,
        }
    }
};
bool SoundMacro::CmdScaleVolumeDLS::Do(SoundMacroState& st, Voice& vox) const
{
    vox.m_curVol = int32_t(originalVol ? st.m_initVel : st.m_curVel) * scale / 4096.f / 127.f;
    return false;
}


const SoundMacro::CmdIntrospection SoundMacro::CmdMod2Vibrange::Introspective =
{
    CmdType::Pitch,
    "Mod 2 Vibrange"sv,
    "Values used to scale the modwheel control for vibrato."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdMod2Vibrange, keys),
            "Key range"sv,
            0, 16383, 4096,
        },
        {
            FIELD_HEAD(SoundMacro::CmdMod2Vibrange, cents),
            "Cent range"sv,
            0, 1, 1,
        }
    }
};
bool SoundMacro::CmdMod2Vibrange::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setMod2VibratoRange(keys * 100 + cents);
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSetupTremolo::Introspective =
{
    CmdType::Special,
    "Setup Tremolo"sv,
    "Setup tremolo effect. Must be combined with Tremolo Select to connect "
    "with a configured LFO. A value of 4096 == 100%."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetupTremolo, scale),
            "Scale"sv,
            0, 16383, 8192,
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetupTremolo, modwAddScale),
            "Modw. add scale"sv,
            0, 16383, 0,
        }
    }
};
bool SoundMacro::CmdSetupTremolo::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setTremolo(scale / 4096.f, scale / 4096.f);
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdReturn::Introspective =
{
    CmdType::Structure,
    "Return"sv,
    "Branch to after last Go Subroutine command and pop call stack."sv
};
bool SoundMacro::CmdReturn::Do(SoundMacroState& st, Voice& vox) const
{
    if (st.m_pc.size() > 1)
    {
        st.m_pc.pop_back();
        vox._setObjectId(std::get<0>(st.m_pc.back()));
    }
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdGoSub::Introspective =
{
    CmdType::Structure,
    "Go Subroutine"sv,
    "Push location onto call stack and branch to specified location."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSplitRnd, macro),
            "SoundMacro"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSplitRnd, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        },
    }
};
bool SoundMacro::CmdGoSub::Do(SoundMacroState& st, Voice& vox) const
{
    if (macro.id == std::get<0>(st.m_pc.back()))
        st.m_pc.emplace_back(std::get<0>(st.m_pc.back()), std::get<1>(st.m_pc.back()),
            std::get<1>(st.m_pc.back())->assertPC(macroStep));
    else
        vox.loadMacroObject(macro.id, macroStep, st.m_ticksPerSec, st.m_initKey, st.m_initVel, st.m_initMod, true);

    vox._setObjectId(std::get<0>(st.m_pc.back()));

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdTrapEvent::Introspective =
{
    CmdType::Structure,
    "Trap Event"sv,
    "Register event-based branch to a specified location."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdTrapEvent, event),
            "Event"sv,
            0, 2, 0,
            {
                "Key Off"sv,
                "Sample End"sv,
                "Message Recv"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdTrapEvent, macro),
            "SoundMacro"sv,
            0, 65535, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdTrapEvent, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        },
    }
};
bool SoundMacro::CmdTrapEvent::Do(SoundMacroState& st, Voice& vox) const
{
    switch (event)
    {
    case EventType::KeyOff:
        vox.m_keyoffTrap.macroId = macro.id;
        vox.m_keyoffTrap.macroStep = macroStep;
        break;
    case EventType::SampleEnd:
        vox.m_sampleEndTrap.macroId = macro.id;
        vox.m_sampleEndTrap.macroStep = macroStep;
        break;
    case EventType::MessageRecv:
        vox.m_messageTrap.macroId = macro.id;
        vox.m_messageTrap.macroStep = macroStep;
        break;
    default:
        break;
    }

    return false;
}


const SoundMacro::CmdIntrospection SoundMacro::CmdUntrapEvent::Introspective =
{
    CmdType::Structure,
    "Untrap Event"sv,
    "Unregister event-based branch."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdUntrapEvent, event),
            "Event"sv,
            0, 2, 0,
            {
                "Key Off"sv,
                "Sample End"sv,
                "Message Recv"sv
            }
        }
    }
};
bool SoundMacro::CmdUntrapEvent::Do(SoundMacroState& st, Voice& vox) const
{
    switch (event)
    {
    case CmdTrapEvent::EventType::KeyOff:
        vox.m_keyoffTrap.macroId = 0xffff;
        vox.m_keyoffTrap.macroStep = 0xffff;
        break;
    case CmdTrapEvent::EventType::SampleEnd:
        vox.m_sampleEndTrap.macroId = 0xffff;
        vox.m_sampleEndTrap.macroStep = 0xffff;
        break;
    case CmdTrapEvent::EventType::MessageRecv:
        vox.m_messageTrap.macroId = 0xffff;
        vox.m_messageTrap.macroStep = 0xffff;
        break;
    default:
        break;
    }

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSendMessage::Introspective =
{
    CmdType::Special,
    "Send Message"sv,
    "Send message to SoundMacro or Voice referenced in a variable. "
    "The message value is retrieved from a variable."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSendMessage, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSendMessage, macro),
            "SoundMacro"sv,
            1, 16383, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdSendMessage, voiceVar),
            "Voice Var"sv,
            0, 31, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSendMessage, valueVar),
            "Value Var"sv,
            0, 31, 0
        },
    }
};
bool SoundMacro::CmdSendMessage::Do(SoundMacroState& st, Voice& vox) const
{
    if (isVar)
    {
        std::shared_ptr<Voice> findVox = vox.getEngine().findVoice(st.m_variables[voiceVar & 0x1f]);
        if (findVox)
            findVox->message(st.m_variables[valueVar & 0x1f]);
    }
    else
        vox.getEngine().sendMacroMessage(macro.id, st.m_variables[valueVar & 0x1f]);

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdGetMessage::Introspective =
{
    CmdType::Special,
    "Get Message"sv,
    "Get voice's latest received message and store its value in Variable."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdGetMessage, variable),
            "Variable"sv,
            0, 31, 0
        }
    }
};
bool SoundMacro::CmdGetMessage::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_variables[variable & 0x1f] = vox.m_latestMessage;
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdGetVid::Introspective =
{
    CmdType::Special,
    "Get VID"sv,
    "Get ID of current voice or last voice started by Play Macro command and store in Variable."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdGetVid, variable),
            "Variable"sv,
            0, 31, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdGetVid, playMacro),
            "Play Macro"sv,
            0, 1, 1
        }
    }
};
bool SoundMacro::CmdGetVid::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_variables[variable & 0x1f] = playMacro ? st.m_lastPlayMacroVid : vox.vid();
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdAddAgeCount::Introspective =
{
    CmdType::Special,
    "Add Age Count"sv,
    "Adds a value to the current voice's age counter."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdAddAgeCount, add),
            "Add"sv,
            -32768, 32767, -30000
        }
    }
};
bool SoundMacro::CmdAddAgeCount::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}


const SoundMacro::CmdIntrospection SoundMacro::CmdSetAgeCount::Introspective =
{
    CmdType::Special,
    "Set Age Count"sv,
    "Set a value into the current voice's age counter."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetAgeCount, counter),
            "Counter"sv,
            0, 65535, 0
        }
    }
};
bool SoundMacro::CmdSetAgeCount::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSendFlag::Introspective =
{
    CmdType::Special,
    "Send Flag"sv,
    "Send a flag value to the host application."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSendFlag, flagId),
            "Flag ID"sv,
            0, 15, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSendFlag, value),
            "Value"sv,
            0, 255, 255
        }
    }
};
bool SoundMacro::CmdSendFlag::Do(SoundMacroState& st, Voice& vox) const
{
    /* TODO: figure out a good API */
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPitchWheelR::Introspective =
{
    CmdType::Setup,
    "Pitch Wheel Range"sv,
    "Specifies the number of note steps for the range of the pitch wheel."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPitchWheelR, rangeUp),
            "Range Up"sv,
            0, 127, 2
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchWheelR, rangeDown),
            "Range Down"sv,
            0, 127, 2
        }
    }
};
bool SoundMacro::CmdPitchWheelR::Do(SoundMacroState& st, Voice& vox) const
{
    vox.setPitchWheelRange(rangeUp, rangeDown);
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSetPriority::Introspective =
{
    CmdType::Special,
    "Set Priority"sv,
    "Sets the priority of the current voice."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetPriority, prio),
            "Priority"sv,
            0, 254, 50
        }
    }
};
bool SoundMacro::CmdSetPriority::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}


const SoundMacro::CmdIntrospection SoundMacro::CmdAddPriority::Introspective =
{
    CmdType::Special,
    "Add Priority"sv,
    "Adds to the priority of the current voice."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdAddPriority, prio),
            "Priority"sv,
            -255, 255, 1
        }
    }
};
bool SoundMacro::CmdAddPriority::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdAgeCntSpeed::Introspective =
{
    CmdType::Special,
    "Age Count Speed"sv,
    "Sets the speed the current voice's age counter is decremented."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdAgeCntSpeed, time),
            "Millisec"sv,
            0, 16777215, 1080000
        }
    }
};
bool SoundMacro::CmdAgeCntSpeed::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdAgeCntVel::Introspective =
{
    CmdType::Special,
    "Age Count Velocity"sv,
    "Sets the current voice's age counter by scaling the velocity."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdAgeCntVel, ageBase),
            "Base"sv,
            0, 65535, 60000
        },
        {
            FIELD_HEAD(SoundMacro::CmdAgeCntVel, ageScale),
            "Scale"sv,
            0, 65535, 127
        }
    }
};
bool SoundMacro::CmdAgeCntVel::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdVolSelect::Introspective =
{
    CmdType::Setup,
    "Volume Select"sv,
    "Appends an evaluator component for computing the voice's volume."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdVolSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 7
        },
        {
            FIELD_HEAD(SoundMacro::CmdVolSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdVolSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdVolSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdVolSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdVolSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_volumeSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                SoundMacroState::Evaluator::Combine(combine),
                                SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPanSelect::Introspective =
{
    CmdType::Setup,
    "Pan Select"sv,
    "Appends an evaluator component for computing the voice's pan."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPanSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 10
        },
        {
            FIELD_HEAD(SoundMacro::CmdPanSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdPanSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdPanSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPanSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdPanSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_panSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                             SoundMacroState::Evaluator::Combine(combine),
                             SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPitchWheelSelect::Introspective =
{
    CmdType::Setup,
    "Pitch Wheel Select"sv,
    "Appends an evaluator component for computing the voice's pitch wheel."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPitchWheelSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 128
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchWheelSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchWheelSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchWheelSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPitchWheelSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdPitchWheelSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_pitchWheelSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                    SoundMacroState::Evaluator::Combine(combine),
                                    SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdModWheelSelect::Introspective =
{
    CmdType::Setup,
    "Mod Wheel Select"sv,
    "Appends an evaluator component for computing the voice's mod wheel."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdModWheelSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdModWheelSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdModWheelSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdModWheelSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdModWheelSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdModWheelSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_modWheelSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                  SoundMacroState::Evaluator::Combine(combine),
                                  SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPedalSelect::Introspective =
{
    CmdType::Setup,
    "Pedal Select"sv,
    "Appends an evaluator component for computing the voice's pedal."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPedalSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdPedalSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdPedalSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdPedalSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPedalSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdPedalSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_pedalSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                               SoundMacroState::Evaluator::Combine(combine),
                               SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPortamentoSelect::Introspective =
{
    CmdType::Setup,
    "Portamento Select"sv,
    "Appends an evaluator component for computing the voice's portamento."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPortamentoSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdPortamentoSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdPortamentoSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdPortamentoSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPortamentoSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdPortamentoSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_portamentoSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                    SoundMacroState::Evaluator::Combine(combine),
                                    SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdReverbSelect::Introspective =
{
    CmdType::Setup,
    "Reverb Select"sv,
    "Appends an evaluator component for computing the voice's reverb."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdReverbSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdReverbSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdReverbSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdReverbSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdReverbSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdReverbSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_reverbSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                SoundMacroState::Evaluator::Combine(combine),
                                SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSpanSelect::Introspective =
{
    CmdType::Setup,
    "Span Select"sv,
    "Appends an evaluator component for computing the voice's span."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSpanSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 19
        },
        {
            FIELD_HEAD(SoundMacro::CmdSpanSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdSpanSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdSpanSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSpanSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdSpanSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_spanSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                              SoundMacroState::Evaluator::Combine(combine),
                              SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdDopplerSelect::Introspective =
{
    CmdType::Setup,
    "Doppler Select"sv,
    "Appends an evaluator component for computing the voice's doppler."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdDopplerSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 132
        },
        {
            FIELD_HEAD(SoundMacro::CmdDopplerSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdDopplerSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdDopplerSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdDopplerSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdDopplerSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_dopplerSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                 SoundMacroState::Evaluator::Combine(combine),
                                 SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdTremoloSelect::Introspective =
{
    CmdType::Setup,
    "Tremolo Select"sv,
    "Appends an evaluator component for computing the voice's tremolo."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdTremoloSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdTremoloSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdTremoloSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdTremoloSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdTremoloSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdTremoloSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_tremoloSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                 SoundMacroState::Evaluator::Combine(combine),
                                 SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPreASelect::Introspective =
{
    CmdType::Setup,
    "PreA Select"sv,
    "Appends an evaluator component for computing the voice's pre-AUXA."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPreASelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdPreASelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdPreASelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdPreASelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPreASelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdPreASelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_preAuxASel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                 SoundMacroState::Evaluator::Combine(combine),
                                 SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPreBSelect::Introspective =
{
    CmdType::Setup,
    "PreB Select"sv,
    "Appends an evaluator component for computing the voice's pre-AUXB."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPreBSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdPreBSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdPreBSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdPreBSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPreBSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdPreBSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_preAuxBSel.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                                 SoundMacroState::Evaluator::Combine(combine),
                                 SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdPostBSelect::Introspective =
{
    CmdType::Setup,
    "PostB Select"sv,
    "Appends an evaluator component for computing the voice's post-AUXB."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdPostBSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdPostBSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdPostBSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdPostBSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdPostBSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        }
    }
};
bool SoundMacro::CmdPostBSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_postAuxB.addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                               SoundMacroState::Evaluator::Combine(combine),
                               SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdAuxAFXSelect::Introspective =
{
    CmdType::Setup,
    "AuxA FX Select"sv,
    "Appends an evaluator component for computing the AUXA Parameter."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdAuxAFXSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxAFXSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxAFXSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxAFXSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxAFXSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxAFXSelect, paramIndex),
            "Param Index"sv,
            0, 2, 0
        }
    }
};
bool SoundMacro::CmdAuxAFXSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_auxAFxSel[std::min(paramIndex, atUint8(3))].
        addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                     SoundMacroState::Evaluator::Combine(combine),
                     SoundMacroState::Evaluator::VarType(isVar));
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdAuxBFXSelect::Introspective =
{
    CmdType::Setup,
    "AuxB FX Select"sv,
    "Appends an evaluator component for computing the AUXB Parameter."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdAuxBFXSelect, midiControl),
            "MIDI Control"sv,
            0, 132, 1
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxBFXSelect, scalingPercentage),
            "Scale Percentage"sv,
            -10000, 10000, 100
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxBFXSelect, combine),
            "Combine Mode"sv,
            0, 2, 0,
            {
                "Set"sv,
                "Add"sv,
                "Mult"sv
            }
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxBFXSelect, isVar),
            "Is Var"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxBFXSelect, fineScaling),
            "Fine Scaling"sv,
            -100, 100, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAuxBFXSelect, paramIndex),
            "Param Index"sv,
            0, 2, 0
        }
    }
};
bool SoundMacro::CmdAuxBFXSelect::Do(SoundMacroState& st, Voice& vox) const
{
    st.m_auxBFxSel[std::min(paramIndex, atUint8(3))].
        addComponent(midiControl, (scalingPercentage + fineScaling / 100.f) / 100.f,
                     SoundMacroState::Evaluator::Combine(combine),
                     SoundMacroState::Evaluator::VarType(isVar));
    return false;
}


const SoundMacro::CmdIntrospection SoundMacro::CmdSetupLFO::Introspective =
{
    CmdType::Setup,
    "Setup LFO"sv,
    "Configures voice's LFO period in milliseconds."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetupLFO, lfoNumber),
            "LFO Number"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetupLFO, periodInMs),
            "Period"sv,
            -10000, 10000, 100
        }
    }
};
bool SoundMacro::CmdSetupLFO::Do(SoundMacroState& st, Voice& vox) const
{
    if (lfoNumber == 0)
        vox.setLFO1Period(periodInMs / 1000.f);
    else if (lfoNumber == 1)
        vox.setLFO2Period(periodInMs / 1000.f);
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdModeSelect::Introspective =
{
    CmdType::Setup,
    "Mode Select"sv,
    "Sets operating modes for current voice."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdModeSelect, dlsVol),
            "DLS Vol"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdModeSelect, itd),
            "ITD"sv,
            0, 1, 0
        }
    }
};
bool SoundMacro::CmdModeSelect::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSetKeygroup::Introspective =
{
    CmdType::Setup,
    "Set Keygroup"sv,
    "Selects keygroup for current voice."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetKeygroup, group),
            "Group"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetKeygroup, killNow),
            "Kill now"sv,
            0, 1, 0
        }
    }
};
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

const SoundMacro::CmdIntrospection SoundMacro::CmdSRCmodeSelect::Introspective =
{
    CmdType::Setup,
    "SRC Mode Select"sv,
    "Sets operating modes for sample rate converter."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSRCmodeSelect, srcType),
            "SRC Type"sv,
            0, 2, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSRCmodeSelect, type0SrcFilter),
            "Type 0 SRC Filter"sv,
            0, 2, 1
        }
    }
};
bool SoundMacro::CmdSRCmodeSelect::Do(SoundMacroState& st, Voice& vox) const
{
    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdAddVars::Introspective =
{
    CmdType::Special,
    "Add Vars"sv,
    "A = B + C"sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdAddVars, varCtrlA),
            "Use Ctrl A"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddVars, a),
            "A"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddVars, varCtrlB),
            "Use Ctrl B"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddVars, b),
            "B"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddVars, varCtrlC),
            "Use Ctrl C"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddVars, c),
            "C"sv,
            0, 255, 0
        },
    }
};
bool SoundMacro::CmdAddVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB, useC;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b & 0x1f];

    if (varCtrlC)
        useC = vox.getCtrlValue(c);
    else
        useC = st.m_variables[c & 0x1f];

    if (varCtrlA)
        vox.setCtrlValue(a, useB + useC);
    else
        st.m_variables[a & 0x1f] = useB + useC;

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSubVars::Introspective =
{
    CmdType::Special,
    "Sub Vars"sv,
    "A = B - C"sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSubVars, varCtrlA),
            "Use Ctrl A"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSubVars, a),
            "A"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSubVars, varCtrlB),
            "Use Ctrl B"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSubVars, b),
            "B"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSubVars, varCtrlC),
            "Use Ctrl C"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSubVars, c),
            "C"sv,
            0, 255, 0
        },
    }
};
bool SoundMacro::CmdSubVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB, useC;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b & 0x1f];

    if (varCtrlC)
        useC = vox.getCtrlValue(c);
    else
        useC = st.m_variables[c & 0x1f];

    if (varCtrlA)
        vox.setCtrlValue(a, useB - useC);
    else
        st.m_variables[a & 0x1f] = useB - useC;

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdMulVars::Introspective =
{
    CmdType::Special,
    "Mul Vars"sv,
    "A = B * C"sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdMulVars, varCtrlA),
            "Use Ctrl A"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdMulVars, a),
            "A"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdMulVars, varCtrlB),
            "Use Ctrl B"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdMulVars, b),
            "B"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdMulVars, varCtrlC),
            "Use Ctrl C"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdMulVars, c),
            "C"sv,
            0, 255, 0
        },
    }
};
bool SoundMacro::CmdMulVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB, useC;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b & 0x1f];

    if (varCtrlC)
        useC = vox.getCtrlValue(c);
    else
        useC = st.m_variables[c & 0x1f];

    if (varCtrlA)
        vox.setCtrlValue(a, useB * useC);
    else
        st.m_variables[a & 0x1f] = useB * useC;

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdDivVars::Introspective =
{
    CmdType::Special,
    "Div Vars"sv,
    "A = B / C"sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdDivVars, varCtrlA),
            "Use Ctrl A"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdDivVars, a),
            "A"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdDivVars, varCtrlB),
            "Use Ctrl B"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdDivVars, b),
            "B"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdDivVars, varCtrlC),
            "Use Ctrl C"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdDivVars, c),
            "C"sv,
            0, 255, 0
        },
    }
};
bool SoundMacro::CmdDivVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB, useC;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b & 0x1f];

    if (varCtrlC)
        useC = vox.getCtrlValue(c);
    else
        useC = st.m_variables[c & 0x1f];

    if (varCtrlA)
        vox.setCtrlValue(a, useB / useC);
    else
        st.m_variables[a & 0x1f] = useB / useC;

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdAddIVars::Introspective =
{
    CmdType::Special,
    "Add Imm Vars"sv,
    "A = B + Immediate"sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdAddIVars, varCtrlA),
            "Use Ctrl A"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddIVars, a),
            "A"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddIVars, varCtrlB),
            "Use Ctrl B"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddIVars, b),
            "B"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdAddIVars, imm),
            "Immediate"sv,
            -32768, 32767, 0
        },
    }
};
bool SoundMacro::CmdAddIVars::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useB;

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b & 0x1f];

    if (varCtrlA)
        vox.setCtrlValue(a, useB + imm);
    else
        st.m_variables[a & 0x1f] = useB + imm;

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdSetVar::Introspective =
{
    CmdType::Special,
    "Set Var"sv,
    "A = Immediate"sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdSetVar, varCtrlA),
            "Use Ctrl A"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetVar, a),
            "A"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdSetVar, imm),
            "Immediate"sv,
            -32768, 32767, 0
        },
    }
};
bool SoundMacro::CmdSetVar::Do(SoundMacroState& st, Voice& vox) const
{
    if (varCtrlA)
        vox.setCtrlValue(a, imm);
    else
        st.m_variables[a & 0x1f] = imm;

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdIfEqual::Introspective =
{
    CmdType::Structure,
    "If Equal"sv,
    "Branches to specified step if A == B."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdIfEqual, varCtrlA),
            "Use Ctrl A"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfEqual, a),
            "A"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfEqual, varCtrlB),
            "Use Ctrl B"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfEqual, b),
            "B"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfEqual, notEq),
            "Not"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfEqual, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        },
    }
};
bool SoundMacro::CmdIfEqual::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useA, useB;

    if (varCtrlA)
        useA = vox.getCtrlValue(a);
    else
        useA = st.m_variables[a & 0x1f];

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b & 0x1f];

    if ((useA == useB) ^ notEq)
        st._setPC(macroStep);

    return false;
}

const SoundMacro::CmdIntrospection SoundMacro::CmdIfLess::Introspective =
{
    CmdType::Structure,
    "If Less"sv,
    "Branches to specified step if A < B."sv,
    {
        {
            FIELD_HEAD(SoundMacro::CmdIfLess, varCtrlA),
            "Use Ctrl A"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfLess, a),
            "A"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfLess, varCtrlB),
            "Use Ctrl B"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfLess, b),
            "B"sv,
            0, 255, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfLess, notLt),
            "Not"sv,
            0, 1, 0
        },
        {
            FIELD_HEAD(SoundMacro::CmdIfLess, macroStep),
            "SoundMacro Step"sv,
            0, 65535, 0
        },
    }
};
bool SoundMacro::CmdIfLess::Do(SoundMacroState& st, Voice& vox) const
{
    int32_t useA, useB;

    if (varCtrlA)
        useA = vox.getCtrlValue(a);
    else
        useA = st.m_variables[a & 0x1f];

    if (varCtrlB)
        useB = vox.getCtrlValue(b);
    else
        useB = st.m_variables[b & 0x1f];

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

#ifndef __AMUSE_SOUNDMACROSTATE_HPP__
#define __AMUSE_SOUNDMACROSTATE_HPP__

#include <stdint.h>
#include <vector>
#include <list>
#include "Entity.hpp"

namespace amuse
{
class Voice;

class SoundMacroState
{
    /** SoundMacro header */
    struct Header
    {
        uint32_t m_size;
        ObjectId m_macroId;
        uint8_t m_volume;
        uint8_t m_pan;
        void swapBig();
    } m_header;

    /** SoundMacro command operations */
    enum class Op : uint8_t
    {
        End,
        Stop,
        SplitKey,
        SplitVel,
        WaitTicks,
        Loop,
        Goto,
        WaitMs,
        PlayMacro,
        SendKeyOff,
        SplitMod,
        PianoPan,
        SetAdsr,
        ScaleVolume,
        Panning,
        Envelope,
        StartSample,
        StopSample,
        KeyOff,
        SplitRnd,
        FadeIn,
        Spanning,
        SetAdsrCtrl,
        RndNote,
        AddNote,
        SetNote,
        LastNote,
        Portamento,
        Vibrato,
        PitchSweep1,
        PitchSweep2,
        SetPitch,
        SetPitchAdsr,
        ScaleVolumeDLS,
        Mod2Vibrange,
        SetupTremolo,
        Return,
        GoSub,
        TrapEvent = 0x28,
        UntrapEvent,
        SendMessage,
        GetMessage,
        GetVid,
        AddAgeCount = 0x30,
        SetAgeCount,
        SendFlag,
        PitchWheelR,
        SetPriority,
        AddPriority,
        AgeCntSpeed,
        AgeCntVel,
        VolSelect = 0x40,
        PanSelect,
        PitchWheelSelect,
        ModWheelSelect,
        PedalSelect,
        PortamentoSelect,
        ReverbSelect,
        SpanSelect,
        DopplerSelect,
        TremoloSelect,
        PreASelect,
        PreBSelect,
        PostBSelect,
        AuxAFXSelect,
        AuxBFXSelect,
        SetupLFO = 0x50,
        ModeSelect = 0x58,
        SetKeygroup,
        SRCmodeSelect,
        AddVars = 0x60,
        SubVars,
        MulVars,
        DivVars,
        AddIVars,
        IfEqual = 0x70,
        IfLess,
    };

    /** SoundMacro command structure */
    struct Command
    {
        Op m_op;
        char m_data[7];
        void swapBig();
    };

    /** 'program counter' stack for the active SoundMacro */
    std::vector<std::pair<const unsigned char*, int>> m_pc;

    float m_curVol; /**< final volume sent to voice */
    bool m_volDirty; /**< set when voice needs updated volume */

    float m_curPan; /**< final pan sent to voice */
    bool m_panDirty; /**< set when voice needs updated pan */

    float m_curSpan; /**< final span sent to voice */
    bool m_spanDirty; /**< set when voice needs updated span */

    float m_ticksPerSec; /**< ratio for resolving ticks in commands that use them */
    uint8_t m_initVel; /**< Velocity played for this macro invocation */
    uint8_t m_initMod; /**< Modulation played for this macro invocation */
    uint8_t m_initKey; /**< Key played for this macro invocation */
    uint8_t m_curVel; /**< Current velocity played for this macro invocation */
    uint8_t m_curMod; /**< Current modulation played for this macro invocation */
    uint32_t m_curKey; /**< Current key played for this macro invocation (in cents) */
    uint32_t m_pitchSweep1; /**< Current value of PITCHSWEEP1 controller (in cents) */
    uint32_t m_pitchSweep2; /**< Current value of PITCHSWEEP2 controller (in cents) */
    int16_t m_pitchSweep1Add; /**< Value to add to PITCHSWEEP1 controller each cycle */
    int16_t m_pitchSweep2Add; /**< Value to add to PITCHSWEEP2 controller each cycle */
    uint8_t m_pitchSweep1Times; /**< Remaining times to advance PITCHSWEEP1 controller */
    uint8_t m_pitchSweep2Times; /**< Remaining times to advance PITCHSWEEP2 controller */
    bool m_pitchDirty; /**< set when voice needs latest pitch computation */

    float m_execTime; /**< time in seconds of SoundMacro execution */
    bool m_keyoff; /**< keyoff message has been received */
    bool m_sampleEnd; /**< sample has finished playback */

    float m_envelopeTime; /**< time since last ENVELOPE command, -1 for no active volume-sweep */
    float m_envelopeDur; /**< requested duration of last ENVELOPE command */
    uint8_t m_envelopeStart; /**< initial value for last ENVELOPE command */
    uint8_t m_envelopeEnd; /**< final value for last ENVELOPE command */
    const Curve* m_envelopeCurve; /**< curve to use for ENVELOPE command */

    float m_panningTime; /**< time since last PANNING command, -1 for no active pan-sweep */
    float m_panningDur; /**< requested duration of last PANNING command */
    uint8_t m_panPos; /**< initial pan value of last PANNING command */
    uint8_t m_panWidth; /**< delta pan value to target of last PANNING command */

    float m_spanningTime; /**< time since last SPANNING command, -1 for no active span-sweep */
    float m_spanningDur; /**< requested duration of last SPANNING command */
    uint8_t m_spanPos; /**< initial pan value of last SPANNING command */
    uint8_t m_spanWidth; /**< delta pan value to target of last SPANNING command */

    bool m_inWait = false; /**< set when timer/keyoff/sampleend wait active */
    bool m_keyoffWait = false; /**< set when active wait is a keyoff wait */
    bool m_sampleEndWait = false; /**< set when active wait is a sampleend wait */
    float m_waitCountdown; /**< countdown timer for active wait */

    int m_loopCountdown = -1; /**< countdown for current loop */
    int m_lastPlayMacroVid = -1; /**< VoiceId from last PlayMacro command */

    bool m_useAdsrControllers; /**< when set, use the following controllers for envelope times */
    uint8_t m_midiAttack; /**< Attack MIDI controller */
    uint8_t m_midiDecay; /**< Decay MIDI controller */
    uint8_t m_midiSustain; /**< Sustain MIDI controller */
    uint8_t m_midiRelease; /**< Release MIDI controller */

    uint8_t m_portamentoMode; /**< (0: Off, 1: On, 2: MIDI specified) */
    uint8_t m_portamentoType; /**< (0: New key pressed while old key pressed, 1: Always) */
    float m_portamentoTime; /**< portamento transition time, 0.f will perform legato */

    int32_t m_vibratoLevel; /**< scale of vibrato effect (in cents) */
    int32_t m_vibratoModLevel; /**< scale of vibrato mod-wheel influence (in cents) */
    float m_vibratoPeriod; /**< vibrato wave period-time, 0.f will disable vibrato */
    bool m_vibratoModWheel; /**< vibrato scaled with mod-wheel if set */

    float m_tremoloScale; /**< minimum volume factor produced via LFO */
    float m_tremoloModScale; /**< minimum volume factor produced via LFO, scaled via mod wheel */

    float m_lfoPeriods[2]; /**< time-periods for LFO1 and LFO2 */

    /** Used to build a multi-component formula for overriding controllers */
    struct Evaluator
    {
        enum class Combine : uint8_t
        {
            Set,
            Add,
            Mult
        };
        enum class VarType : uint8_t
        {
            Ctrl,
            Var
        };

        /** Represents one term of the formula assembled via *_SELECT commands */
        struct Component
        {
            uint8_t m_midiCtrl;
            float m_scale;
            Combine m_combine;
            VarType m_varType;

            Component(uint8_t midiCtrl, float scale, Combine combine, VarType varType)
            : m_midiCtrl(midiCtrl), m_scale(scale), m_combine(combine), m_varType(varType) {}
        };
        std::vector<Component> m_comps; /**< Components built up by the macro */

        /** Combine additional component(s) to formula */
        void addComponent(uint8_t midiCtrl, float scale,
                          Combine combine, VarType varType);

        /** Calculate value */
        float evaluate(Voice& vox, const SoundMacroState& st);

        /** Determine if able to use */
        operator bool() const {return m_comps.size() != 0;}
    };

    Evaluator m_volumeSel;
    Evaluator m_panSel;
    Evaluator m_pitchWheelSel;
    Evaluator m_modWheelSel;
    Evaluator m_pedalSel;
    Evaluator m_portamentoSel;
    Evaluator m_reverbSel;
    Evaluator m_preAuxASel;
    Evaluator m_preAuxBSel;
    Evaluator m_auxAFxSel;
    Evaluator m_auxBFxSel;
    Evaluator m_postAuxB;
    Evaluator m_spanSel;
    Evaluator m_dopplerSel;
    Evaluator m_tremoloSel;

    int32_t m_variables[256]; /**< 32-bit variables set with relevant commands */

    /** Messages pending processing for this SoundMacro voice */
    std::list<int32_t> m_messageQueue;

    /** Event registration data for TRAP_EVENT */
    struct EventTrap
    {
        ObjectId macroId;
        uint16_t macroStep;
    };
    EventTrap m_keyoffTrap;
    EventTrap m_sampleEndTrap;
    EventTrap m_messageTrap;

public:
    /** initialize state for SoundMacro data at `ptr` */
    void initialize(const unsigned char* ptr);
    void initialize(const unsigned char* ptr, float ticksPerSec,
                    uint8_t midiKey, uint8_t midiVel, uint8_t midiMod);

    /** advances `dt` seconds worth of commands in the SoundMacro
     *  @return `true` if END reached
     */
    bool advance(Voice& vox, float dt);

    /** keyoff event */
    void keyoffNotify(Voice& vox);

    /** sample end event */
    void sampleEndNotify(Voice& vox);

    /** SEND_MESSAGE receive event */
    void messageNotify(Voice& vox, int32_t val);
};

}

#endif // __AMUSE_SOUNDMACROSTATE_HPP__

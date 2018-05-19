#ifndef __AMUSE_SOUNDMACROSTATE_HPP__
#define __AMUSE_SOUNDMACROSTATE_HPP__

#include <cstdint>
#include <vector>
#include <list>
#include "Entity.hpp"
#include "Common.hpp"

/* Squelch Win32 macro pollution >.< */
#undef SendMessage
#undef GetMessage

namespace amuse
{
class Voice;

/** Real-time state of SoundMacro execution */
class SoundMacroState
{
    friend class Voice;
    friend class Envelope;

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
        AddAgeCount = 0x30, /* unimplemented */
        SetAgeCount,        /* unimplemented */
        SendFlag,           /* unimplemented */
        PitchWheelR,
        SetPriority = 0x36, /* unimplemented */
        AddPriority, /* unimplemented */
        AgeCntSpeed, /* unimplemented */
        AgeCntVel,   /* unimplemented */
        VolSelect = 0x40,
        PanSelect,
        PitchWheelSelect,
        ModWheelSelect,
        PedalSelect,
        PortamentoSelect,
        ReverbSelect, /* serves as PostASelect */
        SpanSelect,
        DopplerSelect,
        TremoloSelect,
        PreASelect,
        PreBSelect,
        PostBSelect,
        AuxAFXSelect, /* unimplemented */
        AuxBFXSelect, /* unimplemented */
        SetupLFO = 0x50,
        ModeSelect = 0x58,
        SetKeygroup,
        SRCmodeSelect, /* unimplemented */
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

    static int _assertPC(int pc, uint32_t size);
    static int _assertPC(int pc, uint32_t size, bool swapSize)
    {
        return _assertPC(pc, swapSize ? SBig(size) : size);
    }

    void _setPC(int pc)
    {
        m_pc.back().second = _assertPC(pc, m_header.m_size);
    }

    double m_ticksPerSec; /**< ratio for resolving ticks in commands that use them */
    uint8_t m_initVel;    /**< Velocity played for this macro invocation */
    uint8_t m_initMod;    /**< Modulation played for this macro invocation */
    uint8_t m_initKey;    /**< Key played for this macro invocation */
    uint8_t m_curVel;     /**< Current velocity played for this macro invocation */
    uint8_t m_curMod;     /**< Current modulation played for this macro invocation */
    uint32_t m_curPitch;  /**< Current key played for this macro invocation (in cents) */

    double m_execTime; /**< time in seconds of SoundMacro execution (per-update resolution) */
    bool m_keyoff;     /**< keyoff message has been received */
    bool m_sampleEnd;  /**< sample has finished playback */

    bool m_inWait = false;         /**< set when timer/keyoff/sampleend wait active */
    bool m_indefiniteWait = false; /**< set when timer wait is indefinite (keyoff/sampleend only) */
    bool m_keyoffWait = false;     /**< set when active wait is a keyoff wait */
    bool m_sampleEndWait = false;  /**< set when active wait is a sampleend wait */
    double m_waitCountdown;        /**< countdown timer for active wait */

    int m_loopCountdown = -1;    /**< countdown for current loop */
    int m_lastPlayMacroVid = -1; /**< VoiceId from last PlayMacro command */

    bool m_useAdsrControllers; /**< when set, use the following controllers for envelope times */
    uint8_t m_midiAttack;      /**< Attack MIDI controller */
    uint8_t m_midiDecay;       /**< Decay MIDI controller */
    uint8_t m_midiSustain;     /**< Sustain MIDI controller */
    uint8_t m_midiRelease;     /**< Release MIDI controller */

    uint8_t m_portamentoMode = 2;  /**< (0: Off, 1: On, 2: MIDI specified) */
    uint8_t m_portamentoType = 0;  /**< (0: New key pressed while old key pressed, 1: Always) */
    float m_portamentoTime = 0.5f; /**< portamento transition time, 0.f will perform legato */

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
            : m_midiCtrl(midiCtrl), m_scale(scale), m_combine(combine), m_varType(varType)
            {
            }
        };
        std::vector<Component> m_comps; /**< Components built up by the macro */

        /** Combine additional component(s) to formula */
        void addComponent(uint8_t midiCtrl, float scale, Combine combine, VarType varType);

        /** Calculate value */
        float evaluate(double time, const Voice& vox, const SoundMacroState& st) const;

        /** Determine if able to use */
        operator bool() const { return m_comps.size() != 0; }
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

    /** Event registration data for TRAP_EVENT */
    struct EventTrap
    {
        ObjectId macroId = 0xffff;
        uint16_t macroStep;
    };

public:
    /** initialize state for SoundMacro data at `ptr` */
    void initialize(const unsigned char* ptr, int step, bool swapData);
    void initialize(const unsigned char* ptr, int step, double ticksPerSec, uint8_t midiKey, uint8_t midiVel,
                    uint8_t midiMod, bool swapData);

    /** advances `dt` seconds worth of commands in the SoundMacro
     *  @return `true` if END reached
     */
    bool advance(Voice& vox, double dt);

    /** keyoff event */
    void keyoffNotify(Voice& vox);

    /** sample end event */
    void sampleEndNotify(Voice& vox);
};
}

#endif // __AMUSE_SOUNDMACROSTATE_HPP__

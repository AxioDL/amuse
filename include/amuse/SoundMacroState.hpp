#ifndef __AMUSE_SOUNDMACROSTATE_HPP__
#define __AMUSE_SOUNDMACROSTATE_HPP__

#include <stdint.h>
#include <random>

namespace amuse
{
class Voice;

class SoundMacroState
{
    /** SoundMacro header */
    struct Header
    {
        uint32_t m_size;
        uint16_t m_macroId;
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
        PortaSelect,
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

    const unsigned char* m_ptr = nullptr; /**< pointer to selected SoundMacro data */
    std::vector<int> m_pc; /**< 'program counter' stack for the active SoundMacro */

    float m_ticksPerSec; /**< ratio for resolving ticks in commands that use them */
    uint8_t m_midiKey; /**< Key played for this macro invocation */
    uint8_t m_midiVel; /**< Velocity played for this macro invocation */
    uint8_t m_midiMod; /**< Modulation played for this macro invocation */
    std::linear_congruential_engine<uint32_t, 0x41c64e6d, 0x3039, UINT32_MAX> m_random;

    float m_execTime; /**< time in seconds of SoundMacro execution */
    bool m_keyoff; /**< keyoff message has been received */
    bool m_sampleEnd; /**< sample has finished playback */

    bool m_inWait = false; /**< set when timer/keyoff/sampleend wait active */
    bool m_keyoffWait = false; /**< set when active wait is a keyoff wait */
    bool m_sampleEndWait = false; /**< set when active wait is a sampleend wait */
    float m_waitCountdown; /**< countdown timer for active wait */

    int m_loopCountdown = -1; /**< countdown for current loop */
    int m_lastPlayMacroVid = -1; /**< VoiceId from last PlayMacro command */

    int32_t m_variables[256]; /**< 32-bit variables set with relevant commands */

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
    void keyoff();

    /** sample end event */
    void sampleEnd();
};

}

#endif // __AMUSE_SOUNDMACROSTATE_HPP__

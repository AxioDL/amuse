#ifndef __AMUSE_AUDIOGROUPPOOL_HPP__
#define __AMUSE_AUDIOGROUPPOOL_HPP__

#include <cstdint>
#include <vector>
#include <cmath>
#include <unordered_map>
#include "Entity.hpp"
#include "Common.hpp"

namespace amuse
{
class AudioGroupData;
struct SoundMacroState;
class Voice;

/** Header at the top of the pool file */
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
PoolHeader : BigDNA
{
    AT_DECL_DNA
    Value<atUint32, DNAEn> soundMacrosOffset;
    Value<atUint32, DNAEn> tablesOffset;
    Value<atUint32, DNAEn> keymapsOffset;
    Value<atUint32, DNAEn> layersOffset;
};

/** Header present at the top of each pool object */
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
ObjectHeader : BigDNA
{
    AT_DECL_DNA
    Value<atUint32, DNAEn> size;
    ObjectIdDNA<DNAEn> objectId;
    Seek<2, athena::Current> pad;
};

struct SoundMacro
{
    /** SoundMacro command operations */
    enum class CmdOp : uint8_t
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

    /** Base command interface. All versions of MusyX encode little-endian parameters */
    struct ICmd : LittleDNAV
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        virtual bool Do(SoundMacroState& st, Voice& vox) const = 0;
        virtual CmdOp Isa() const = 0;
    };
    struct CmdEnd : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::End; }
    };
    struct CmdStop : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Stop; }
    };
    struct CmdSplitKey : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> key;
        ObjectIdDNA<athena::Little> macro;
        Value<atUint16> macroStep;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SplitKey; }
    };
    struct CmdSplitVel : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> velocity;
        ObjectIdDNA<athena::Little> macro;
        Value<atUint16> macroStep;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SplitVel; }
    };
    struct CmdWaitTicks : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> keyOff;
        Value<bool> random;
        Value<bool> sampleEnd;
        Value<bool> absolute;
        Value<bool> msSwitch;
        Value<atUint16> ticksPerMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::WaitTicks; }
    };
    struct CmdLoop : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> keyOff;
        Value<bool> random;
        Value<bool> sampleEnd;
        Value<atUint16> macroStep;
        Value<atUint16> times;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Loop; }
    };
    struct CmdGoto : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Seek<1, athena::SeekOrigin::Current> dummy;
        ObjectIdDNA<athena::Little> macro;
        Value<atUint16> macroStep;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Goto; }
    };
    struct CmdWaitMs : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> keyOff;
        Value<bool> random;
        Value<bool> sampleEnd;
        Value<bool> absolute;
        Seek<1, athena::SeekOrigin::Current> dummy;
        Value<atUint16> ms;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::WaitMs; }
    };
    struct CmdPlayMacro : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> addNote;
        ObjectIdDNA<athena::Little> macro;
        Value<atUint16> macroStep;
        Value<atUint8> priority;
        Value<atUint8> maxVoices;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PlayMacro; }
    };
    struct CmdSendKeyOff : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> variable;
        Value<bool> lastStarted;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SendKeyOff; }
    };
    struct CmdSplitMod : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> modValue;
        ObjectIdDNA<athena::Little> macro;
        Value<atUint16> macroStep;
        Value<atUint8> priority;
        Value<atUint8> maxVoices;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SplitMod; }
    };
    struct CmdPianoPan : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> scale;
        Value<atInt8> centerKey;
        Value<atInt8> centerPan;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PianoPan; }
    };
    struct CmdSetAdsr : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        ObjectIdDNA<athena::Little> table;
        Value<bool> dlsMode;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetAdsr; }
    };
    struct CmdScaleVolume : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> scale;
        Value<atInt8> add;
        ObjectIdDNA<athena::Little> table;
        Value<bool> originalVol;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::ScaleVolume; }
    };
    struct CmdPanning : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> panPosition;
        Value<atUint16> timeMs;
        Value<atInt8> width;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Panning; }
    };
    struct CmdEnvelope : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> scale;
        Value<atInt8> add;
        ObjectIdDNA<athena::Little> table;
        Value<bool> msSwitch;
        Value<atUint16> fadeTime;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Envelope; }
    };
    struct CmdStartSample : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        SampleIdDNA<athena::Little> sample;
        Value<atInt8> mode;
        Value<atUint32> offset;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::StartSample; }
    };
    struct CmdStopSample : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::StopSample; }
    };
    struct CmdKeyOff : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::KeyOff; }
    };
    struct CmdSplitRnd : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> rnd;
        ObjectIdDNA<athena::Little> macro;
        Value<atUint16> macroStep;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SplitRnd; }
    };
    struct CmdFadeIn : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> scale;
        Value<atInt8> add;
        ObjectIdDNA<athena::Little> table;
        Value<bool> msSwitch;
        Value<atUint16> ticksPerMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::FadeIn; }
    };
    struct CmdSpanning : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> spanPosition;
        Value<atUint16> timeMs;
        Value<atInt8> width;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Spanning; }
    };
    struct CmdSetAdsrCtrl : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> attack;
        Value<atUint8> decay;
        Value<atUint8> sustain;
        Value<atUint8> release;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetAdsrCtrl; }
    };
    struct CmdRndNote : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> noteLo;
        Value<atInt8> detune;
        Value<atInt8> noteHi;
        Value<bool> fixedFree;
        Value<bool> absRel;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::RndNote; }
    };
    struct CmdAddNote : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> add;
        Value<atInt8> detune;
        Value<bool> originalKey;
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<bool> msSwitch;
        Value<atUint16> ticksPerMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::AddNote; }
    };
    struct CmdSetNote : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> key;
        Value<atInt8> detune;
        Seek<2, athena::SeekOrigin::Current> seek;
        Value<bool> msSwitch;
        Value<atUint16> ticksPerMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetNote; }
    };
    struct CmdLastNote : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> add;
        Value<atInt8> detune;
        Seek<2, athena::SeekOrigin::Current> seek;
        Value<bool> msSwitch;
        Value<atUint16> ticksPerMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::LastNote; }
    };
    struct CmdPortamento : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> portState;
        Value<atInt8> portType;
        Seek<2, athena::SeekOrigin::Current> seek;
        Value<bool> msSwitch;
        Value<atUint16> ticksPerMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Portamento; }
    };
    struct CmdVibrato : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> levelNote;
        Value<atUint8> levelFine;
        Value<bool> modwheelFlag;
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<bool> msSwitch;
        Value<atUint16> ticksPerMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Vibrato; }
    };
    struct CmdPitchSweep1 : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> times;
        Value<atInt16> add;
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<bool> msSwitch;
        Value<atUint16> ticksPerMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PitchSweep1; }
    };
    struct CmdPitchSweep2 : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> times;
        Value<atInt16> add;
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<bool> msSwitch;
        Value<atUint16> ticksPerMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PitchSweep2; }
    };
    struct CmdSetPitch : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        LittleUInt24 hz;
        Value<atUint16> fine;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetPitch; }
    };
    struct CmdSetPitchAdsr : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        ObjectIdDNA<athena::Little> table;
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<atInt8> keys;
        Value<atInt8> cents;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetPitchAdsr; }
    };
    struct CmdScaleVolumeDLS : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt16> scale;
        Value<bool> originalVol;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::ScaleVolumeDLS; }
    };
    struct CmdMod2Vibrange : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> keys;
        Value<atInt8> cents;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Mod2Vibrange; }
    };
    struct CmdSetupTremolo : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt16> scale;
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<atInt16> modwAddScale;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetupTremolo; }
    };
    struct CmdReturn : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::Return; }
    };
    struct CmdGoSub : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Seek<1, athena::SeekOrigin::Current> seek;
        ObjectIdDNA<athena::Little> macro;
        Value<atUint16> macroStep;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::GoSub; }
    };
    struct CmdTrapEvent : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> event;
        ObjectIdDNA<athena::Little> macro;
        Value<atUint16> macroStep;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::TrapEvent; }
    };
    struct CmdUntrapEvent : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> event;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::UntrapEvent; }
    };
    struct CmdSendMessage : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> isVar;
        ObjectIdDNA<athena::Little> macro;
        Value<atUint8> vid;
        Value<atUint8> variable;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SendMessage; }
    };
    struct CmdGetMessage : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> variable;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::GetMessage; }
    };
    struct CmdGetVid : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> variable;
        Value<bool> playMacro;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::GetVid; }
    };
    struct CmdAddAgeCount : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<atUint16> add;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::AddAgeCount; }
    };
    struct CmdSetAgeCount : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<atUint16> counter;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetAgeCount; }
    };
    struct CmdSendFlag : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> flagId;
        Value<atInt8> value;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SendFlag; }
    };
    struct CmdPitchWheelR : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> rangeUp;
        Value<atInt8> rangeDown;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PitchWheelR; }
    };
    struct CmdSetPriority : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atInt8> prio;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetPriority; }
    };
    struct CmdAddPriority : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<atInt16> prio;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::AddPriority; }
    };
    struct CmdAgeCntSpeed : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Seek<3, athena::SeekOrigin::Current> seek;
        Value<atUint32> time;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::AgeCntSpeed; }
    };
    struct CmdAgeCntVel : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Seek<1, athena::SeekOrigin::Current> seek;
        Value<atUint16> ageBase;
        Value<atUint16> ageScale;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::AgeCntVel; }
    };
    struct CmdVolSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<atInt8> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::VolSelect; }
    };
    struct CmdPanSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PanSelect; }
    };
    struct CmdPitchWheelSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PitchWheelSelect; }
    };
    struct CmdModWheelSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::ModWheelSelect; }
    };
    struct CmdPedalSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PedalSelect; }
    };
    struct CmdPortamentoSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PortamentoSelect; }
    };
    struct CmdReverbSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::ReverbSelect; }
    };
    struct CmdSpanSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SpanSelect; }
    };
    struct CmdDopplerSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::DopplerSelect; }
    };
    struct CmdTremoloSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::TremoloSelect; }
    };
    struct CmdPreASelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PreASelect; }
    };
    struct CmdPreBSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PreBSelect; }
    };
    struct CmdPostBSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::PostBSelect; }
    };
    struct CmdAuxAFXSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::AuxAFXSelect; }
    };
    struct CmdAuxBFXSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> midiControl;
        Value<atUint16> scalingPercentage;
        Value<bool> combine;
        Value<bool> isVar;
        Value<atUint8> fineScaling;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::AuxBFXSelect; }
    };
    struct CmdSetupLFO : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> lfoNumber;
        Value<atUint16> periodInMs;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetupLFO; }
    };
    struct CmdModeSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> dlsVol;
        Value<atUint8> itd;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::ModeSelect; }
    };
    struct CmdSetKeygroup : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> group;
        Value<bool> killNow;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SetKeygroup; }
    };
    struct CmdSRCmodeSelect : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<atUint8> srcType;
        Value<atUint8> type0SrcFilter;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SRCmodeSelect; }
    };
    struct CmdAddVars : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> varCtrlA;
        Value<atInt8> a;
        Value<bool> varCtrlB;
        Value<atInt8> b;
        Value<bool> varCtrlC;
        Value<atInt8> c;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::AddVars; }
    };
    struct CmdSubVars : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> varCtrlA;
        Value<atInt8> a;
        Value<bool> varCtrlB;
        Value<atInt8> b;
        Value<bool> varCtrlC;
        Value<atInt8> c;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::SubVars; }
    };
    struct CmdMulVars : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> varCtrlA;
        Value<atInt8> a;
        Value<bool> varCtrlB;
        Value<atInt8> b;
        Value<bool> varCtrlC;
        Value<atInt8> c;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::MulVars; }
    };
    struct CmdDivVars : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> varCtrlA;
        Value<atInt8> a;
        Value<bool> varCtrlB;
        Value<atInt8> b;
        Value<bool> varCtrlC;
        Value<atInt8> c;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::DivVars; }
    };
    struct CmdAddIVars : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> varCtrlA;
        Value<atInt8> a;
        Value<bool> varCtrlB;
        Value<atInt8> b;
        Value<atInt16> imm;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::AddIVars; }
    };
    struct CmdIfEqual : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> varCtrlA;
        Value<atInt8> a;
        Value<bool> varCtrlB;
        Value<atInt8> b;
        Value<bool> notEq;
        Value<atUint16> macroStep;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::IfEqual; }
    };
    struct CmdIfLess : ICmd
    {
        AT_DECL_DNA_YAML
        AT_DECL_DNAV
        Value<bool> varCtrlA;
        Value<atInt8> a;
        Value<bool> varCtrlB;
        Value<atInt8> b;
        Value<bool> notLt;
        Value<atUint16> macroStep;
        bool Do(SoundMacroState& st, Voice& vox) const;
        CmdOp Isa() const { return CmdOp::IfLess; }
    };

    std::vector<std::unique_ptr<ICmd>> m_cmds;
    int assertPC(int pc) const;
    
    const ICmd& getCmd(int i) const { return *m_cmds[assertPC(i)]; }

    template <athena::Endian DNAE>
    void readCmds(athena::io::IStreamReader& r, uint32_t size);
};

/** Converts time-cents representation to seconds */
static inline double TimeCentsToSeconds(int32_t tc)
{
    if (uint32_t(tc) == 0x80000000)
        return 0.0;
    return std::exp2(tc / (1200.0 * 65536.0));
}

/** Polymorphic interface for representing table data */
struct ITable : LittleDNAV
{
    AT_DECL_DNA_YAML
    AT_DECL_DNAV
};

/** Defines phase-based volume curve for macro volume control */
struct ADSR : ITable
{
    AT_DECL_DNA_YAML
    AT_DECL_DNAV
    Value<atUint16> attack;
    Value<atUint16> decay;
    Value<atUint16> sustain;     /* 0x1000 == 100% */
    Value<atUint16> release;     /* milliseconds */

    double getAttack() const { return attack / 1000.0; }
    double getDecay() const { return (decay == 0x8000) ? 0.0 : (decay / 1000.0); }
    double getSustain() const { return sustain / double(0x1000); }
    double getRelease() const { return release / 1000.0; }
};

/** Defines phase-based volume curve for macro volume control (modified DLS standard) */
struct ADSRDLS : ITable
{
    AT_DECL_DNA_YAML
    AT_DECL_DNAV
    Value<atUint32> attack;      /* 16.16 Time-cents */
    Value<atUint32> decay;       /* 16.16 Time-cents */
    Value<atUint16> sustain;     /* 0x1000 == 100% */
    Value<atUint16> release;     /* milliseconds */
    Value<atUint32> velToAttack; /* 16.16, 1000.0 == 100%; attack = <attack> + (vel/128) * <velToAttack> */
    Value<atUint32> keyToDecay;  /* 16.16, 1000.0 == 100%; decay = <decay> + (note/128) * <keyToDecay> */

    double getAttack() const { return TimeCentsToSeconds(attack); }
    double getDecay() const { return TimeCentsToSeconds(decay); }
    double getSustain() const { return sustain / double(0x1000); }
    double getRelease() const { return release / 1000.0; }
    double getVelToAttack(int8_t vel) const
    {
        if (velToAttack == 0x80000000)
            return getAttack();
        return getAttack() + vel * (velToAttack / 65536.0 / 1000.0) / 128.0;
    }
    double getKeyToDecay(int8_t note) const
    {
        if (keyToDecay == 0x80000000)
            return getDecay();
        return getDecay() + note * (keyToDecay / 65536.0 / 1000.0) / 128.0;
    }
};

/** Defines arbitrary data for use as volume curve */
struct Curve : ITable
{
    AT_DECL_EXPLICIT_DNA_YAML
    AT_DECL_DNAV
    std::vector<uint8_t> data;
};

/** Maps individual MIDI keys to sound-entity as indexed in table
 *  (macro-voice, keymap, layer) */
struct Keymap : LittleDNA
{
    AT_DECL_DNA_YAML
    Value<atInt8> transpose;
    Value<atInt8> pan; /* -128 for surround-channel only */
    Value<atInt8> prioOffset;
    Seek<3, athena::Current> pad;
};

/** Maps ranges of MIDI keys to sound-entity (macro-voice, keymap, layer) */
struct LayerMapping : LittleDNA
{
    AT_DECL_DNA_YAML
    Value<atInt8> keyLo;
    Value<atInt8> keyHi;
    Value<atInt8> transpose;
    Value<atInt8> volume;
    Value<atInt8> prioOffset;
    Value<atInt8> span;
    Value<atInt8> pan;
};

/** Database of functional objects within Audio Group */
class AudioGroupPool
{
    std::unordered_map<ObjectId, SoundMacro> m_soundMacros;
    std::unordered_map<ObjectId, std::unique_ptr<ITable>> m_tables;
    std::unordered_map<ObjectId, Keymap> m_keymaps;
    std::unordered_map<ObjectId, std::vector<LayerMapping>> m_layers;

    AudioGroupPool() = default;
    template <athena::Endian DNAE>
    static AudioGroupPool _AudioGroupPool(athena::io::IStreamReader& r);
public:
    static AudioGroupPool CreateAudioGroupPool(const AudioGroupData& data);

    const SoundMacro* soundMacro(ObjectId id) const;
    const Keymap* keymap(ObjectId id) const;
    const std::vector<LayerMapping>* layer(ObjectId id) const;
    const ADSR* tableAsAdsr(ObjectId id) const;
    const ADSRDLS* tableAsAdsrDLS(ObjectId id) const { return reinterpret_cast<const ADSRDLS*>(tableAsAdsr(id)); }
    const Curve* tableAsCurves(ObjectId id) const { return reinterpret_cast<const Curve*>(tableAsAdsr(id)); }
};
}

#endif // __AMUSE_AUDIOGROUPPOOL_HPP__

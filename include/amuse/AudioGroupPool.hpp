#pragma once

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "amuse/Common.hpp"
#include "amuse/Entity.hpp"

#include <athena/DNA.hpp>
#include <athena/MemoryReader.hpp>

namespace amuse {
class AudioGroupData;
struct SoundMacroState;
class Voice;

/** Header at the top of the pool file */
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little) PoolHeader : BigDNA {
  AT_DECL_DNA
  Value<atUint32, DNAEn> soundMacrosOffset;
  Value<atUint32, DNAEn> tablesOffset;
  Value<atUint32, DNAEn> keymapsOffset;
  Value<atUint32, DNAEn> layersOffset;
};

/** Header present at the top of each pool object */
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little) ObjectHeader : BigDNA {
  AT_DECL_DNA
  Value<atUint32, DNAEn> size;
  ObjectIdDNA<DNAEn> objectId;
  Seek<2, athena::SeekOrigin::Current> pad;
};

struct SoundMacro {
  /** SoundMacro command operations */
  enum class CmdOp : uint8_t {
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
    AddPriority,        /* unimplemented */
    AgeCntSpeed,        /* unimplemented */
    AgeCntVel,          /* unimplemented */
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
    WiiUnknown = 0x5E,
    WiiUnknown2 = 0x5F,
    AddVars = 0x60,
    SubVars,
    MulVars,
    DivVars,
    AddIVars,
    SetVar,
    IfEqual = 0x70,
    IfLess,
    CmdOpMAX,
    Invalid = 0xff
  };

  enum class CmdType : uint8_t { Control, Pitch, Sample, Setup, Special, Structure, Volume, CmdTypeMAX };

  /** Introspection structure used by editors to define user interactivity per command */
  struct CmdIntrospection {
    struct Field {
      enum class Type {
        Invalid,
        Bool,
        Int8,
        UInt8,
        Int16,
        UInt16,
        Int32,
        UInt32,
        SoundMacroId,
        SoundMacroStep,
        TableId,
        SampleId,
        Choice
      };
      Type m_tp;
      size_t m_offset;
      std::string_view m_name;
      int64_t m_min, m_max, m_default;
      std::string_view m_choices[4];
    };
    CmdType m_tp;
    std::string_view m_name;
    std::string_view m_description;
    Field m_fields[7];
  };

  /** Base command interface. All versions of MusyX encode little-endian parameters */
  struct ICmd : LittleDNAV {
    AT_DECL_DNA_YAMLV
    virtual bool Do(SoundMacroState& st, Voice& vox) const = 0;
    virtual CmdOp Isa() const = 0;
  };
  struct CmdEnd : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::End; }
  };
  struct CmdStop : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Stop; }
  };
  struct CmdSplitKey : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> key;
    SoundMacroIdDNA<athena::Endian::Little> macro;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SplitKey; }
  };
  struct CmdSplitVel : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> velocity;
    SoundMacroIdDNA<athena::Endian::Little> macro;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SplitVel; }
  };
  struct CmdWaitTicks : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> keyOff;
    Value<bool> random;
    Value<bool> sampleEnd;
    Value<bool> absolute;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::WaitTicks; }
  };
  struct CmdLoop : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> keyOff;
    Value<bool> random;
    Value<bool> sampleEnd;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    Value<atUint16> times;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Loop; }
  };
  struct CmdGoto : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Seek<1, athena::SeekOrigin::Current> dummy;
    SoundMacroIdDNA<athena::Endian::Little> macro;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Goto; }
  };
  struct CmdWaitMs : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> keyOff;
    Value<bool> random;
    Value<bool> sampleEnd;
    Value<bool> absolute;
    Seek<1, athena::SeekOrigin::Current> dummy;
    Value<atUint16> ms;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::WaitMs; }
  };
  struct CmdPlayMacro : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> addNote;
    SoundMacroIdDNA<athena::Endian::Little> macro;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    Value<atUint8> priority;
    Value<atUint8> maxVoices;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PlayMacro; }
  };
  struct CmdSendKeyOff : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> variable;
    Value<bool> lastStarted;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SendKeyOff; }
  };
  struct CmdSplitMod : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> modValue;
    SoundMacroIdDNA<athena::Endian::Little> macro;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SplitMod; }
  };
  struct CmdPianoPan : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> scale;
    Value<atInt8> centerKey;
    Value<atInt8> centerPan;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PianoPan; }
  };
  struct CmdSetAdsr : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    TableIdDNA<athena::Endian::Little> table;
    Value<bool> dlsMode;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetAdsr; }
  };
  struct CmdScaleVolume : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> scale;
    Value<atInt8> add;
    TableIdDNA<athena::Endian::Little> table;
    Value<bool> originalVol;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::ScaleVolume; }
  };
  struct CmdPanning : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> panPosition;
    Value<atUint16> timeMs;
    Value<atInt8> width;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Panning; }
  };
  struct CmdEnvelope : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> scale;
    Value<atInt8> add;
    TableIdDNA<athena::Endian::Little> table;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Envelope; }
  };
  struct CmdStartSample : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    enum class Mode : atInt8 { NoScale = 0, Negative = 1, Positive = 2 };
    SampleIdDNA<athena::Endian::Little> sample;
    Value<Mode> mode;
    Value<atUint32> offset;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::StartSample; }
  };
  struct CmdStopSample : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::StopSample; }
  };
  struct CmdKeyOff : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::KeyOff; }
  };
  struct CmdSplitRnd : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> rnd;
    SoundMacroIdDNA<athena::Endian::Little> macro;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SplitRnd; }
  };
  struct CmdFadeIn : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> scale;
    Value<atInt8> add;
    TableIdDNA<athena::Endian::Little> table;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::FadeIn; }
  };
  struct CmdSpanning : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> spanPosition;
    Value<atUint16> timeMs;
    Value<atInt8> width;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Spanning; }
  };
  struct CmdSetAdsrCtrl : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> attack;
    Value<atUint8> decay;
    Value<atUint8> sustain;
    Value<atUint8> release;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetAdsrCtrl; }
  };
  struct CmdRndNote : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> noteLo;
    Value<atInt8> detune;
    Value<atInt8> noteHi;
    Value<bool> fixedFree;
    Value<bool> absRel;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::RndNote; }
  };
  struct CmdAddNote : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> add;
    Value<atInt8> detune;
    Value<bool> originalKey;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::AddNote; }
  };
  struct CmdSetNote : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> key;
    Value<atInt8> detune;
    Seek<2, athena::SeekOrigin::Current> seek;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetNote; }
  };
  struct CmdLastNote : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> add;
    Value<atInt8> detune;
    Seek<2, athena::SeekOrigin::Current> seek;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::LastNote; }
  };
  struct CmdPortamento : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    enum class PortState : atInt8 { Disable, Enable, MIDIControlled };
    Value<PortState> portState;
    enum class PortType : atInt8 { LastPressed, Always };
    Value<PortType> portType;
    Seek<2, athena::SeekOrigin::Current> seek;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Portamento; }
  };
  struct CmdVibrato : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> levelNote;
    Value<atInt8> levelFine;
    Value<bool> modwheelFlag;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Vibrato; }
  };
  struct CmdPitchSweep1 : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> times;
    Value<atInt16> add;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PitchSweep1; }
  };
  struct CmdPitchSweep2 : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> times;
    Value<atInt16> add;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<bool> msSwitch;
    Value<atUint16> ticksOrMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PitchSweep2; }
  };
  struct CmdSetPitch : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    LittleUInt24 hz;
    Value<atUint16> fine;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetPitch; }
  };
  struct CmdSetPitchAdsr : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    TableIdDNA<athena::Endian::Little> table;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<atInt8> keys;
    Value<atInt8> cents;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetPitchAdsr; }
  };
  struct CmdScaleVolumeDLS : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt16> scale;
    Value<bool> originalVol;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::ScaleVolumeDLS; }
  };
  struct CmdMod2Vibrange : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> keys;
    Value<atInt8> cents;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Mod2Vibrange; }
  };
  struct CmdSetupTremolo : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt16> scale;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<atInt16> modwAddScale;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetupTremolo; }
  };
  struct CmdReturn : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::Return; }
  };
  struct CmdGoSub : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Seek<1, athena::SeekOrigin::Current> seek;
    SoundMacroIdDNA<athena::Endian::Little> macro;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::GoSub; }
  };
  struct CmdTrapEvent : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    enum class EventType : atInt8 { KeyOff, SampleEnd, MessageRecv };
    Value<EventType> event;
    SoundMacroIdDNA<athena::Endian::Little> macro;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::TrapEvent; }
  };
  struct CmdUntrapEvent : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<CmdTrapEvent::EventType> event;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::UntrapEvent; }
  };
  struct CmdSendMessage : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> isVar;
    SoundMacroIdDNA<athena::Endian::Little> macro;
    Value<atUint8> voiceVar;
    Value<atUint8> valueVar;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SendMessage; }
  };
  struct CmdGetMessage : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> variable;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::GetMessage; }
  };
  struct CmdGetVid : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> variable;
    Value<bool> playMacro;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::GetVid; }
  };
  struct CmdAddAgeCount : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<atInt16> add;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::AddAgeCount; }
  };
  struct CmdSetAgeCount : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<atUint16> counter;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetAgeCount; }
  };
  struct CmdSendFlag : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> flagId;
    Value<atUint8> value;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SendFlag; }
  };
  struct CmdPitchWheelR : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atInt8> rangeUp;
    Value<atInt8> rangeDown;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PitchWheelR; }
  };
  struct CmdSetPriority : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> prio;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetPriority; }
  };
  struct CmdAddPriority : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<atInt16> prio;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::AddPriority; }
  };
  struct CmdAgeCntSpeed : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Seek<3, athena::SeekOrigin::Current> seek;
    Value<atUint32> time;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::AgeCntSpeed; }
  };
  struct CmdAgeCntVel : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Seek<1, athena::SeekOrigin::Current> seek;
    Value<atUint16> ageBase;
    Value<atUint16> ageScale;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::AgeCntVel; }
  };
  enum class Combine : atInt8 { Set, Add, Mult };
  struct CmdVolSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::VolSelect; }
  };
  struct CmdPanSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PanSelect; }
  };
  struct CmdPitchWheelSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PitchWheelSelect; }
  };
  struct CmdModWheelSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::ModWheelSelect; }
  };
  struct CmdPedalSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PedalSelect; }
  };
  struct CmdPortamentoSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PortamentoSelect; }
  };
  struct CmdReverbSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::ReverbSelect; }
  };
  struct CmdSpanSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SpanSelect; }
  };
  struct CmdDopplerSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::DopplerSelect; }
  };
  struct CmdTremoloSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::TremoloSelect; }
  };
  struct CmdPreASelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PreASelect; }
  };
  struct CmdPreBSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PreBSelect; }
  };
  struct CmdPostBSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::PostBSelect; }
  };
  struct CmdAuxAFXSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    Value<atUint8> paramIndex;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::AuxAFXSelect; }
  };
  struct CmdAuxBFXSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> midiControl;
    Value<atInt16> scalingPercentage;
    Value<Combine> combine;
    Value<bool> isVar;
    Value<atInt8> fineScaling;
    Value<atUint8> paramIndex;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::AuxBFXSelect; }
  };
  struct CmdSetupLFO : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> lfoNumber;
    Value<atInt16> periodInMs;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetupLFO; }
  };
  struct CmdModeSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> dlsVol;
    Value<bool> itd;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::ModeSelect; }
  };
  struct CmdSetKeygroup : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> group;
    Value<bool> killNow;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetKeygroup; }
  };
  struct CmdSRCmodeSelect : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<atUint8> srcType;
    Value<atUint8> type0SrcFilter;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SRCmodeSelect; }
  };
  struct CmdWiiUnknown : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> flag;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::WiiUnknown; }
  };
  struct CmdWiiUnknown2 : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> flag;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::WiiUnknown2; }
  };
  struct CmdAddVars : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> varCtrlA;
    Value<atUint8> a;
    Value<bool> varCtrlB;
    Value<atUint8> b;
    Value<bool> varCtrlC;
    Value<atUint8> c;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::AddVars; }
  };
  struct CmdSubVars : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> varCtrlA;
    Value<atInt8> a;
    Value<bool> varCtrlB;
    Value<atInt8> b;
    Value<bool> varCtrlC;
    Value<atInt8> c;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SubVars; }
  };
  struct CmdMulVars : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> varCtrlA;
    Value<atInt8> a;
    Value<bool> varCtrlB;
    Value<atInt8> b;
    Value<bool> varCtrlC;
    Value<atInt8> c;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::MulVars; }
  };
  struct CmdDivVars : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> varCtrlA;
    Value<atInt8> a;
    Value<bool> varCtrlB;
    Value<atInt8> b;
    Value<bool> varCtrlC;
    Value<atInt8> c;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::DivVars; }
  };
  struct CmdAddIVars : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> varCtrlA;
    Value<atInt8> a;
    Value<bool> varCtrlB;
    Value<atInt8> b;
    Value<atInt16> imm;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::AddIVars; }
  };
  struct CmdSetVar : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> varCtrlA;
    Value<atInt8> a;
    Seek<1, athena::SeekOrigin::Current> pad;
    Value<atInt16> imm;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::SetVar; }
  };
  struct CmdIfEqual : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> varCtrlA;
    Value<atInt8> a;
    Value<bool> varCtrlB;
    Value<atInt8> b;
    Value<bool> notEq;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::IfEqual; }
  };
  struct CmdIfLess : ICmd {
    AT_DECL_DNA_YAMLV
    static const CmdIntrospection Introspective;
    Value<bool> varCtrlA;
    Value<atInt8> a;
    Value<bool> varCtrlB;
    Value<atInt8> b;
    Value<bool> notLt;
    SoundMacroStepDNA<athena::Endian::Little> macroStep;
    bool Do(SoundMacroState& st, Voice& vox) const override;
    CmdOp Isa() const override { return CmdOp::IfLess; }
  };

  template <class Op, class O, class... _Args>
  static O CmdDo(_Args&&... args);
  static std::unique_ptr<SoundMacro::ICmd> MakeCmd(CmdOp op);
  static const CmdIntrospection* GetCmdIntrospection(CmdOp op);
  static std::string_view CmdOpToStr(CmdOp op);
  static CmdOp CmdStrToOp(std::string_view op);

  std::vector<std::unique_ptr<ICmd>> m_cmds;
  int assertPC(int pc) const;

  const ICmd& getCmd(int i) const { return *m_cmds[assertPC(i)]; }

  template <athena::Endian DNAE>
  void readCmds(athena::io::IStreamReader& r, uint32_t size);
  template <athena::Endian DNAE>
  void writeCmds(athena::io::IStreamWriter& w) const;

  ICmd* insertNewCmd(int idx, CmdOp op) { return m_cmds.insert(m_cmds.begin() + idx, MakeCmd(op))->get(); }
  ICmd* insertCmd(int idx, std::unique_ptr<ICmd>&& cmd) {
    return m_cmds.insert(m_cmds.begin() + idx, std::move(cmd))->get();
  }
  std::unique_ptr<ICmd> deleteCmd(int idx) {
    std::unique_ptr<ICmd> ret = std::move(m_cmds[idx]);
    m_cmds.erase(m_cmds.begin() + idx);
    return ret;
  }
  void swapPositions(int a, int b) {
    if (a == b)
      return;
    std::swap(m_cmds[a], m_cmds[b]);
  }
  void buildFromPrototype(const SoundMacro& other);

  void toYAML(athena::io::YAMLDocWriter& w) const;
  void fromYAML(athena::io::YAMLDocReader& r, size_t cmdCount);
};

template <typename T>
inline T& AccessField(SoundMacro::ICmd* cmd, const SoundMacro::CmdIntrospection::Field& field) {
  return *reinterpret_cast<T*>(reinterpret_cast<uintptr_t>(std::addressof(*cmd)) + field.m_offset);
}

/** Converts time-cents representation to seconds */
inline double TimeCentsToSeconds(int32_t tc) {
  if (uint32_t(tc) == 0x80000000)
    return 0.0;
  return std::exp2(tc / (1200.0 * 65536.0));
}

/** Converts seconds representation to time-cents */
inline int32_t SecondsToTimeCents(double sec) {
  if (sec == 0.0)
    return 0x80000000;
  return int32_t(std::log2(sec) * (1200.0 * 65536.0));
}

/** Polymorphic interface for representing table data */
struct ITable : LittleDNAV {
  AT_DECL_DNA_YAMLV
  enum class Type { ADSR, ADSRDLS, Curve };
  virtual Type Isa() const = 0;
};

/** Defines phase-based volume curve for macro volume control */
struct ADSR : ITable {
  AT_DECL_DNA_YAMLV
  Value<atUint16> attack = 0;
  Value<atUint16> decay = 0x8000;
  Value<atUint16> sustain = 0; /* 0x1000 == 100% */
  Value<atUint16> release = 0; /* milliseconds */

  double getAttack() const { return attack / 1000.0; }
  void setAttack(double v) { attack = v * 1000.0; }
  double getDecay() const { return (decay == 0x8000) ? 0.0 : (decay / 1000.0); }
  void setDecay(double v) { decay = v == 0.0 ? 0x8000 : v * 1000.0; }
  double getSustain() const { return sustain / double(0x1000); }
  void setSustain(double v) { sustain = v * double(0x1000); }
  double getRelease() const { return release / 1000.0; }
  void setRelease(double v) { release = v * 1000.0; }

  Type Isa() const override { return ITable::Type::ADSR; }
};

/** Defines phase-based volume curve for macro volume control (modified DLS standard) */
struct ADSRDLS : ITable {
  AT_DECL_DNA_YAMLV
  Value<atUint32> attack = 0x80000000;      /* 16.16 Time-cents */
  Value<atUint32> decay = 0x80000000;       /* 16.16 Time-cents */
  Value<atUint16> sustain = 0;              /* 0x1000 == 100% */
  Value<atUint16> release = 0;              /* milliseconds */
  Value<atUint32> velToAttack = 0x80000000; /* 16.16, 1000.0 == 100%; attack = <attack> + (vel/128) * <velToAttack> */
  Value<atUint32> keyToDecay = 0x80000000;  /* 16.16, 1000.0 == 100%; decay = <decay> + (note/128) * <keyToDecay> */

  double getAttack() const { return TimeCentsToSeconds(attack); }
  void setAttack(double v) { attack = SecondsToTimeCents(v); }
  double getDecay() const { return TimeCentsToSeconds(decay); }
  void setDecay(double v) { decay = SecondsToTimeCents(v); }
  double getSustain() const { return sustain / double(0x1000); }
  void setSustain(double v) { sustain = v * double(0x1000); }
  double getRelease() const { return release / 1000.0; }
  void setRelease(double v) { release = v * 1000.0; }

  double _getVelToAttack() const {
    if (velToAttack == 0x80000000)
      return 0.0;
    else
      return velToAttack / 65536.0 / 1000.0;
  }

  void _setVelToAttack(double v) {
    if (v == 0.0)
      velToAttack = 0x80000000;
    else
      velToAttack = atUint32(v * 1000.0 * 65536.0);
  }

  double _getKeyToDecay() const {
    if (keyToDecay == 0x80000000)
      return 0.0;
    else
      return keyToDecay / 65536.0 / 1000.0;
  }

  void _setKeyToDecay(double v) {
    if (v == 0.0)
      keyToDecay = 0x80000000;
    else
      keyToDecay = atUint32(v * 1000.0 * 65536.0);
  }

  double getVelToAttack(int8_t vel) const {
    if (velToAttack == 0x80000000)
      return getAttack();
    return getAttack() + vel * (velToAttack / 65536.0 / 1000.0) / 128.0;
  }

  double getKeyToDecay(int8_t note) const {
    if (keyToDecay == 0x80000000)
      return getDecay();
    return getDecay() + note * (keyToDecay / 65536.0 / 1000.0) / 128.0;
  }

  Type Isa() const override  { return ITable::Type::ADSRDLS; }
};

/** Defines arbitrary data for use as volume curve */
struct Curve : ITable {
  AT_DECL_EXPLICIT_DNA_YAMLV
  std::vector<uint8_t> data;

  Type Isa() const override  { return ITable::Type::Curve; }
};

/** Maps individual MIDI keys to sound-entity as indexed in table
 *  (macro-voice, keymap, layer) */
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little) KeymapDNA : BigDNA {
  AT_DECL_DNA
  SoundMacroIdDNA<DNAEn> macro;
  Value<atInt8> transpose;
  Value<atInt8> pan; /* -128 for surround-channel only */
  Value<atInt8> prioOffset;
  Seek<3, athena::SeekOrigin::Current> pad;
};
struct Keymap : BigDNA {
  AT_DECL_DNA_YAML
  SoundMacroIdDNA<athena::Endian::Big> macro;
  Value<atInt8> transpose = 0;
  Value<atInt8> pan = 64; /* -128 for surround-channel only */
  Value<atInt8> prioOffset = 0;

  Keymap() = default;

  template <athena::Endian DNAE>
  Keymap(const KeymapDNA<DNAE>& in)
  : macro(in.macro.id), transpose(in.transpose), pan(in.pan), prioOffset(in.prioOffset) {}

  template <athena::Endian DNAEn>
  KeymapDNA<DNAEn> toDNA() const {
    KeymapDNA<DNAEn> ret;
    ret.macro.id = macro;
    ret.transpose = transpose;
    ret.pan = pan;
    ret.prioOffset = prioOffset;
    return ret;
  }

  uint64_t configKey() const {
    return uint64_t(macro.id.id) | (uint64_t(transpose) << 16) | (uint64_t(pan) << 24) | (uint64_t(prioOffset) << 32);
  }
};

/** Maps ranges of MIDI keys to sound-entity (macro-voice, keymap, layer) */
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little) LayerMappingDNA : BigDNA {
  AT_DECL_DNA
  SoundMacroIdDNA<DNAEn> macro;
  Value<atInt8> keyLo;
  Value<atInt8> keyHi;
  Value<atInt8> transpose;
  Value<atInt8> volume;
  Value<atInt8> prioOffset;
  Value<atInt8> span;
  Value<atInt8> pan;
  Seek<3, athena::SeekOrigin::Current> pad;
};
struct LayerMapping : BigDNA {
  AT_DECL_DNA_YAML
  SoundMacroIdDNA<athena::Endian::Big> macro;
  Value<atInt8> keyLo = 0;
  Value<atInt8> keyHi = 127;
  Value<atInt8> transpose = 0;
  Value<atInt8> volume = 127;
  Value<atInt8> prioOffset = 0;
  Value<atInt8> span = 0;
  Value<atInt8> pan = 64;

  LayerMapping() = default;

  template <athena::Endian DNAE>
  LayerMapping(const LayerMappingDNA<DNAE>& in)
  : macro(in.macro.id)
  , keyLo(in.keyLo)
  , keyHi(in.keyHi)
  , transpose(in.transpose)
  , volume(in.volume)
  , prioOffset(in.prioOffset)
  , span(in.span)
  , pan(in.pan) {}

  template <athena::Endian DNAEn>
  LayerMappingDNA<DNAEn> toDNA() const {
    LayerMappingDNA<DNAEn> ret;
    ret.macro.id = macro;
    ret.keyLo = keyLo;
    ret.keyHi = keyHi;
    ret.transpose = transpose;
    ret.volume = volume;
    ret.prioOffset = prioOffset;
    ret.span = span;
    ret.pan = pan;
    return ret;
  }
};

/** Database of functional objects within Audio Group */
class AudioGroupPool {
  std::unordered_map<SoundMacroId, ObjToken<SoundMacro>> m_soundMacros;
  std::unordered_map<TableId, ObjToken<std::unique_ptr<ITable>>> m_tables;
  std::unordered_map<KeymapId, ObjToken<std::array<Keymap, 128>>> m_keymaps;
  std::unordered_map<LayersId, ObjToken<std::vector<LayerMapping>>> m_layers;

  template <athena::Endian DNAE>
  static AudioGroupPool _AudioGroupPool(athena::io::IStreamReader& r);

public:
  AudioGroupPool() = default;
  static AudioGroupPool CreateAudioGroupPool(const AudioGroupData& data);
  static AudioGroupPool CreateAudioGroupPool(SystemStringView groupPath);

  const std::unordered_map<SoundMacroId, ObjToken<SoundMacro>>& soundMacros() const { return m_soundMacros; }
  const std::unordered_map<TableId, ObjToken<std::unique_ptr<ITable>>>& tables() const { return m_tables; }
  const std::unordered_map<KeymapId, ObjToken<std::array<Keymap, 128>>>& keymaps() const { return m_keymaps; }
  const std::unordered_map<LayersId, ObjToken<std::vector<LayerMapping>>>& layers() const { return m_layers; }
  std::unordered_map<SoundMacroId, ObjToken<SoundMacro>>& soundMacros() { return m_soundMacros; }
  std::unordered_map<TableId, ObjToken<std::unique_ptr<ITable>>>& tables() { return m_tables; }
  std::unordered_map<KeymapId, ObjToken<std::array<Keymap, 128>>>& keymaps() { return m_keymaps; }
  std::unordered_map<LayersId, ObjToken<std::vector<LayerMapping>>>& layers() { return m_layers; }

  const SoundMacro* soundMacro(ObjectId id) const;
  const Keymap* keymap(ObjectId id) const;
  const std::vector<LayerMapping>* layer(ObjectId id) const;
  const ADSR* tableAsAdsr(ObjectId id) const;
  const ADSRDLS* tableAsAdsrDLS(ObjectId id) const;
  const Curve* tableAsCurves(ObjectId id) const;

  std::vector<uint8_t> toYAML() const;
  template <athena::Endian DNAE>
  std::vector<uint8_t> toData() const;

  AudioGroupPool(const AudioGroupPool&) = delete;
  AudioGroupPool& operator=(const AudioGroupPool&) = delete;
  AudioGroupPool(AudioGroupPool&&) = default;
  AudioGroupPool& operator=(AudioGroupPool&&) = default;
};
} // namespace amuse

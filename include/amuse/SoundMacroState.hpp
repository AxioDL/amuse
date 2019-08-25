#pragma once

#include <cstdint>
#include <tuple>
#include <vector>

#include "amuse/AudioGroupPool.hpp"
#include "amuse/Common.hpp"
#include "amuse/Entity.hpp"

/* Squelch Win32 macro pollution >.< */
#undef SendMessage
#undef GetMessage

namespace amuse {
class Voice;

/** Real-time state of SoundMacro execution */
struct SoundMacroState {
  /** 'program counter' stack for the active SoundMacro */
  std::vector<std::tuple<ObjectId, const SoundMacro*, int>> m_pc;
  void _setPC(int pc) { std::get<2>(m_pc.back()) = std::get<1>(m_pc.back())->assertPC(pc); }

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

  SoundMacro::CmdPortamento::PortState m_portamentoMode =
      SoundMacro::CmdPortamento::PortState::MIDIControlled; /**< (0: Off, 1: On, 2: MIDI specified) */
  SoundMacro::CmdPortamento::PortType m_portamentoType =
      SoundMacro::CmdPortamento::PortType::LastPressed; /**< (0: New key pressed while old key pressed, 1: Always) */
  float m_portamentoTime = 0.5f;                        /**< portamento transition time, 0.f will perform legato */

  /** Used to build a multi-component formula for overriding controllers */
  struct Evaluator {
    enum class Combine : uint8_t { Set, Add, Mult };
    enum class VarType : uint8_t { Ctrl, Var };

    /** Represents one term of the formula assembled via *_SELECT commands */
    struct Component {
      uint8_t m_midiCtrl;
      float m_scale;
      Combine m_combine;
      VarType m_varType;

      Component(uint8_t midiCtrl, float scale, Combine combine, VarType varType)
      : m_midiCtrl(midiCtrl), m_scale(scale), m_combine(combine), m_varType(varType) {}
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
  Evaluator m_auxAFxSel[3];
  Evaluator m_auxBFxSel[3];
  Evaluator m_postAuxB;
  Evaluator m_spanSel;
  Evaluator m_dopplerSel;
  Evaluator m_tremoloSel;

  int32_t m_variables[32]; /**< 32-bit variables set with relevant commands */

  /** Event registration data for TRAP_EVENT */
  struct EventTrap {
    ObjectId macroId = 0xffff;
    uint16_t macroStep;
  };

public:
  /** initialize state for SoundMacro data at `ptr` */
  void initialize(ObjectId id, const SoundMacro* macro, int step);
  void initialize(ObjectId id, const SoundMacro* macro, int step, double ticksPerSec, uint8_t midiKey, uint8_t midiVel,
                  uint8_t midiMod);

  /** advances `dt` seconds worth of commands in the SoundMacro
   *  @return `true` if END reached
   */
  bool advance(Voice& vox, double dt);

  /** keyoff event */
  void keyoffNotify(Voice& vox);

  /** sample end event */
  void sampleEndNotify(Voice& vox);
};
} // namespace amuse

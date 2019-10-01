#pragma once

#include <array>
#include <cstdint>
#include <cstdlib>
#include <list>
#include <memory>
#include <queue>

#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupSampleDirectory.hpp"
#include "amuse/Entity.hpp"
#include "amuse/Envelope.hpp"
#include "amuse/SoundMacroState.hpp"
#include "amuse/Studio.hpp"

namespace amuse {
class IBackendVoice;
struct Keymap;
struct LayerMapping;

/** State of voice over lifetime */
enum class VoiceState {
  Playing, /**< SoundMacro actively executing, not in KeyOff */
  KeyOff,  /**< KeyOff event issued, macro beginning fade-out */
  Dead     /**< Default state, causes Engine to remove voice at end of pump cycle */
};

/** Individual source of audio */
class Voice : public Entity {
  friend class Emitter;
  friend class Engine;
  friend class Envelope;
  friend class Sequencer;
  friend struct SoundMacro::CmdGetMessage;
  friend struct SoundMacro::CmdGoSub;
  friend struct SoundMacro::CmdKeyOff;
  friend struct SoundMacro::CmdModeSelect;
  friend struct SoundMacro::CmdReturn;
  friend struct SoundMacro::CmdScaleVolume;
  friend struct SoundMacro::CmdScaleVolumeDLS;
  friend struct SoundMacro::CmdTrapEvent;
  friend struct SoundMacro::CmdUntrapEvent;
  friend struct SoundMacroState;

  struct VolumeCache {
    bool m_curDLS = false;       /**< Current DLS status of cached lookup */
    float m_curVolLUTKey = -1.f; /**< Current input level cached out of LUT */
    float m_curVolLUTVal = 0.f;  /**< Current output level cached out of LUT */
    float getVolume(float vol, bool dls);
    float getCachedVolume() const { return m_curVolLUTVal; }
  };

  struct Panning {
    double m_time;  /**< time of PANNING command */
    double m_dur;   /**< requested duration of PANNING command */
    uint8_t m_pos;  /**< initial pan value of PANNING command */
    int8_t m_width; /**< delta pan value to target of PANNING command */
  };

  void _setObjectId(ObjectId id) { m_objectId = id; }

  int m_vid;                        /**< VoiceID of this voice instance */
  bool m_emitter;                   /**< Voice is part of an Emitter */
  ObjToken<Studio> m_studio;        /**< Studio this voice outputs to */
  IObjToken<Sequencer> m_sequencer; /**< Strong reference to parent sequencer to retain ctrl vals */

  std::unique_ptr<IBackendVoice> m_backendVoice; /**< Handle to client-implemented backend voice */
  SoundMacroState m_state;                       /**< State container for SoundMacro playback */
  SoundMacroState::EventTrap m_keyoffTrap;    /**< Trap for keyoff (SoundMacro overrides default envelope behavior) */
  SoundMacroState::EventTrap m_sampleEndTrap; /**< Trap for sampleend (SoundMacro overrides voice removal) */
  SoundMacroState::EventTrap m_messageTrap;   /**< Trap for messages sent from other SoundMacros */
  int32_t m_latestMessage = 0;                /**< Latest message received on voice */
  std::list<ObjToken<Voice>> m_childVoices;   /**< Child voices for PLAYMACRO usage */
  uint8_t m_keygroup = 0;                     /**< Keygroup voice is a member of */

  ObjToken<SampleEntryData> m_curSample;          /**< Current sample entry playing */
  const unsigned char* m_curSampleData = nullptr; /**< Current sample data playing */
  SampleFormat m_curFormat;                       /**< Current sample format playing */
  uint32_t m_curSamplePos = 0;                    /**< Current sample position */
  uint32_t m_lastSamplePos = 0;                   /**< Last sample position (or last loop sample) */
  int16_t m_prev1 = 0;                            /**< DSPADPCM prev sample */
  int16_t m_prev2 = 0;                            /**< DSPADPCM prev-prev sample */
  double m_dopplerRatio = 1.0;                    /**< Current ratio to mix with chromatic pitch for doppler effects */
  double m_sampleRate = NativeSampleRate; /**< Current sample rate computed from relative sample key or SETPITCH */
  double m_voiceTime = 0.0;               /**< Current seconds of voice playback (per-sample resolution) */
  uint64_t m_voiceSamples = 0;            /**< Count of samples processed over voice's lifetime */
  float m_lastLevel = 0.f;                /**< Last computed level ([0,1] mapped to [-10,0] clamped decibels) */
  float m_nextLevel = 0.f;                /**< Next computed level used for lerp-mode amplitude */
  VolumeCache m_nextLevelCache;
  VolumeCache m_lerpedCache;

  VoiceState m_voxState = VoiceState::Dead; /**< Current high-level state of voice */
  bool m_sustained = false;                 /**< Sustain pedal pressed for this voice */
  bool m_sustainKeyOff = false;             /**< Keyoff event occured while sustained */
  uint8_t m_curAftertouch = 0;              /**< Aftertouch value (key pressure when 'bottoming out') */

  float m_targetUserVol = 1.f;    /**< Target user volume of voice (slewed to prevent audible aliasing) */
  float m_curUserVol = 1.f;       /**< Current user volume of voice */
  float m_curVol = 1.f;           /**< Current volume of voice */
  float m_envelopeVol = 1.f;      /**< Current envelope volume of voice */
  float m_curReverbVol = 0.f;     /**< Current reverb volume of voice */
  float m_curAuxBVol = 0.f;       /**< Current AuxB volume of voice */
  float m_curPan = 0.f;           /**< Current pan of voice */
  float m_curSpan = -1.f;         /**< Current surround pan of voice */
  float m_curPitchWheel = 0.f;    /**< Current normalized wheel value for control */
  int32_t m_pitchWheelUp = 200;   /**< Up range for pitchwheel control in cents */
  int32_t m_pitchWheelDown = 200; /**< Down range for pitchwheel control in cents */
  int32_t m_pitchWheelVal = 0;    /**< Current resolved pitchwheel delta for control */
  int32_t m_curPitch;             /**< Current base pitch in cents */
  bool m_pitchDirty = true;       /**< m_curPitch has been updated and needs sending to voice */
  bool m_needsSlew = false;       /**< next _setTotalPitch will be slewed */
  bool m_dlsVol = false;          /**< Use DLS LUT for resolving voice volume */

  Envelope m_volAdsr;           /**< Volume envelope */
  double m_envelopeTime = -1.f; /**< time since last ENVELOPE command, -1 for no active volume-sweep */
  double m_envelopeDur;         /**< requested duration of last ENVELOPE command */
  float m_envelopeStart;        /**< initial value for last ENVELOPE command */
  float m_envelopeEnd;          /**< final value for last ENVELOPE command */
  const Curve* m_envelopeCurve; /**< curve to use for ENVELOPE command */

  bool m_pitchEnv = false; /**< Pitch envelope activated */
  Envelope m_pitchAdsr;    /**< Pitch envelope for SETPITCHADSR */
  int32_t m_pitchEnvRange; /**< Pitch delta for SETPITCHADSR (in cents) */

  float m_portamentoTime = -1.f; /**< time since last portamento invocation, -1 for no active portamento-glide */
  int32_t m_portamentoTarget;    /**< destination pitch for latest portamento invocation */

  int32_t m_pitchSweep1 = 0;      /**< Current value of PITCHSWEEP1 controller (in cents) */
  int32_t m_pitchSweep2 = 0;      /**< Current value of PITCHSWEEP2 controller (in cents) */
  int16_t m_pitchSweep1Add = 0;   /**< Value to add to PITCHSWEEP1 controller each cycle */
  int16_t m_pitchSweep2Add = 0;   /**< Value to add to PITCHSWEEP2 controller each cycle */
  uint8_t m_pitchSweep1Times = 0; /**< Remaining times to advance PITCHSWEEP1 controller */
  uint8_t m_pitchSweep2Times = 0; /**< Remaining times to advance PITCHSWEEP2 controller */
  uint8_t m_pitchSweep1It = 0;    /**< Current iteration of PITCHSWEEP1 controller */
  uint8_t m_pitchSweep2It = 0;    /**< Current iteration of PITCHSWEEP2 controller */

  std::queue<Panning> m_panningQueue;  /**< Queue of PANNING commands */
  std::queue<Panning> m_spanningQueue; /**< Queue of SPANNING commands */

  float m_vibratoTime = -1.f;     /**< time since last VIBRATO command, -1 for no active vibrato */
  int32_t m_vibratoLevel = 0;     /**< scale of vibrato effect (in cents) */
  int32_t m_vibratoModLevel = 0;  /**< scale of vibrato mod-wheel influence (in cents) */
  float m_vibratoPeriod = 0.f;    /**< vibrato wave period-time, 0.f will disable vibrato */
  bool m_vibratoModWheel = false; /**< vibrato scaled with mod-wheel if set */

  float m_tremoloScale = 0.f;    /**< minimum volume factor produced via LFO */
  float m_tremoloModScale = 0.f; /**< minimum volume factor produced via LFO, scaled via mod wheel */

  std::array<float, 2> m_lfoPeriods{};      /**< time-periods for LFO1 and LFO2 */
  std::unique_ptr<int8_t[]> m_ctrlValsSelf; /**< Self-owned MIDI Controller values */
  int8_t* m_extCtrlVals = nullptr;          /**< MIDI Controller values (external storage) */

  uint16_t m_rpn = 0; /**< Current RPN (only pitch-range 0x0000 supported) */

  void _destroy();
  bool _checkSamplePos(bool& looped);
  void _doKeyOff();
  void _macroKeyOff();
  void _macroSampleEnd();
  void _procSamplePre(int16_t& samp);
  VolumeCache m_masterCache;
  template <typename T>
  T _procSampleMaster(double time, T samp);
  VolumeCache m_auxACache;
  template <typename T>
  T _procSampleAuxA(double time, T samp);
  VolumeCache m_auxBCache;
  template <typename T>
  T _procSampleAuxB(double time, T samp);
  void _setTotalPitch(int32_t cents, bool slew);
  bool _isRecursivelyDead();
  void _bringOutYourDead();
  static uint32_t _GetBlockSampleCount(SampleFormat fmt);
  ObjToken<Voice> _findVoice(int vid, ObjToken<Voice> thisPtr);
  std::unique_ptr<int8_t[]>& _ensureCtrlVals();

  std::list<ObjToken<Voice>>::iterator _allocateVoice(double sampleRate, bool dynamicPitch);
  std::list<ObjToken<Voice>>::iterator _destroyVoice(std::list<ObjToken<Voice>>::iterator it);

  bool _loadSoundMacro(SoundMacroId id, const SoundMacro* macroData, int macroStep, double ticksPerSec, uint8_t midiKey,
                       uint8_t midiVel, uint8_t midiMod, bool pushPc = false);
  bool _loadKeymap(const Keymap* keymap, double ticksPerSec, uint8_t midiKey, uint8_t midiVel, uint8_t midiMod,
                   bool pushPc = false);
  bool _loadLayer(const std::vector<LayerMapping>& layer, double ticksPerSec, uint8_t midiKey, uint8_t midiVel,
                  uint8_t midiMod, bool pushPc = false);
  ObjToken<Voice> _startChildMacro(ObjectId macroId, int macroStep, double ticksPerSec, uint8_t midiKey,
                                   uint8_t midiVel, uint8_t midiMod, bool pushPc = false);

  std::array<float, 8> _panLaw(float frontPan, float backPan, float totalSpan) const;
  void _setPan(float pan);
  void _setSurroundPan(float span);
  void _setChannelCoefs(const std::array<float, 8>& coefs);
  void _setPitchWheel(float pitchWheel);
  void _notifyCtrlChange(uint8_t ctrl, int8_t val);

public:
  ~Voice() override;
  Voice(Engine& engine, const AudioGroup& group, GroupId groupId, int vid, bool emitter, ObjToken<Studio> studio);
  Voice(Engine& engine, const AudioGroup& group, GroupId groupId, ObjectId oid, int vid, bool emitter,
        ObjToken<Studio> studio);

  /** Called before each supplyAudio invocation to prepare voice
   *  backend for possible parameter updates */
  void preSupplyAudio(double dt);

  /** Request specified count of audio frames (samples) from voice,
   *  internally advancing the voice stream */
  size_t supplyAudio(size_t frames, int16_t* data);

  /** Called three times after resampling supplyAudio output, voice should
   *  perform volume processing / send routing for each aux bus and master */
  void routeAudio(size_t frames, double dt, int busId, int16_t* in, int16_t* out);
  void routeAudio(size_t frames, double dt, int busId, int32_t* in, int32_t* out);
  void routeAudio(size_t frames, double dt, int busId, float* in, float* out);

  /** Obtain pointer to Voice's Studio */
  ObjToken<Studio> getStudio() { return m_studio; }

  /** Get current state of voice */
  VoiceState state() const { return m_voxState; }

  /** Get VoiceId of this voice (unique to all currently-playing voices) */
  int vid() const { return m_vid; }

  /** Get max VoiceId of this voice and any contained children */
  int maxVid() const;

  /** Allocate parallel macro and tie to voice for possible emitter influence */
  ObjToken<Voice> startChildMacro(int8_t addNote, ObjectId macroId, int macroStep);

  /** Load specified SoundMacro Object from within group into voice */
  bool loadMacroObject(SoundMacroId macroId, int macroStep, double ticksPerSec, uint8_t midiKey, uint8_t midiVel,
                       uint8_t midiMod, bool pushPc = false);

  /** Load specified SoundMacro Object into voice */
  bool loadMacroObject(const SoundMacro* macro, int macroStep, double ticksPerSec, uint8_t midiKey, uint8_t midiVel,
                       uint8_t midiMod, bool pushPc = false);

  /** Load specified song page object (Keymap/Layer) from within group into voice */
  bool loadPageObject(ObjectId objectId, double ticksPerSec, uint8_t midiKey, uint8_t midiVel, uint8_t midiMod);

  /** Signals voice to begin fade-out (or defer if sustained), eventually reaching silence */
  void keyOff();

  /** Sends numeric message to voice and all siblings */
  void message(int32_t val);

  /** Start playing specified sample from within group, optionally by sample offset */
  void startSample(SampleId sampId, int32_t offset);

  /** Stop playing current sample */
  void stopSample();

  /** Set current voice volume immediately */
  void setVolume(float vol);

  /** Set current voice panning immediately */
  void setPan(float pan);

  /** Set current voice surround-panning immediately */
  void setSurroundPan(float span);

  /** Set current voice channel coefficients immediately */
  void setChannelCoefs(const std::array<float, 8>& coefs);

  /** Start volume envelope to specified level */
  void startEnvelope(double dur, float vol, const Curve* envCurve);

  /** Start volume envelope from zero to current level */
  void startFadeIn(double dur, float vol, const Curve* envCurve);

  /** Start pan envelope to specified position */
  void startPanning(double dur, uint8_t panPos, int8_t panWidth);

  /** Start span envelope to specified position */
  void startSpanning(double dur, uint8_t spanPos, int8_t spanWidth);

  /** Set voice relative-pitch in cents */
  void setPitchKey(int32_t cents);

  /** Set sustain status within voice; clearing will trigger a deferred keyoff */
  void setPedal(bool pedal);

  /** Set doppler factor for voice */
  void setDoppler(float doppler);

  /** Set vibrato parameters for voice */
  void setVibrato(int32_t level, bool modScale, float period);

  /** Configure modwheel influence range over vibrato */
  void setMod2VibratoRange(int32_t modLevel);

  /** Setup tremolo parameters for voice */
  void setTremolo(float tremoloScale, float tremoloModScale);

  /** Setup LFO1 for voice */
  void setLFO1Period(float period) { m_lfoPeriods[0] = period; }

  /** Setup LFO2 for voice */
  void setLFO2Period(float period) { m_lfoPeriods[1] = period; }

  /** Setup pitch sweep controller 1 */
  void setPitchSweep1(uint8_t times, int16_t add);

  /** Setup pitch sweep controller 2 */
  void setPitchSweep2(uint8_t times, int16_t add);

  /** Set reverb mix for voice */
  void setReverbVol(float rvol);

  /** Set AuxB volume for voice */
  void setAuxBVol(float bvol);

  /** Set envelope for voice */
  void setAdsr(ObjectId adsrId, bool dls);

  /** Set pitch in absolute hertz */
  void setPitchFrequency(uint32_t hz, uint16_t fine);

  /** Set pitch envelope */
  void setPitchAdsr(ObjectId adsrId, int32_t cents);

  /** Set pitchwheel value for use with controller */
  void setPitchWheel(float pitchWheel);

  /** Set effective pitch range via the pitchwheel controller */
  void setPitchWheelRange(int8_t up, int8_t down);

  /** Set aftertouch */
  void setAftertouch(uint8_t aftertouch);

  /** Assign voice to keygroup for coordinated mass-silencing */
  void setKeygroup(uint8_t kg) { m_keygroup = kg; }

  /** Get note played on voice */
  uint8_t getLastNote() const { return m_state.m_initKey; }

  /** Do portamento glide; returns `false` if portamento disabled */
  bool doPortamento(uint8_t newNote);

  /** Get MIDI Controller value on voice */
  int8_t getCtrlValue(uint8_t ctrl) const {
    if (!m_extCtrlVals) {
      if (m_ctrlValsSelf)
        return m_ctrlValsSelf[ctrl];
      return 0;
    }
    return m_extCtrlVals[ctrl];
  }

  /** Set MIDI Controller value on voice */
  void setCtrlValue(uint8_t ctrl, int8_t val) {
    if (!m_extCtrlVals) {
      std::unique_ptr<int8_t[]>& vals = _ensureCtrlVals();
      vals[ctrl] = val;
    } else
      m_extCtrlVals[ctrl] = val;
    _notifyCtrlChange(ctrl, val);
  }

  /** 'install' external MIDI controller storage */
  void installCtrlValues(int8_t* cvs) {
    m_ctrlValsSelf.reset();
    m_extCtrlVals = cvs;
    for (ObjToken<Voice>& vox : m_childVoices)
      vox->installCtrlValues(cvs);
  }

  /** Get MIDI pitch wheel value on voice */
  float getPitchWheel() const { return m_curPitchWheel; }

  /** Get MIDI aftertouch value on voice */
  int8_t getAftertouch() const { return m_curAftertouch; }

  /** Get count of all voices in hierarchy, including this one */
  size_t getTotalVoices() const;

  /** Get latest decoded sample index */
  uint32_t getSamplePos() const { return m_curSamplePos; }

  /** Recursively mark voice as dead for Engine to deallocate on next cycle */
  void kill();
};
} // namespace amuse

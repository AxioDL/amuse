#ifndef __AMUSE_VOICE_HPP__
#define __AMUSE_VOICE_HPP__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include <list>
#include "SoundMacroState.hpp"
#include "Entity.hpp"
#include "AudioGroupSampleDirectory.hpp"
#include "AudioGroup.hpp"
#include "Envelope.hpp"

namespace amuse
{
class IBackendVoice;
class Submix;
struct Keymap;
struct LayerMapping;

/** State of voice over lifetime */
enum class VoiceState
{
    Playing, /**< SoundMacro actively executing, not in KeyOff */
    KeyOff, /**< KeyOff event issued, macro beginning fade-out */
    Dead /**< Default state, causes Engine to remove voice at end of pump cycle */
};

/** Individual source of audio */
class Voice : public Entity
{
    friend class Engine;
    friend class Sequencer;
    friend class SoundMacroState;
    friend class Envelope;
    int m_vid; /**< VoiceID of this voice instance */
    bool m_emitter; /**< Voice is part of an Emitter */
    Submix* m_submix = nullptr; /**< Submix this voice outputs to (or NULL for the main output mix) */

    std::unique_ptr<IBackendVoice> m_backendVoice; /**< Handle to client-implemented backend voice */
    SoundMacroState m_state; /**< State container for SoundMacro playback */
    SoundMacroState::EventTrap m_keyoffTrap; /**< Trap for keyoff (SoundMacro overrides default envelope behavior) */
    SoundMacroState::EventTrap m_sampleEndTrap; /**< Trap for sampleend (SoundMacro overrides voice removal) */
    SoundMacroState::EventTrap m_messageTrap; /**< Trap for messages sent from other SoundMacros */
    std::list<int32_t> m_messageQueue; /**< Messages pending processing for SoundMacros in this voice */
    std::list<std::shared_ptr<Voice>> m_childVoices; /**< Child voices for PLAYMACRO usage */
    uint8_t m_keygroup = 0; /**< Keygroup voice is a member of */

    enum class SampleFormat : uint8_t
    {
        DSP,      /**< GCN DSP-ucode ADPCM (very common for GameCube games) */
        DSP_DRUM, /**< GCN DSP-ucode ADPCM (seems to be set into drum samples for expanding their amplitude appropriately) */
        PCM,      /**< Big-endian PCM found in MusyX2 demo GM instruments */
        N64,      /**< 2-stage VADPCM coding with SAMP-embedded codebooks */
        PCM_PC    /**< Little-endian PCM found in PC Rogue Squadron (actually enum 0 which conflicts with DSP-ADPCM) */
    };
    const Sample* m_curSample = nullptr; /**< Current sample entry playing */
    const unsigned char* m_curSampleData = nullptr; /**< Current sample data playing */
    SampleFormat m_curFormat; /**< Current sample format playing */
    uint32_t m_curSamplePos = 0; /**< Current sample position */
    uint32_t m_lastSamplePos = 0; /**< Last sample position (or last loop sample) */
    int16_t m_prev1 = 0; /**< DSPADPCM prev sample */
    int16_t m_prev2 = 0; /**< DSPADPCM prev-prev sample */
    double m_sampleRate = 32000.0; /**< Current sample rate computed from relative sample key or SETPITCH */
    double m_voiceTime = 0.0; /**< Current seconds of voice playback (per-sample resolution) */
    uint64_t m_voiceSamples = 0; /**< Count of samples processed over voice's lifetime */
    float m_lastLevel = 0.f; /**< Last computed level ([0,1] mapped to [-10,0] clamped decibels) */
    float m_nextLevel = 0.f; /**< Next computed level used for lerp-mode amplitude */

    VoiceState m_voxState = VoiceState::Dead; /**< Current high-level state of voice */
    bool m_sustained = false; /**< Sustain pedal pressed for this voice */
    bool m_sustainKeyOff = false; /**< Keyoff event occured while sustained */
    uint8_t m_curAftertouch = 0; /**< Aftertouch value (key pressure when 'bottoming out') */

    float m_targetUserVol = 1.f; /**< Target user volume of voice (slewed to prevent audible aliasing) */
    float m_curUserVol = 1.f; /**< Current user volume of voice */
    float m_curVol = 1.f; /**< Current volume of voice */
    float m_curReverbVol = 0.f; /**< Current reverb volume of voice */
    float m_userPan = 0.f; /**< User pan of voice */
    float m_curPan = 0.f; /**< Current pan of voice */
    float m_userSpan = 0.f; /**< User span of voice */
    float m_curSpan = 0.f; /**< Current surround pan of voice */
    float m_curPitchWheel = 0.f; /**< Current normalized wheel value for control */
    int32_t m_pitchWheelUp = 600; /**< Up range for pitchwheel control in cents */
    int32_t m_pitchWheelDown = 600; /**< Down range for pitchwheel control in cents */
    int32_t m_pitchWheelVal = 0; /**< Current resolved pitchwheel delta for control */
    int32_t m_curPitch; /**< Current base pitch in cents */
    bool m_pitchDirty = true; /**< m_curPitch has been updated and needs sending to voice */

    Envelope m_volAdsr; /**< Volume envelope */
    double m_envelopeTime = -1.f; /**< time since last ENVELOPE command, -1 for no active volume-sweep */
    double m_envelopeDur; /**< requested duration of last ENVELOPE command */
    float m_envelopeStart; /**< initial value for last ENVELOPE command */
    float m_envelopeEnd; /**< final value for last ENVELOPE command */
    const Curve* m_envelopeCurve; /**< curve to use for ENVELOPE command */

    bool m_pitchEnv = false; /**< Pitch envelope activated */
    Envelope m_pitchAdsr; /**< Pitch envelope for SETPITCHADSR */
    int32_t m_pitchEnvRange; /**< Pitch delta for SETPITCHADSR (in cents) */

    float m_portamentoTime = -1.f; /**< time since last portamento invocation, -1 for no active portamento-glide */
    int32_t m_portamentoTarget; /**< destination pitch for latest portamento invocation */

    uint32_t m_pitchSweep1 = 0; /**< Current value of PITCHSWEEP1 controller (in cents) */
    uint32_t m_pitchSweep2 = 0; /**< Current value of PITCHSWEEP2 controller (in cents) */
    int16_t m_pitchSweep1Add = 0; /**< Value to add to PITCHSWEEP1 controller each cycle */
    int16_t m_pitchSweep2Add = 0; /**< Value to add to PITCHSWEEP2 controller each cycle */
    uint8_t m_pitchSweep1Times = 0; /**< Remaining times to advance PITCHSWEEP1 controller */
    uint8_t m_pitchSweep2Times = 0; /**< Remaining times to advance PITCHSWEEP2 controller */
    uint8_t m_pitchSweep1It = 0; /**< Current iteration of PITCHSWEEP1 controller */
    uint8_t m_pitchSweep2It = 0; /**< Current iteration of PITCHSWEEP2 controller */

    float m_panningTime = -1.f; /**< time since last PANNING command, -1 for no active pan-sweep */
    float m_panningDur; /**< requested duration of last PANNING command */
    uint8_t m_panPos; /**< initial pan value of last PANNING command */
    int8_t m_panWidth; /**< delta pan value to target of last PANNING command */

    float m_spanningTime = -1.f; /**< time since last SPANNING command, -1 for no active span-sweep */
    float m_spanningDur; /**< requested duration of last SPANNING command */
    uint8_t m_spanPos; /**< initial pan value of last SPANNING command */
    int8_t m_spanWidth; /**< delta pan value to target of last SPANNING command */

    int32_t m_vibratoLevel = 0; /**< scale of vibrato effect (in cents) */
    int32_t m_vibratoModLevel = 0; /**< scale of vibrato mod-wheel influence (in cents) */
    float m_vibratoPeriod = 0.f; /**< vibrato wave period-time, 0.f will disable vibrato */
    bool m_vibratoModWheel = false; /**< vibrato scaled with mod-wheel if set */

    float m_tremoloScale = 0.f; /**< minimum volume factor produced via LFO */
    float m_tremoloModScale = 0.f; /**< minimum volume factor produced via LFO, scaled via mod wheel */

    float m_lfoPeriods[2] = {}; /**< time-periods for LFO1 and LFO2 */
    std::unique_ptr<int8_t[]> m_ctrlValsSelf; /**< Self-owned MIDI Controller values */
    int8_t* m_extCtrlVals = nullptr; /**< MIDI Controller values (external storage) */

    void _destroy();
    bool _checkSamplePos(bool& looped);
    void _doKeyOff();
    void _macroKeyOff();
    void _macroSampleEnd();
    bool _advanceSample(int16_t& samp, int32_t& curPitch);
    void _setTotalPitch(int32_t cents, bool slew);
    bool _isRecursivelyDead();
    void _bringOutYourDead();
    static uint32_t _GetBlockSampleCount(SampleFormat fmt);
    std::shared_ptr<Voice> _findVoice(int vid, std::weak_ptr<Voice> thisPtr);
    std::unique_ptr<int8_t[]>& _ensureCtrlVals();

    std::list<std::shared_ptr<Voice>>::iterator _allocateVoice(double sampleRate, bool dynamicPitch);
    std::list<std::shared_ptr<Voice>>::iterator _destroyVoice(std::list<std::shared_ptr<Voice>>::iterator it);

    bool _loadSoundMacro(const unsigned char* macroData, int macroStep, double ticksPerSec,
                         uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc=false);
    bool _loadKeymap(const Keymap* keymap, int macroStep, double ticksPerSec,
                     uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc=false);
    bool _loadLayer(const std::vector<const LayerMapping*>& layer, int macroStep, double ticksPerSec,
                    uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc=false);
    std::shared_ptr<Voice> _startChildMacro(ObjectId macroId, int macroStep, double ticksPerSec,
                                            uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc=false);

    void _setPan(float pan);
    void _setSurroundPan(float span);
    void _setPitchWheel(float pitchWheel);
    void _notifyCtrlChange(uint8_t ctrl, int8_t val);
public:
    ~Voice();
    Voice(Engine& engine, const AudioGroup& group, int groupId, int vid, bool emitter, Submix* smx);
    Voice(Engine& engine, const AudioGroup& group, int groupId, ObjectId oid, int vid, bool emitter, Submix* smx);

    /** Request specified count of audio frames (samples) from voice,
     *  internally advancing the voice stream */
    size_t supplyAudio(size_t frames, int16_t* data);

    /** Obtain pointer to Voice's Submix */
    Submix* getSubmix() {return m_submix;}

    /** Get current state of voice */
    VoiceState state() const {return m_voxState;}

    /** Get VoiceId of this voice (unique to all currently-playing voices) */
    int vid() const {return m_vid;}

    /** Get max VoiceId of this voice and any contained children */
    int maxVid() const;

    /** Allocate parallel macro and tie to voice for possible emitter influence */
    std::shared_ptr<Voice> startChildMacro(int8_t addNote, ObjectId macroId, int macroStep);

    /** Load specified Sound Object from within group into voice */
    bool loadSoundObject(ObjectId objectId, int macroStep, double ticksPerSec,
                         uint8_t midiKey, uint8_t midiVel, uint8_t midiMod,
                         bool pushPc=false);

    /** Signals voice to begin fade-out (or defer if sustained), eventually reaching silence */
    void keyOff();

    /** Sends numeric message to voice and all siblings */
    void message(int32_t val);

    /** Start playing specified sample from within group, optionally by sample offset */
    void startSample(int16_t sampId, int32_t offset);

    /** Stop playing current sample */
    void stopSample();

    /** Set current voice volume immediately */
    void setVolume(float vol);

    /** Set current voice panning immediately */
    void setPan(float pan);

    /** Set current voice surround-panning immediately */
    void setSurroundPan(float span);

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
    void setVibrato(int32_t level, int32_t modLevel, float period);

    /** Configure modwheel influence range over vibrato */
    void setMod2VibratoRange(int32_t modLevel);

    /** Setup tremolo parameters for voice */
    void setTremolo(float tremoloScale, float tremoloModScale);

    /** Setup LFO1 for voice */
    void setLFO1Period(float period) {m_lfoPeriods[0] = period;}

    /** Setup LFO2 for voice */
    void setLFO2Period(float period) {m_lfoPeriods[1] = period;}

    /** Setup pitch sweep controller 1 */
    void setPitchSweep1(uint8_t times, int16_t add);

    /** Setup pitch sweep controller 2 */
    void setPitchSweep2(uint8_t times, int16_t add);

    /** Set reverb mix for voice */
    void setReverbVol(float rvol);

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
    void setKeygroup(uint8_t kg) {m_keygroup = kg;}

    /** Get note played on voice */
    uint8_t getLastNote() const {return m_state.m_initKey;}

    /** Do portamento glide; returns `false` if portamento disabled */
    bool doPortamento(uint8_t newNote);

    /** Get MIDI Controller value on voice */
    int8_t getCtrlValue(uint8_t ctrl) const
    {
        if (!m_extCtrlVals)
        {
            if (m_ctrlValsSelf)
                m_ctrlValsSelf[ctrl];
            return 0;
        }
        return m_extCtrlVals[ctrl];
    }

    /** Set MIDI Controller value on voice */
    void setCtrlValue(uint8_t ctrl, int8_t val)
    {
        if (!m_extCtrlVals)
        {
            std::unique_ptr<int8_t[]>& vals = _ensureCtrlVals();
            vals[ctrl] = val;
        }
        else
            m_extCtrlVals[ctrl] = val;
        _notifyCtrlChange(ctrl, val);
    }

    /** 'install' external MIDI controller storage */
    void installCtrlValues(int8_t* cvs)
    {
        m_ctrlValsSelf.reset();
        m_extCtrlVals = cvs;
    }

    /** Get MIDI pitch wheel value on voice */
    float getPitchWheel() const {return m_curPitchWheel;}

    /** Get MIDI aftertouch value on voice */
    int8_t getAftertouch() const {return m_curAftertouch;}

    /** Get count of all voices in hierarchy, including this one */
    size_t getTotalVoices() const;

    /** Recursively mark voice as dead for Engine to deallocate on next cycle */
    void kill();

};

}

#endif // __AMUSE_VOICE_HPP__

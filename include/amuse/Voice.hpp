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

/** State of voice over lifetime */
enum class VoiceState
{
    Playing,
    KeyOff,
    Finished
};

/** Individual source of audio */
class Voice : public Entity
{
    friend class Engine;
    friend class SoundMacroState;
    int m_vid; /**< VoiceID of this voice instance */
    bool m_emitter; /**< Voice is part of an Emitter */
    Submix* m_submix = nullptr; /**< Submix this voice outputs to (or NULL for the main output mix) */
    std::list<Voice>::iterator m_engineIt; /**< Iterator to self within Engine's list for quick deletion */

    std::unique_ptr<IBackendVoice> m_backendVoice; /**< Handle to client-implemented backend voice */
    SoundMacroState m_state; /**< State container for SoundMacro playback */
    std::list<Voice> m_childVoices; /**< Child voices for PLAYMACRO usage */
    uint8_t m_keygroup = 0; /**< Keygroup voice is a member of */

    const Sample* m_curSample = nullptr; /**< Current sample entry playing */
    const unsigned char* m_curSampleData = nullptr; /**< Current sample data playing */
    uint32_t m_curSamplePos = 0; /**< Current sample position */
    uint32_t m_lastSamplePos = 0; /**< Last sample position (or last loop sample) */
    int16_t m_prev1 = 0; /**< DSPADPCM prev sample */
    int16_t m_prev2 = 0; /**< DSPADPCM prev-prev sample */
    double m_sampleRate; /**< Current sample rate computed from relative sample key or SETPITCH */
    double m_voiceTime; /**< Current seconds of voice playback (per-sample resolution) */

    VoiceState m_voxState = VoiceState::Finished; /**< Current high-level state of voice */
    bool m_sustained = false; /**< Sustain pedal pressed for this voice */
    bool m_sustainKeyOff = false; /**< Keyoff event occured while sustained */

    float m_curVol; /**< Current volume of voice */
    float m_curReverbVol; /**< Current reverb volume of voice */
    float m_curPan; /**< Current pan of voice */
    float m_curSpan; /**< Current surround pan of voice */
    int32_t m_curPitch; /**< Current base pitch in cents */
    bool m_pitchDirty; /**< m_curPitch has been updated and needs sending to voice */

    Envelope m_volAdsr; /**< Volume envelope */
    double m_envelopeTime; /**< time since last ENVELOPE command, -1 for no active volume-sweep */
    double m_envelopeDur; /**< requested duration of last ENVELOPE command */
    float m_envelopeStart; /**< initial value for last ENVELOPE command */
    float m_envelopeEnd; /**< final value for last ENVELOPE command */
    const Curve* m_envelopeCurve; /**< curve to use for ENVELOPE command */

    bool m_pitchEnv = false; /**< Pitch envelope activated */
    Envelope m_pitchAdsr; /**< Pitch envelope for SETPITCHADSR */
    int32_t m_pitchEnvRange; /**< Pitch delta for SETPITCHADSR (in cents) */

    uint32_t m_pitchSweep1; /**< Current value of PITCHSWEEP1 controller (in cents) */
    uint32_t m_pitchSweep2; /**< Current value of PITCHSWEEP2 controller (in cents) */
    int16_t m_pitchSweep1Add; /**< Value to add to PITCHSWEEP1 controller each cycle */
    int16_t m_pitchSweep2Add; /**< Value to add to PITCHSWEEP2 controller each cycle */
    uint8_t m_pitchSweep1Times; /**< Remaining times to advance PITCHSWEEP1 controller */
    uint8_t m_pitchSweep2Times; /**< Remaining times to advance PITCHSWEEP2 controller */

    float m_panningTime; /**< time since last PANNING command, -1 for no active pan-sweep */
    float m_panningDur; /**< requested duration of last PANNING command */
    uint8_t m_panPos; /**< initial pan value of last PANNING command */
    uint8_t m_panWidth; /**< delta pan value to target of last PANNING command */

    float m_spanningTime; /**< time since last SPANNING command, -1 for no active span-sweep */
    float m_spanningDur; /**< requested duration of last SPANNING command */
    uint8_t m_spanPos; /**< initial pan value of last SPANNING command */
    uint8_t m_spanWidth; /**< delta pan value to target of last SPANNING command */

    int32_t m_vibratoLevel; /**< scale of vibrato effect (in cents) */
    int32_t m_vibratoModLevel; /**< scale of vibrato mod-wheel influence (in cents) */
    float m_vibratoPeriod; /**< vibrato wave period-time, 0.f will disable vibrato */
    bool m_vibratoModWheel; /**< vibrato scaled with mod-wheel if set */

    float m_tremoloScale; /**< minimum volume factor produced via LFO */
    float m_tremoloModScale; /**< minimum volume factor produced via LFO, scaled via mod wheel */

    float m_lfoPeriods[2]; /**< time-periods for LFO1 and LFO2 */

    void _destroy();
    void _reset();
    bool _checkSamplePos();
    void _doKeyOff();
    bool _advanceSample(int16_t& samp);
    void _setTotalPitch(int32_t cents);

    Voice* _allocateVoice(double sampleRate, bool dynamicPitch);
    std::list<Voice>::iterator _destroyVoice(Voice* voice);

public:
    Voice(Engine& engine, const AudioGroup& group, int vid, bool emitter, Submix* smx);
    Voice(Engine& engine, const AudioGroup& group, ObjectId oid, int vid, bool emitter, Submix* smx);

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
    Voice* startChildMacro(int8_t addNote, ObjectId macroId, int macroStep);

    /** Load specified SoundMacro ID of within group into voice */
    bool loadSoundMacro(ObjectId macroId, int macroStep, float ticksPerSec,
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
    void startPanning(double dur, uint8_t panPos, uint8_t panWidth);

    /** Start span envelope to specified position */
    void startSpanning(double dur, uint8_t spanPos, uint8_t spanWidth);

    /** Set voice relative-pitch in cents */
    void setPitchKey(int32_t cents);

    /** Set voice modulation */
    void setModulation(float mod);

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
    void setAdsr(ObjectId adsrId);

    /** Set pitch in absolute hertz */
    void setPitchFrequency(uint32_t hz, uint16_t fine);

    /** Set pitch envelope */
    void setPitchAdsr(ObjectId adsrId, int32_t cents);

    /** Set effective pitch range via the pitchwheel controller */
    void setPitchWheelRange(int8_t up, int8_t down);

    /** Assign voice to keygroup for coordinated mass-silencing */
    void setKeygroup(uint8_t kg) {m_keygroup = kg;}

    uint8_t getLastNote() const {return m_state.m_initKey;}
    int8_t getCtrlValue(uint8_t ctrl) const {}
    void setCtrlValue(uint8_t ctrl, int8_t val) {}
    int8_t getPitchWheel() const {}
    int8_t getModWheel() const {}
    int8_t getAftertouch() const {}

};

}

#endif // __AMUSE_VOICE_HPP__

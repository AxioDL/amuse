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
    int m_vid; /**< VoiceID of this voice instance */
    bool m_emitter; /**< Voice is part of an Emitter */
    Submix* m_submix = nullptr; /**< Submix this voice outputs to (or NULL for the main output mix) */
    std::list<Voice>::iterator m_engineIt; /**< Iterator to self within Engine's list for quick deletion */
    std::unique_ptr<IBackendVoice> m_backendVoice; /**< Handle to client-implemented backend voice */
    SoundMacroState m_state; /**< State container for SoundMacro playback */
    std::list<Voice> m_childVoices; /**< Child voices for PLAYMACRO usage */
    uint8_t m_lastNote = 0; /**< Last MIDI semitone played by voice */
    uint8_t m_keygroup = 0; /**< Keygroup voice is a member of */

    const Sample* m_curSample = nullptr; /**< Current sample entry playing */
    const unsigned char* m_curSampleData = nullptr; /**< Current sample data playing */
    uint32_t m_curSamplePos = 0; /**< Current sample position */
    uint32_t m_lastSamplePos = 0; /**< Last sample position (or last loop sample) */
    int16_t m_prev1 = 0; /**< DSPADPCM prev sample */
    int16_t m_prev2 = 0; /**< DSPADPCM prev-prev sample */

    VoiceState m_voxState = VoiceState::Finished; /**< Current high-level state of voice */
    bool m_sustained = false; /**< Sustain pedal pressed for this voice */
    bool m_sustainKeyOff = false; /**< Keyoff event occured while sustained */

    void _destroy();
    bool _checkSamplePos();
    void _doKeyOff();

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

    /** Allocate parallel macro and tie to voice for possible emitter influence */
    Voice* startChildMacro(int8_t addNote, ObjectId macroId, int macroStep);

    /** Load specified SoundMacro ID of within group into voice */
    bool loadSoundMacro(ObjectId macroId, int macroStep=0, bool pushPc=false);

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
    void setPanning(float pan);

    /** Set current voice surround-panning immediately */
    void setSurroundPanning(float span);

    /** Set voice relative-pitch in cents */
    void setPitchKey(int32_t cents);

    /** Set voice modulation */
    void setModulation(float mod);

    /** Set sustain status within voice; clearing will trigger a deferred keyoff */
    void setPedal(bool pedal);

    /** Set doppler factor for voice */
    void setDoppler(float doppler);

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

    uint8_t getLastNote() const {return m_lastNote;}
    int8_t getCtrlValue(uint8_t ctrl) const {}
    void setCtrlValue(uint8_t ctrl, int8_t val) {}
    int8_t getPitchWheel() const {}
    int8_t getModWheel() const {}
    int8_t getAftertouch() const {}

};

}

#endif // __AMUSE_VOICE_HPP__

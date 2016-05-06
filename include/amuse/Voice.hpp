#ifndef __AMUSE_VOICE_HPP__
#define __AMUSE_VOICE_HPP__

#include <stdint.h>
#include <stdlib.h>
#include <memory>
#include "SoundMacroState.hpp"
#include "Entity.hpp"

namespace amuse
{
class IBackendVoice;

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
    int m_vid;
    bool m_emitter;
    std::unique_ptr<IBackendVoice> m_backendVoice;
    SoundMacroState m_state;
    Voice* m_sibling = nullptr;
    uint8_t m_lastNote = 0;
public:
    Voice(Engine& engine, const AudioGroup& group, int vid, bool emitter);
    Voice(Engine& engine, const AudioGroup& group, ObjectId oid, int vid, bool emitter);

    /** Request specified count of audio frames (samples) from voice,
     *  internally advancing the voice stream */
    size_t supplyAudio(size_t frames, int16_t* data);

    /** Get current state of voice */
    VoiceState state() const;

    /** Get VoiceId of this voice (unique to all currently-playing voices) */
    int vid() const {return m_vid;}

    /** Allocate parallel macro and tie to voice for possible emitter influence */
    Voice* startSiblingMacro(int8_t addNote, ObjectId macroId, int macroStep);

    /** Load specified SoundMacro ID of within group into voice */
    bool loadSoundMacro(ObjectId macroId, int macroStep=0);

    /** Signals voice to begin fade-out, eventually reaching silence */
    void keyOff();

    void startSample(int16_t sampId, int32_t offset);
    void stopSample();
    void setVolume(float vol);
    void setPanning(float pan);
    void setSurroundPanning(float span);
    void setPitchKey(int32_t cents);
    void setModulation(float mod);
    void setPedal(bool pedal);
    void setDoppler(float doppler);
    void setReverbVol(float rvol);
    void setAdsr(ObjectId adsrId);
    void setPitchFrequency(uint32_t hz, uint16_t fine);
    void setPitchAdsr(ObjectId adsrId, int32_t cents);

    uint8_t getLastNote() const {return m_lastNote;}
    int8_t getCtrlValue(uint8_t ctrl) const;
    int8_t getPitchWheel() const;
    int8_t getModWheel() const;
    int8_t getAftertouch() const;

};

}

#endif // __AMUSE_VOICE_HPP__

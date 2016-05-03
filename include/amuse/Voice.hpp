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
    std::unique_ptr<IBackendVoice> m_backendVoice;
    SoundMacroState m_state;
public:
    Voice(Engine& engine, int groupId);

    /** Request specified count of audio frames (samples) from voice,
     *  internally advancing the voice stream */
    size_t supplyAudio(size_t frames, int16_t* data);

    /** Get current state of voice */
    VoiceState state() const;

    /** Signals voice to begin fade-out, eventually reaching silence */
    void keyOff();

    void setVolume(float vol);
    void setPanning(float pan);
    void setSurroundPanning(float span);
    void setPitchBend(float pitch);
    void setModulation(float mod);
    void setPedal(bool pedal);
    void setDoppler(float doppler);
    void setReverbVol(float rvol);

};

}

#endif // __AMUSE_VOICE_HPP__

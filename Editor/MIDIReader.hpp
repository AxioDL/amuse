#ifndef AMUSE_MIDI_READER_HPP
#define AMUSE_MIDI_READER_HPP

#include "amuse/Voice.hpp"
#include "amuse/BooBackend.hpp"
#include "amuse/Common.hpp"
#include <unordered_set>

class MIDIReader : public amuse::BooBackendMIDIReader
{
    std::unordered_map<uint8_t, amuse::ObjToken<amuse::Voice>> m_chanVoxs;
    std::unordered_set<amuse::ObjToken<amuse::Voice>> m_keyoffVoxs;
    amuse::ObjToken<amuse::Voice> m_lastVoice;
public:
    MIDIReader(amuse::Engine& engine, const char* name, bool useLock);
    boo::IMIDIIn* getMidiIn() const { return m_midiIn.get(); }

    void noteOff(uint8_t chan, uint8_t key, uint8_t velocity);
    void noteOn(uint8_t chan, uint8_t key, uint8_t velocity);
    void notePressure(uint8_t chan, uint8_t key, uint8_t pressure);
    void controlChange(uint8_t chan, uint8_t control, uint8_t value);
    void programChange(uint8_t chan, uint8_t program);
    void channelPressure(uint8_t chan, uint8_t pressure);
    void pitchBend(uint8_t chan, int16_t pitch);

    void allSoundOff(uint8_t chan);
    void resetAllControllers(uint8_t chan);
    void localControl(uint8_t chan, bool on);
    void allNotesOff(uint8_t chan);
    void omniMode(uint8_t chan, bool on);
    void polyMode(uint8_t chan, bool on);

    void sysex(const void* data, size_t len);
    void timeCodeQuarterFrame(uint8_t message, uint8_t value);
    void songPositionPointer(uint16_t pointer);
    void songSelect(uint8_t song);
    void tuneRequest();

    void startSeq();
    void continueSeq();
    void stopSeq();

    void reset();
};

class VoiceAllocator : public amuse::BooBackendVoiceAllocator
{
public:
    VoiceAllocator(boo::IAudioVoiceEngine& booEngine);
    std::unique_ptr<amuse::IMIDIReader> allocateMIDIReader(amuse::Engine& engine, const char* name = nullptr);
};

#endif // AMUSE_MIDI_READER_HPP

#pragma once

#include <cstdint>
#include <memory>
#include <unordered_map>
#include <unordered_set>

#include <amuse/BooBackend.hpp>
#include <amuse/Common.hpp>
#include <amuse/Voice.hpp>

namespace amuse {
class Engine;
}

class MIDIReader : public amuse::BooBackendMIDIReader {
  std::unordered_map<uint8_t, amuse::ObjToken<amuse::Voice>> m_chanVoxs;
  std::unordered_set<amuse::ObjToken<amuse::Voice>> m_keyoffVoxs;
  amuse::ObjToken<amuse::Voice> m_lastVoice;

public:
  MIDIReader(amuse::Engine& engine, bool useLock);

  void noteOff(uint8_t chan, uint8_t key, uint8_t velocity) override;
  void noteOn(uint8_t chan, uint8_t key, uint8_t velocity) override;
  void notePressure(uint8_t chan, uint8_t key, uint8_t pressure) override;
  void controlChange(uint8_t chan, uint8_t control, uint8_t value) override;
  void programChange(uint8_t chan, uint8_t program) override;
  void channelPressure(uint8_t chan, uint8_t pressure) override;
  void pitchBend(uint8_t chan, int16_t pitch) override;

  void allSoundOff(uint8_t chan) override;
  void resetAllControllers(uint8_t chan) override;
  void localControl(uint8_t chan, bool on) override;
  void allNotesOff(uint8_t chan) override;
  void omniMode(uint8_t chan, bool on) override;
  void polyMode(uint8_t chan, bool on) override;

  void sysex(const void* data, size_t len) override;
  void timeCodeQuarterFrame(uint8_t message, uint8_t value) override;
  void songPositionPointer(uint16_t pointer) override;
  void songSelect(uint8_t song) override;
  void tuneRequest() override;

  void startSeq() override;
  void continueSeq() override;
  void stopSeq() override;

  void reset() override;
};

class VoiceAllocator : public amuse::BooBackendVoiceAllocator {
public:
  VoiceAllocator(boo::IAudioVoiceEngine& booEngine);
  std::unique_ptr<amuse::IMIDIReader> allocateMIDIReader(amuse::Engine& engine) override;
};

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <list>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <vector>

#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendSubmix.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"

#include "boo2/audiodev/IAudioSubmix.hpp"
#include "boo2/audiodev/IAudioVoiceEngine.hpp"
#include "boo2/audiodev/IMIDIReader.hpp"
#include "boo2/audiodev/MIDIDecoder.hpp"

namespace amuse {

/** Backend voice implementation for boo mixer */
class BooBackendVoice : public IBackendVoice {
  friend class BooBackendVoiceAllocator;
  Voice& m_clientVox;
  struct VoiceCallback : boo2::IAudioVoiceCallback {
    BooBackendVoice& m_parent;
    void preSupplyAudio(boo2::IAudioVoice& voice, double dt) override;
    size_t supplyAudio(boo2::IAudioVoice& voice, size_t frames, int16_t* data) override;
    void routeAudio(size_t frames, size_t channels, double dt, int busId, int16_t* in, int16_t* out) override;
    void routeAudio(size_t frames, size_t channels, double dt, int busId, int32_t* in, int32_t* out) override;
    void routeAudio(size_t frames, size_t channels, double dt, int busId, float* in, float* out) override;
    VoiceCallback(BooBackendVoice& parent) : m_parent(parent) {}
  } m_cb;
  boo2::ObjToken<boo2::IAudioVoice> m_booVoice;

public:
  BooBackendVoice(boo2::IAudioVoiceEngine& engine, Voice& clientVox, double sampleRate, bool dynamicPitch);
  void resetSampleRate(double sampleRate) override;

  void resetChannelLevels() override;
  void setChannelLevels(IBackendSubmix* submix, const std::array<float, 8>& coefs, bool slew) override;
  void setPitchRatio(double ratio, bool slew) override;
  void start() override;
  void stop() override;
};

/** Backend submix implementation for boo mixer */
class BooBackendSubmix : public IBackendSubmix {
  friend class BooBackendVoiceAllocator;
  friend class BooBackendVoice;
  Submix& m_clientSmx;
  struct SubmixCallback : boo2::IAudioSubmixCallback {
    BooBackendSubmix& m_parent;
    bool canApplyEffect() const override;
    void applyEffect(float* audio, size_t frameCount, const boo2::ChannelMap& chanMap, double sampleRate) const override;
    void resetOutputSampleRate(double sampleRate) override;
    SubmixCallback(BooBackendSubmix& parent) : m_parent(parent) {}
  } m_cb;
  boo2::ObjToken<boo2::IAudioSubmix> m_booSubmix;

public:
  BooBackendSubmix(boo2::IAudioVoiceEngine& engine, Submix& clientSmx, bool mainOut, int busId);
  void setSendLevel(IBackendSubmix* submix, float level, bool slew) override;
  double getSampleRate() const override;
};

/** Backend MIDI event reader for controlling sequencer with external hardware / software */
class BooBackendMIDIReader : public IMIDIReader, public boo2::IMIDIReader {
  friend class BooBackendVoiceAllocator;

protected:
  Engine& m_engine;
  std::unordered_map<std::string, std::unique_ptr<boo2::IMIDIIn>> m_midiIns;
  std::unique_ptr<boo2::IMIDIIn> m_virtualIn;
  boo2::MIDIDecoder m_decoder;

  bool m_useLock;
  std::list<std::pair<double, std::vector<uint8_t>>> m_queue;
  std::mutex m_midiMutex;
  void _MIDIReceive(std::vector<uint8_t>&& bytes, double time);

public:
  ~BooBackendMIDIReader();
  BooBackendMIDIReader(Engine& engine, bool useLock);

  void addMIDIIn(const char* name);
  void removeMIDIIn(const char* name);
  bool hasMIDIIn(const char* name) const;
  void setVirtualIn(bool v);
  bool hasVirtualIn() const;

  void pumpReader(double dt) override;

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

/** Backend voice allocator implementation for boo mixer */
class BooBackendVoiceAllocator : public IBackendVoiceAllocator, public boo2::IAudioVoiceEngineCallback {
  friend class BooBackendMIDIReader;

protected:
  boo2::IAudioVoiceEngine& m_booEngine;
  Engine* m_cbInterface = nullptr;

public:
  BooBackendVoiceAllocator(boo2::IAudioVoiceEngine& booEngine);
  std::unique_ptr<IBackendVoice> allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch) override;
  std::unique_ptr<IBackendSubmix> allocateSubmix(Submix& clientSmx, bool mainOut, int busId) override;
  std::vector<std::pair<std::string, std::string>> enumerateMIDIDevices() override;
  std::unique_ptr<IMIDIReader> allocateMIDIReader(Engine& engine) override;
  void setCallbackInterface(Engine* engine) override;
  AudioChannelSet getAvailableSet() override;
  void setVolume(float vol) override;
  void on5MsInterval(boo2::IAudioVoiceEngine& engine, double dt) override;
  void onPumpCycleComplete(boo2::IAudioVoiceEngine& engine) override;
};
} // namespace amuse

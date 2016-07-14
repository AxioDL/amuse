#ifndef __AMUSE_BOO_BACKEND_HPP__
#define __AMUSE_BOO_BACKEND_HPP__

#include <boo/audiodev/IAudioVoiceEngine.hpp>
#include <boo/audiodev/IAudioSubmix.hpp>
#include <boo/audiodev/IMIDIReader.hpp>
#include <boo/audiodev/MIDIDecoder.hpp>
#include "IBackendVoice.hpp"
#include "IBackendSubmix.hpp"
#include "IBackendVoiceAllocator.hpp"
#include <mutex>
#include <list>

namespace amuse
{

/** Backend voice implementation for boo mixer */
class BooBackendVoice : public IBackendVoice
{
    friend class BooBackendVoiceAllocator;
    Voice& m_clientVox;
    struct VoiceCallback : boo::IAudioVoiceCallback
    {
        BooBackendVoice& m_parent;
        void preSupplyAudio(boo::IAudioVoice& voice, double dt);
        size_t supplyAudio(boo::IAudioVoice& voice, size_t frames, int16_t* data);
        void routeAudio(size_t frames, double dt, int busId, int16_t* in, int16_t* out);
        void routeAudio(size_t frames, double dt, int busId, int32_t* in, int32_t* out);
        void routeAudio(size_t frames, double dt, int busId, float* in, float* out);
        VoiceCallback(BooBackendVoice& parent) : m_parent(parent) {}
    } m_cb;
    std::unique_ptr<boo::IAudioVoice> m_booVoice;

public:
    BooBackendVoice(boo::IAudioVoiceEngine& engine, Voice& clientVox, double sampleRate, bool dynamicPitch);
    void resetSampleRate(double sampleRate);

    void resetChannelLevels();
    void setChannelLevels(IBackendSubmix* submix, const float coefs[8], bool slew);
    void setPitchRatio(double ratio, bool slew);
    void start();
    void stop();
};

/** Backend submix implementation for boo mixer */
class BooBackendSubmix : public IBackendSubmix
{
    friend class BooBackendVoiceAllocator;
    friend class BooBackendVoice;
    Submix& m_clientSmx;
    struct SubmixCallback : boo::IAudioSubmixCallback
    {
        BooBackendSubmix& m_parent;
        bool canApplyEffect() const;
        void applyEffect(int16_t* audio, size_t frameCount, const boo::ChannelMap& chanMap, double sampleRate) const;
        void applyEffect(int32_t* audio, size_t frameCount, const boo::ChannelMap& chanMap, double sampleRate) const;
        void applyEffect(float* audio, size_t frameCount, const boo::ChannelMap& chanMap, double sampleRate) const;
        void resetOutputSampleRate(double sampleRate);
        SubmixCallback(BooBackendSubmix& parent) : m_parent(parent) {}
    } m_cb;
    std::unique_ptr<boo::IAudioSubmix> m_booSubmix;

public:
    BooBackendSubmix(boo::IAudioVoiceEngine& engine, Submix& clientSmx, bool mainOut, int busId);
    void setSendLevel(IBackendSubmix* submix, float level, bool slew);
    double getSampleRate() const;
    SubmixFormat getSampleFormat() const;
};

/** Backend MIDI event reader for controlling sequencer with external hardware / software */
class BooBackendMIDIReader : public IMIDIReader, public boo::IMIDIReader
{
    friend class BooBackendVoiceAllocator;
    Engine& m_engine;
    std::unique_ptr<boo::IMIDIIn> m_midiIn;
    boo::MIDIDecoder m_decoder;

    bool m_useLock;
    std::list<std::pair<double, std::vector<uint8_t>>> m_queue;
    std::mutex m_midiMutex;
    void _MIDIReceive(std::vector<uint8_t>&& bytes, double time);

public:
    ~BooBackendMIDIReader();
    BooBackendMIDIReader(Engine& engine, const char* name, bool useLock);

    std::string description();
    void pumpReader(double dt);

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

/** Backend voice allocator implementation for boo mixer */
class BooBackendVoiceAllocator : public IBackendVoiceAllocator
{
    friend class BooBackendMIDIReader;
    boo::IAudioVoiceEngine& m_booEngine;

public:
    BooBackendVoiceAllocator(boo::IAudioVoiceEngine& booEngine);
    std::unique_ptr<IBackendVoice> allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch);
    std::unique_ptr<IBackendSubmix> allocateSubmix(Submix& clientSmx, bool mainOut, int busId);
    std::vector<std::pair<std::string, std::string>> enumerateMIDIDevices();
    std::unique_ptr<IMIDIReader> allocateMIDIReader(Engine& engine, const char* name = nullptr);
    void register5MsCallback(std::function<void(double)>&& callback);
    AudioChannelSet getAvailableSet();
    void pumpAndMixVoices();
};
}

#endif // __AMUSE_BOO_BACKEND_HPP__

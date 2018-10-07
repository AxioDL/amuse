#pragma once

#include <memory>
#include <functional>
#include <vector>

namespace amuse
{
class IBackendVoice;
class IBackendSubmix;
class Voice;
class Submix;
class Engine;

/** Same enum as boo for describing speaker-configuration */
enum class AudioChannelSet
{
    Stereo,
    Quad,
    Surround51,
    Surround71,
    Unknown = 0xff
};

/** Handle to MIDI-reader, implementation opaque */
class IMIDIReader
{
public:
    virtual ~IMIDIReader() = default;
    virtual void pumpReader(double dt) = 0;
};

/** Client-implemented voice allocator */
class IBackendVoiceAllocator
{
public:
    virtual ~IBackendVoiceAllocator() = default;

    /** Amuse obtains a new voice from the platform this way */
    virtual std::unique_ptr<IBackendVoice> allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch) = 0;

    /** Amuse obtains a new submix from the platform this way */
    virtual std::unique_ptr<IBackendSubmix> allocateSubmix(Submix& clientSmx, bool mainOut, int busId) = 0;

    /** Amuse obtains a list of all MIDI devices this way */
    virtual std::vector<std::pair<std::string, std::string>> enumerateMIDIDevices() = 0;

    /** Amuse obtains an interactive MIDI-in connection from the OS this way */
    virtual std::unique_ptr<IMIDIReader> allocateMIDIReader(Engine& engine) = 0;

    /** Amuse obtains speaker-configuration from the platform this way */
    virtual AudioChannelSet getAvailableSet() = 0;

    /** Set volume of main mix out */
    virtual void setVolume(float vol) = 0;

    /** Amuse registers for key callback events from the mixing engine this way */
    virtual void setCallbackInterface(Engine* engine) = 0;
};
}


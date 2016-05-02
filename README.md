### Amuse

**Amuse** is a real-time MIDI and SFX sequencer, with basic effects, 
3D positional audio and surround-output capabilities.

The project is designed for compatibility with Audio Groups and Song data
found in PC/N64/GCN/GBA games using the *MusyX* audio engine, providing an
alternate runtime library to use for sequencing these games' audio libraries.

#### Library

The Amuse API exposes full interactivity between a client application
(game engine) and the sequencer engine. Unlike the interrupt-driven nature
of the original console implementations (where the audio chip 'requests' more
audio as needed), Amuse is entirely synchronous. This means the client must
periodically *pump* the audio engine (typically once per video frame) to keep
the OS' audio system fed.

The client must provide the implementation for allocating and mixing audio
voices, since this may drastically differ from target to target.
`amuse::IVoiceAllocator` is the pure-virtual interface to implement for this.

Here's an example usage:

```cpp
#include <amuse/amuse.hpp>
#include "MyVoiceAllocator.hpp"
#inlcude "MyAudioGroupLoader.hpp"

int main(int argc, char* argv[])
{
    /* Up to the client to implement voice allocation and mixing */
    std::unique_ptr<amuse::IVoiceAllocator> voxAlloc = MakeMyVoiceAllocator();

    /* Application just needs one per audio output (not per channel) */
    amuse::Engine snd(*voxAlloc);

    /* An 'AudioGroup' is an atomically-loadable unit within Amuse. 
     * A client-assigned integer serves as the handle to the group once loaded
     */
    int groupId = 1;
    amuse::AudioGroupData data = LoadMyAudioGroup(groupId);
    snd.addAudioGroup(groupId, data);

    /* Starting a SoundMacro playing is accomplished like so: */
    int sfxId = 0x1337;
    float vol = 1.0f;
    float pan = 0.0f;
    int voiceId = snd.fxStart(sfxId, vol, pan);

    /* Play for ~5 sec */
    int passedFrames = 0;
    while (passedFrames < 300)
    {
        snd.pumpEngine();
        ++passedFrames;
        WaitForVSync();
    }

    /* Stopping a SoundMacro is accomplished by sending the engine a
     * MIDI-style 'KeyOff' message for the voice
     */
    snd.fxKeyOff(voiceId);
    
    /* Play for 2 more seconds to allow the macro to gracefully fade-out */
    passedFrames = 0;
    while (passedFrames < 120)
    {
        snd.pumpEngine();
        ++passedFrames;
        WaitForVSync();
    }

    /* Clean up and exit */
    return 0;
}
```

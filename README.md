### Amuse

**Amuse** is a real-time MIDI and SFX sequencer, with basic effects, 
3D positional audio and surround-output capabilities.

The project is designed for compatibility with Audio Groups and Song data
found in PC/N64/GCN/GBA games using the *MusyX* audio engine; providing an
alternate runtime library to use for sequencing these games' audio libraries.

#### Command-Line Player

[Download](https://github.com/AxioDL/amuse/releases)

A simple command-line program for loading and playing AudioGroups out of
game archives or raw (`.proj`,`.pool`,`.sdir`,`.samp`) files is provided.

```sh
[jacko@ghor ~]$ amuseplay test.proj
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
░░░   ████ ████  ┃  ████ ████ ████   ┃   ████ ████  ░░░
░░░   ████ ████  ┃  ████ ████ ████   ┃   ████ ████  ░░░
░░░   ▌W▐█ ▌E▐█  ┃  ▌T▐█ ▌Y▐█ ▌U▐█   ┃   ▌O▐█ ▌P▐█  ░░░
░░░    │    │    ┃    │    │    │    ┃    │    │    ░░░
░░░ A  │ S  │ D  ┃ F  │ G  │ H  │ J  ┃ K  │ L  │ ;  ░░░
░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░
<left/right>: cycle MIDI setup, <up/down>: volume, <space>: PANIC
<tab>: sustain pedal, <window-Y>: pitch wheel, <window-X>: mod wheel
<Z/X>: octave, <C/V>: velocity, <B/N>: channel, <,/.>: program, <Q>: quit
  0 Setup 0, Chan 0, Prog 0, Octave: 4, Vel: 64, VOL: 80%
```

The command-line program requires a windowing environment and will open a
small 100x100 window alongside your terminal/cmd. This window **must** be
frontmost, since it listens to full keyboard events through it.

If a MIDI keyboard is connected and recognized by your OS before `amuseplay` 
is launched, you may directly control the sequencer using physical input.

On OS X and Linux, `amuseplay` will advertise a virtual MIDI-IN port for
other audio applications to route their MIDI messages to. This enables
tracker, drum machine, and DAW applications to produce sampled audio 
using Amuse directly.

### Converter Usage

`amuseconv <in-path> <out-path> [gcn|n64|pc]`

**Note:** `amuseconv` currently only supports SNG-to-MIDI conversion and vice-versa. To batch-extract MIDIs from one of the game files listed below, run `amuseconv <data-file> <destination-dir>`

### Player Usage

`amuseplay <data-file> [<songs-file>]`
— _or_ —
Drag-n-drop data file on amuseplay executable _(Windows and many freedesktop file managers support this)_

### Song Renderer Usage

`amuserender <data-file> [<songs-file>] [-r <sample-rate-out>] [-v <volume 0.0-1.0>]`
— _or_ —
Drag-n-drop data file on amuseplay executable _(Windows and many freedesktop file managers support this)_

**Note:** .wav file will be emitted at `<group-name>-<song-name>.wav`. If `-r` option is not specified, rate will default to 32KHz

### Currently Supported Game Containers
- _Indiana Jones and the Infernal Machine_ (N64) `N64 ROM file`
- _Metroid Prime_ (GCN) `AudioGrp.pak` `MidiData.pak`
  - _Pass `AudioGrp.pak MidiData.pak` for listening to SongGroup 53 (sequenced ship sounds)_
  - _Pass `AudioGrp.pak` for listening to any SFXGroup_
- _Metroid Prime 2: Echoes_ (GCN) `AudioGrp.pak`
- _Paper Mario: The Thousand Year Door_ (GCN) `pmario.proj` `pmario.slib`
  - _Pass `pmario.proj pmario.slib` for listening to SongGroups (sequenced SFX events)_
  - _Pass `pmario.proj` for listening to any SFXGroup_
- _Star Fox Adventures_ (GCN) `starfoxm.pro` `starfoxs.pro` `midi.wad`
  - _Pass `starfoxm.pro midi.wad` for listening to SongGroup 0 songs only (music)_
  - _Pass `starfoxs.pro midi.wad` for listening to other SongGroups (sequenced ambience)_
  - _Pass `starfoxs.pro` for listening to any SFXGroup_
- _Star Wars Episode I: Battle for Naboo_ (N64) `N64 ROM file`
- _Star Wars: Rogue Squadron_ (N64) `N64 ROM file` (PC) `data.dat`
- _Star Wars Rogue Squadron II: Rogue Leader_ (GCN) `data.dat`
- _Star Wars Rogue Squadron III: Rebel Strike_ (GCN) `data.dat`
- Any other game with raw `.proj` `.pool` `.sdir` `.samp` files
  - _Pass any one of them to Amuse, all in the same directory_

### Windows 7 Compatibility

Installing the MSVC++ 2015 runtime may be required if you haven't already installed/updated it:
https://www.microsoft.com/en-us/download/details.aspx?id=48145

#### Library

The Amuse API exposes full interactivity between a client application
(game engine) and the sequencer engine. Unlike the interrupt-driven nature
of the original console implementations (where the audio chip 'requests' more
audio as needed), Amuse is entirely synchronous. This means the client must
periodically *pump* the audio engine (typically once per video frame) to keep
the OS' audio system fed.

The client must provide the implementation for allocating and mixing audio
voices, since this may drastically differ from target to target.
`amuse::IBackendVoiceAllocator` is the pure-virtual interface to implement
for this. Alternatively, if [Boo](https://github.com/AxioDL/boo) is present
in the CMake project tree, Amuse will be compiled with a backend supporting
multiple popular low-level audio APIs. Windows, OS X, and Linux all have
excellent support this way.

Here's an example usage:

```cpp
#include <amuse/amuse.hpp>
#include "MyVoiceAllocator.hpp"
#include "MyAudioGroupLoader.hpp"

int main(int argc, char* argv[])
{
    /* Up to the client to implement voice allocation and mixing */
    std::unique_ptr<amuse::IBackendVoiceAllocator> voxAlloc = MakeMyVoiceAllocator();

    /* Application just needs one per audio output (not per channel) */
    amuse::Engine snd(*voxAlloc);

    /* An 'AudioGroup' is an atomically-loadable unit within Amuse. 
     * A client-assigned integer serves as the handle to the group once loaded
     */
    amuse::IntrusiveAudioGroupData data = LoadMyAudioGroup();
    snd.addAudioGroup(data);

    /* Starting a SoundMacro playing is accomplished like so: */
    int sfxId = 0x1337;
    float vol = 1.0f;
    float pan = 0.0f;
    std::shared_ptr<amuse::Voice> voice = snd.fxStart(sfxId, vol, pan);

    /* Play for ~5 sec */
    int passedFrames = 0;
    while (passedFrames < 300)
    {
        snd.pumpEngine();
        ++passedFrames;
        WaitForVSync();
    }

    /* Stopping a SoundMacro is accomplished by sending a
     * MIDI-style 'KeyOff' message for the voice
     */
    voice->keyOff();
    
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

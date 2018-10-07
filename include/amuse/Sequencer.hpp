#pragma once

#include "Entity.hpp"
#include "AudioGroupProject.hpp"
#include "SongState.hpp"
#include "Studio.hpp"
#include "Voice.hpp"
#include <unordered_map>
#include <unordered_set>
#include <memory>
#include <list>

namespace amuse
{

/** State of sequencer over lifetime */
enum class SequencerState
{
    Playing,     /**< Sequencer actively playing arrangement */
    Interactive, /**< Interactive sequencer for live MIDI message processing, will not automatically die */
    Dead         /**< Set when arrangement complete and `dieOnEnd` was set, or manually with die() */
};

/** Multi-voice lifetime manager and polyphonic parameter tracking */
class Sequencer : public Entity
{
    friend class Engine;
    const SongGroupIndex* m_songGroup = nullptr;               /**< Quick access to song group project index */
    const SongGroupIndex::MIDISetup* m_midiSetup = nullptr;    /**< Selected MIDI setup (may be null) */
    const SFXGroupIndex* m_sfxGroup = nullptr;                 /**< SFX Groups are alternatively referenced here */
    std::vector<const SFXGroupIndex::SFXEntry*> m_sfxMappings; /**< SFX entries are mapped to MIDI keys this via this */
    ObjToken<Studio> m_studio;                                 /**< Studio this sequencer outputs to */

    const unsigned char* m_arrData = nullptr;             /**< Current playing arrangement data */
    SongState m_songState;                                /**< State of current arrangement playback */
    SequencerState m_state = SequencerState::Interactive; /**< Current high-level state of sequencer */
    bool m_dieOnEnd = false; /**< Sequencer will be killed when current arrangement completes */

    float m_curVol = 1.f; /**< Current volume of sequencer */
    float m_volFadeTime = 0.f;
    float m_volFadeTarget = 0.f;
    float m_volFadeStart = 0.f;
    float m_stopFadeTime = 0.f;
    float m_stopFadeBeginVol = 0.f;

    /** State of a single MIDI channel */
    struct ChannelState
    {
        Sequencer* m_parent = nullptr;
        uint8_t m_chanId;
        const SongGroupIndex::MIDISetup* m_setup = nullptr; /* Channel defaults to program 0 if null */
        const SongGroupIndex::PageEntry* m_page = nullptr;
        ~ChannelState();
        ChannelState() = default;
        ChannelState(Sequencer& parent, uint8_t chanId);
        operator bool() const { return m_parent != nullptr; }

        /** Voices corresponding to currently-pressed keys in channel */
        std::unordered_map<uint8_t, ObjToken<Voice>> m_chanVoxs;
        std::unordered_set<ObjToken<Voice>> m_keyoffVoxs;
        ObjToken<Voice> m_lastVoice;
        int8_t m_ctrlVals[128] = {}; /**< MIDI controller values */
        float m_curPitchWheel = 0.f; /**< MIDI pitch-wheel */
        int8_t m_pitchWheelRange = -1; /**< Pitch wheel range settable by RPN 0 */
        int8_t m_curProgram = 0;     /**< MIDI program number */
        float m_curVol = 1.f;        /**< Current volume of channel */
        float m_curPan = 0.f;        /**< Current panning of channel */
        uint16_t m_rpn = 0;          /**< Current RPN (only pitch-range 0x0000 supported) */
        double m_ticksPerSec = 1000.0; /**< Current ticks per second (tempo) for channel */

        void _bringOutYourDead();
        size_t getVoiceCount() const;
        ObjToken<Voice> keyOn(uint8_t note, uint8_t velocity);
        void keyOff(uint8_t note, uint8_t velocity);
        void setCtrlValue(uint8_t ctrl, int8_t val);
        bool programChange(int8_t prog);
        void nextProgram();
        void prevProgram();
        void setPitchWheel(float pitchWheel);
        void setVolume(float vol);
        void setPan(float pan);
        void allOff();
        void killKeygroup(uint8_t kg, bool now);
        ObjToken<Voice> findVoice(int vid);
        void sendMacroMessage(ObjectId macroId, int32_t val);
    };
    std::array<ChannelState, 16> m_chanStates; /**< Lazily-allocated channel states */

    void _bringOutYourDead();
    void _destroy();

public:
    ~Sequencer();
    Sequencer(Engine& engine, const AudioGroup& group, GroupId groupId, const SongGroupIndex* songGroup, SongId setupId,
              ObjToken<Studio> studio);
    Sequencer(Engine& engine, const AudioGroup& group, GroupId groupId, const SFXGroupIndex* sfxGroup,
              ObjToken<Studio> studio);

    /** Advance current song data (if any) */
    void advance(double dt);

    /** Obtain pointer to Sequencer's Submix */
    ObjToken<Studio> getStudio() { return m_studio; }

    /** Get current state of sequencer */
    SequencerState state() const { return m_state; }

    /** Get number of active voices */
    size_t getVoiceCount() const;

    /** Register key press with voice set */
    ObjToken<Voice> keyOn(uint8_t chan, uint8_t note, uint8_t velocity);

    /** Register key release with voice set */
    void keyOff(uint8_t chan, uint8_t note, uint8_t velocity);

    /** Set MIDI control value [0,127] for all voices */
    void setCtrlValue(uint8_t chan, uint8_t ctrl, int8_t val);

    /** Set pitchwheel value for use with voice controllers */
    void setPitchWheel(uint8_t chan, float pitchWheel);

    /** Send keyoffs to all active notes, silence immediately if `now` set */
    void allOff(bool now = false);

    /** Send keyoffs to all active notes on specified channel, silence immediately if `now` set */
    void allOff(uint8_t chan, bool now = false);

    /** Stop all voices in `kg`, stops immediately (no KeyOff) when `now` set */
    void killKeygroup(uint8_t kg, bool now);

    /** Find voice instance contained within Sequencer */
    ObjToken<Voice> findVoice(int vid);

    /** Send all voices using `macroId` the message `val` */
    void sendMacroMessage(ObjectId macroId, int32_t val);

    /** Set tempo of sequencer and all voices in ticks per second */
    void setTempo(uint8_t chan, double ticksPerSec);
    void setTempo(double ticksPerSec);

    /** Play MIDI arrangement */
    void playSong(const unsigned char* arrData, bool loop = true, bool dieOnEnd = true);

    /** Stop current MIDI arrangement */
    void stopSong(float fadeTime = 0.f, bool now = false);

    /** Set total volume of sequencer */
    void setVolume(float vol, float fadeTime = 0.f);

    /** Get current program number of channel */
    int8_t getChanProgram(int8_t chanId) const;

    /** Set current program number of channel */
    bool setChanProgram(int8_t chanId, int8_t prog);

    /** Advance to next program in channel */
    void nextChanProgram(int8_t chanId);

    /** Advance to prev program in channel */
    void prevChanProgram(int8_t chanId);

    /** Manually kill sequencer for deferred release from engine */
    void kill() { m_state = SequencerState::Dead; }
};
}


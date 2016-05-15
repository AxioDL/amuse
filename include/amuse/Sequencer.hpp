#ifndef __AMUSE_SEQUENCER_HPP__
#define __AMUSE_SEQUENCER_HPP__

#include "Entity.hpp"
#include "AudioGroupProject.hpp"
#include <unordered_map>
#include <memory>
#include <list>

namespace amuse
{
class Submix;
class Voice;

/** State of sequencer over lifetime */
enum class SequencerState
{
    Playing, /**< Sequencer actively playing arrangement */
    Interactive, /**< Interactive sequencer for live MIDI message processing, will not automatically die */
    Dead /**< Set when arrangement complete and `dieOnEnd` was set, or manually with die() */
};

class Sequencer : public Entity
{
    friend class Engine;
    const SongGroupIndex& m_songGroup; /**< Quick access to song group project index */
    const SongGroupIndex::MIDISetup* m_midiSetup = nullptr; /**< Selected MIDI setup */
    Submix* m_submix = nullptr; /**< Submix this sequencer outputs to (or NULL for the main output mix) */
    std::list<std::shared_ptr<Sequencer>>::iterator m_engineIt; /**< Iterator to self within Engine's list for quick deletion */

    const unsigned char* m_arrData = nullptr; /**< Current playing arrangement data */
    double m_ticksPerSec = 1000.0; /**< Current ticks per second (tempo) for arrangement data */
    SequencerState m_state = SequencerState::Interactive; /**< Current high-level state of sequencer */
    bool m_dieOnEnd = false; /**< Sequencer will be killed when current arrangement completes */

    /** State of a single MIDI channel */
    struct ChannelState
    {
        Sequencer& m_parent;
        const SongGroupIndex::MIDISetup& m_setup;
        const SongGroupIndex::PageEntry* m_page = nullptr;
        Submix* m_submix = nullptr;
        ~ChannelState();
        ChannelState(Sequencer& parent, uint8_t chanId);

        /** Voices corresponding to currently-pressed keys in channel */
        std::unordered_map<uint8_t, std::shared_ptr<Voice>> m_chanVoxs;
        int8_t m_ctrlVals[128]; /**< MIDI controller values */

        void _bringOutYourDead();
        std::shared_ptr<Voice> keyOn(uint8_t note, uint8_t velocity);
        void keyOff(uint8_t note, uint8_t velocity);
        void setCtrlValue(uint8_t ctrl, int8_t val);
        void setPitchWheel(float pitchWheel);
        void allOff();
        void killKeygroup(uint8_t kg, bool now);
        std::shared_ptr<Voice> findVoice(int vid);
        void sendMacroMessage(ObjectId macroId, int32_t val);
    };
    std::unordered_map<uint8_t, std::unique_ptr<ChannelState>> m_chanStates; /**< Lazily-allocated channel states */

    void _bringOutYourDead();
    void _destroy();
public:
    ~Sequencer();
    Sequencer(Engine& engine, const AudioGroup& group, int groupId,
              const SongGroupIndex& songGroup, int setupId, Submix* smx);

    /** Obtain pointer to Sequencer's Submix */
    Submix* getSubmix() {return m_submix;}

    /** Get current state of sequencer */
    SequencerState state() const {return m_state;}

    /** Get number of active voices */
    size_t getVoiceCount() const;

    /** Register key press with voice set */
    std::shared_ptr<Voice> keyOn(uint8_t chan, uint8_t note, uint8_t velocity);

    /** Register key release with voice set */
    void keyOff(uint8_t chan, uint8_t note, uint8_t velocity);

    /** Set MIDI control value [0,127] for all voices */
    void setCtrlValue(uint8_t chan, uint8_t ctrl, int8_t val);

    /** Set pitchwheel value for use with voice controllers */
    void setPitchWheel(uint8_t chan, float pitchWheel);

    /** Send keyoffs to all active notes, silence immediately if `now` set */
    void allOff(bool now=false);

    /** Stop all voices in `kg`, stops immediately (no KeyOff) when `now` set */
    void killKeygroup(uint8_t kg, bool now);

    /** Find voice instance contained within Sequencer */
    std::shared_ptr<Voice> findVoice(int vid);

    /** Send all voices using `macroId` the message `val` */
    void sendMacroMessage(ObjectId macroId, int32_t val);

    /** Set tempo of sequencer and all voices in ticks per second */
    void setTempo(double ticksPerSec);

    /** Play MIDI arrangement */
    void playSong(const unsigned char* arrData, bool dieOnEnd=true);

    /** Manually kill sequencer for deferred release from engine */
    void kill() {m_state = SequencerState::Dead;}
};

}

#endif // __AMUSE_SEQUENCER_HPP__

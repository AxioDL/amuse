#ifndef __AMUSE_SEQUENCER_HPP__
#define __AMUSE_SEQUENCER_HPP__

#include "Entity.hpp"
#include "AudioGroupProject.hpp"
#include <unordered_map>
#include <memory>

namespace amuse
{
class Submix;
class Voice;

class Sequencer : public Entity
{
    friend class Engine;
    const SongGroupIndex& m_songGroup; /**< Quick access to song group project index */
    const SongGroupIndex::MIDISetup* m_midiSetup = nullptr; /**< Selected MIDI setup */
    Submix* m_submix = nullptr; /**< Submix this sequencer outputs to (or NULL for the main output mix) */

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

        std::shared_ptr<Voice> keyOn(uint8_t note, uint8_t velocity);
        void keyOff(uint8_t note, uint8_t velocity);
        void setCtrlValue(uint8_t ctrl, int8_t val);
        void setPitchWheel(float pitchWheel);
        void allOff();
    };
    std::unordered_map<uint8_t, ChannelState> m_chanStates; /**< Lazily-allocated channel states */

    void _destroy();
public:
    ~Sequencer();
    Sequencer(Engine& engine, const AudioGroup& group,
              const SongGroupIndex& songGroup, int setupId, Submix* smx);

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
};

}

#endif // __AMUSE_SEQUENCER_HPP__

#ifndef __AMUSE_SONGSTATE_HPP__
#define __AMUSE_SONGSTATE_HPP__

#include <stdint.h>
#include <vector>
#include <array>
#include <list>
#include "optional.hpp"
#include "Entity.hpp"

namespace amuse
{
class Sequencer;

enum class SongPlayState
{
    Stopped,
    Playing
};

/** Real-time state of Song execution */
class SongState
{
    friend class Voice;

    /** Song header */
    struct Header
    {
        uint32_t m_version;
        uint32_t m_chanIdxOff;
        uint32_t m_chanMapOff;
        uint32_t m_tempoTableOff;
        uint32_t m_initialTempo;
        uint32_t m_unkOff;
        uint32_t m_chanOffs[64];
        void swapBig();
    } m_header;

    /** Channel header */
    struct ChanHeader
    {
        uint32_t m_startTick;
        uint16_t m_unk1;
        uint16_t m_unk2;
        uint16_t m_dataIndex;
        uint16_t m_unk3;
        uint32_t m_startTick2;
        uint16_t m_unk4;
        uint16_t m_unk5;
        uint16_t m_unk6;
        uint16_t m_unk7;
        void swapBig();
    };

    /** Tempo change entry */
    struct TempoChange
    {
        uint32_t m_tick; /**< Relative song ticks from previous tempo change */
        uint32_t m_tempo; /**< Tempo value in beats-per-minute (at 384 ticks per quarter-note) */
        void swapBig();
    };

    /** State of a single channel within arrangement */
    struct Channel
    {
        struct Header
        {
            uint32_t m_type;
            uint32_t m_pitchOff;
            uint32_t m_modOff;
            void swapBig();
        };

        SongState& m_parent;
        uint8_t m_midiChan; /**< MIDI channel number of song channel */
        uint32_t m_startTick; /**< Tick to start execution of channel commands */

        const unsigned char* m_dataBase; /**< Base pointer to command data */
        const unsigned char* m_data; /**< Pointer to upcoming command data */
        const unsigned char* m_pitchWheelData = nullptr; /**< Pointer to upcoming pitch data */
        const unsigned char* m_modWheelData = nullptr; /**< Pointer to upcoming modulation data */
        uint32_t m_lastPitchTick = 0; /**< Last position of pitch wheel change */
        int32_t m_lastPitchVal = 0; /**< Last value of pitch */
        uint32_t m_lastModTick = 0; /**< Last position of mod wheel change */
        int32_t m_lastModVal = 0; /**< Last value of mod */
        std::array<uint16_t, 128> m_remNoteLengths = {}; /**< Remaining ticks per note */

        int32_t m_waitCountdown = 0; /**< Current wait in ticks */

        Channel(SongState& parent, uint8_t midiChan, uint32_t startTick,
                const unsigned char* song, const unsigned char* chan);
        bool advance(Sequencer& seq, int32_t ticks);
    };
    std::array<std::experimental::optional<Channel>, 64> m_channels;

    /** Current pointer to tempo control, iterated over playback */
    const TempoChange* m_tempoPtr = nullptr;
    uint32_t m_tempo = 120; /**< Current tempo (beats per minute) */

    uint32_t m_curTick = 0; /**< Current playback position for all channels */
    SongPlayState m_songState = SongPlayState::Playing; /**< High-level state of Song playback */
    double m_curDt = 0.f; /**< Cumulative dt value for time-remainder tracking */

public:
    /** initialize state for Song data at `ptr` */
    void initialize(const unsigned char* ptr);

    /** advances `dt` seconds worth of commands in the Song
     *  @return `true` if END reached
     */
    bool advance(Sequencer& seq, double dt);
};

}

#endif // __AMUSE_SONGSTATE_HPP__

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
    friend class SongConverter;

    /** Song header */
    struct Header
    {
        uint32_t m_trackIdxOff;
        uint32_t m_regionIdxOff;
        uint32_t m_chanMapOff;
        uint32_t m_tempoTableOff;
        uint32_t m_initialTempo;
        uint32_t m_unkOff;
        void swapBig();
    } m_header;

    /** Track region ('clip' in an NLA representation) */
    struct TrackRegion
    {
        uint32_t m_startTick;
        uint8_t m_progNum;
        uint8_t m_unk1;
        uint16_t m_unk2;
        int16_t m_regionIndex;
        int16_t m_unk3;
        bool indexValid(bool bigEndian) const;
    };

    /** Tempo change entry */
    struct TempoChange
    {
        uint32_t m_tick;  /**< Relative song ticks from previous tempo change */
        uint32_t m_tempo; /**< Tempo value in beats-per-minute (at 384 ticks per quarter-note) */
        void swapBig();
    };

    const unsigned char* m_songData = nullptr; /**< Base pointer to active song */
    int m_sngVersion;                          /**< Detected song revision, 1 has RLE-compressed delta-times */
    bool m_bigEndian;                          /**< True if loaded song is big-endian data */

    /** State of a single track within arrangement */
    struct Track
    {
        struct Header
        {
            uint32_t m_type;
            uint32_t m_pitchOff;
            uint32_t m_modOff;
            void swapBig();
        };

        SongState& m_parent;
        uint8_t m_midiChan;              /**< MIDI channel number of song channel */
        const TrackRegion* m_curRegion;  /**< Pointer to currently-playing track region */
        const TrackRegion* m_nextRegion; /**< Pointer to next-queued track region */

        const unsigned char* m_data = nullptr;           /**< Pointer to upcoming command data */
        const unsigned char* m_pitchWheelData = nullptr; /**< Pointer to upcoming pitch data */
        const unsigned char* m_modWheelData = nullptr;   /**< Pointer to upcoming modulation data */
        uint32_t m_lastPitchTick = 0;                    /**< Last position of pitch wheel change */
        int32_t m_lastPitchVal = 0;                      /**< Last value of pitch */
        uint32_t m_lastModTick = 0;                      /**< Last position of mod wheel change */
        int32_t m_lastModVal = 0;                        /**< Last value of mod */
        std::array<int, 128> m_remNoteLengths;           /**< Remaining ticks per note */

        int32_t m_eventWaitCountdown = 0; /**< Current wait in ticks */
        int32_t m_lastN64EventTick =
            0; /**< Last command time on this channel (for computing delta times from absolute times in N64 songs) */

        Track(SongState& parent, uint8_t midiChan, const TrackRegion* regions);
        void setRegion(Sequencer* seq, const TrackRegion* region);
        void advanceRegion(Sequencer* seq);
        bool advance(Sequencer& seq, int32_t ticks);
    };
    std::array<std::experimental::optional<Track>, 64> m_tracks;
    const uint32_t* m_regionIdx; /**< Table of offsets to song-region data */

    /** Current pointer to tempo control, iterated over playback */
    const TempoChange* m_tempoPtr = nullptr;
    uint32_t m_tempo = 120; /**< Current tempo (beats per minute) */

    uint32_t m_curTick = 0;                             /**< Current playback position for all channels */
    SongPlayState m_songState = SongPlayState::Playing; /**< High-level state of Song playback */
    double m_curDt = 0.f;                               /**< Cumulative dt value for time-remainder tracking */

public:
    /** Determine SNG version
     *  @param isBig returns true if big-endian SNG
     *  @return 0 for initial version, 1 for delta-time revision, -1 for non-SNG */
    static int DetectVersion(const unsigned char* ptr, bool& isBig);

    /** initialize state for Song data at `ptr` */
    bool initialize(const unsigned char* ptr);

    /** advances `dt` seconds worth of commands in the Song
     *  @return `true` if END reached
     */
    bool advance(Sequencer& seq, double dt);

    /** Get current song tempo in BPM */
    uint32_t getTempo() const { return m_tempo; }
};
}

#endif // __AMUSE_SONGSTATE_HPP__

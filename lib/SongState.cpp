#include "amuse/SongState.hpp"
#include "amuse/Common.hpp"
#include "amuse/Sequencer.hpp"
#include <cmath>

namespace amuse
{

static uint32_t DecodeRLE(const unsigned char*& data)
{
    uint32_t ret = 0;

    while (true)
    {
        uint32_t thisPart = *data & 0x7f;
        if (*data & 0x80)
        {
            ++data;
            thisPart = thisPart * 256 + *data;
            if (thisPart == 0)
                return -1;
        }

        if (thisPart == 32767)
        {
            ret += 32767;
            data += 2;
            continue;
        }

        ret += thisPart;
        data += 1;
        break;
    }

    return ret;
}

static int32_t DecodeContinuousRLE(const unsigned char*& data)
{
    int32_t ret = int32_t(DecodeRLE(data));
    if (ret >= 16384)
        return ret - 32767;
    return ret;
}

static uint32_t DecodeTimeRLE(const unsigned char*& data)
{
    uint32_t ret = 0;

    while (true)
    {
        uint16_t thisPart = SBig(*reinterpret_cast<const uint16_t*>(data));
        if (thisPart == 0xffff)
        {
            ret += 65535;
            data += 4;
            continue;
        }

        ret += thisPart;
        data += 2;
        break;
    }

    return ret;
}

void SongState::Header::swapBig()
{
    m_version = SBig(m_version);
    m_chanIdxOff = SBig(m_chanIdxOff);
    m_chanMapOff = SBig(m_chanMapOff);
    m_tempoTableOff = SBig(m_tempoTableOff);
    m_initialTempo = SBig(m_initialTempo);
    m_unkOff = SBig(m_unkOff);
    for (int i=0 ; i<64 ; ++i)
        m_chanOffs[i] = SBig(m_chanOffs[i]);
}

void SongState::ChanHeader::swapBig()
{
    m_startTick = SBig(m_startTick);
    m_unk1 = SBig(m_unk1);
    m_unk2 = SBig(m_unk2);
    m_dataIndex = SBig(m_dataIndex);
    m_unk3 = SBig(m_unk3);
    m_startTick2 = SBig(m_startTick2);
    m_unk4 = SBig(m_unk4);
    m_unk5 = SBig(m_unk5);
    m_unk6 = SBig(m_unk6);
    m_unk7 = SBig(m_unk7);
}

void SongState::TempoChange::swapBig()
{
    m_tick = SBig(m_tick);
    m_tempo = SBig(m_tempo);
}

void SongState::Channel::Header::swapBig()
{
    m_type = SBig(m_type);
    m_pitchOff = SBig(m_pitchOff);
    m_modOff = SBig(m_modOff);
}

SongState::Channel::Channel(SongState& parent, uint8_t midiChan, uint32_t startTick,
                            const unsigned char* song, const unsigned char* chan)
: m_parent(parent), m_midiChan(midiChan), m_startTick(startTick), m_dataBase(chan + 12)
{
    m_data = m_dataBase;

    Header header = *reinterpret_cast<const Header*>(chan);
    header.swapBig();
    if (header.m_type != 8)
    {
        m_data = nullptr;
        return;
    }

    if (header.m_pitchOff)
        m_pitchWheelData = song + header.m_pitchOff;
    if (header.m_modOff)
        m_modWheelData = song + header.m_modOff;

    m_waitCountdown = startTick;
    m_lastPitchTick = startTick;
    m_lastModTick = startTick;
    m_waitCountdown += int32_t(DecodeTimeRLE(m_data));
}

void SongState::initialize(const unsigned char* ptr)
{
    m_header = *reinterpret_cast<const Header*>(ptr);
    m_header.swapBig();

    /* Initialize all channels */
    for (int i=0 ; i<64 ; ++i)
    {
        if (m_header.m_chanOffs[i])
        {
            ChanHeader cHeader = *reinterpret_cast<const ChanHeader*>(ptr + m_header.m_chanOffs[i]);
            cHeader.swapBig();
            const uint32_t* chanIdx = reinterpret_cast<const uint32_t*>(ptr + m_header.m_chanIdxOff);
            const uint8_t* chanMap = reinterpret_cast<const uint8_t*>(ptr + m_header.m_chanMapOff);
            m_channels[i].emplace(*this, chanMap[i], cHeader.m_startTick, ptr,
                                  ptr + SBig(chanIdx[cHeader.m_dataIndex]));
        }
        else
            m_channels[i] = std::experimental::nullopt;
    }

    /* Initialize tempo */
    if (m_header.m_tempoTableOff)
        m_tempoPtr = reinterpret_cast<const TempoChange*>(ptr + m_header.m_tempoTableOff);
    else
        m_tempoPtr = nullptr;

    m_tempo = m_header.m_initialTempo;
    m_curTick = 0;
    m_songState = SongPlayState::Playing;
}

bool SongState::Channel::advance(Sequencer& seq, int32_t ticks)
{
    if (!m_data)
        return true;

    int32_t endTick = m_parent.m_curTick + ticks;

    /* Update continuous pitch data */
    if (m_pitchWheelData)
    {
        int32_t pitchTick = m_parent.m_curTick;
        int32_t remPitchTicks = ticks;
        while (pitchTick < endTick)
        {
            /* See if there's an upcoming pitch change in this interval */
            const unsigned char* ptr = m_pitchWheelData;
            uint32_t deltaTicks = DecodeRLE(ptr);
            if (deltaTicks != 0xffffffff)
            {
                int32_t nextTick = m_lastPitchTick + deltaTicks;
                if (pitchTick + remPitchTicks > nextTick)
                {
                    /* Update pitch */
                    int32_t pitchDelta = DecodeContinuousRLE(ptr);
                    m_lastPitchVal += pitchDelta;
                    m_pitchWheelData = ptr;
                    m_lastPitchTick = nextTick;
                    remPitchTicks -= (nextTick - pitchTick);
                    pitchTick = nextTick;
                    seq.setPitchWheel(m_midiChan, clamp(-1.f, m_lastPitchVal / 32768.f, 1.f));
                    continue;
                }
                remPitchTicks -= (nextTick - pitchTick);
                pitchTick = nextTick;
            }
            else
                break;
        }
    }

    /* Update continuous modulation data */
    if (m_modWheelData)
    {
        int32_t modTick = m_parent.m_curTick;
        int32_t remModTicks = ticks;
        while (modTick < endTick)
        {
            /* See if there's an upcoming modulation change in this interval */
            const unsigned char* ptr = m_modWheelData;
            uint32_t deltaTicks = DecodeRLE(ptr);
            if (deltaTicks != 0xffffffff)
            {
                int32_t nextTick = m_lastModTick + deltaTicks;
                if (modTick + remModTicks > nextTick)
                {
                    /* Update modulation */
                    int32_t modDelta = DecodeContinuousRLE(ptr);
                    m_lastModVal += modDelta;
                    m_modWheelData = ptr;
                    m_lastModTick = nextTick;
                    remModTicks -= (nextTick - modTick);
                    modTick = nextTick;
                    seq.setCtrlValue(m_midiChan, 1, clamp(0, (m_lastModVal + 8192) * 128 / 16384, 127));
                    continue;
                }
                remModTicks -= (nextTick - modTick);
                modTick = nextTick;
            }
            else
                break;
        }
    }

    /* Stop finished notes */
    for (int i=0 ; i<128 ; ++i)
    {
        if (m_remNoteLengths[i])
        {
            if (m_remNoteLengths[i] <= ticks)
            {
                seq.keyOff(m_midiChan, i, 0);
                m_remNoteLengths[i] = 0;
            }
            else
                m_remNoteLengths[i] -= ticks;
        }
    }

    /* Loop through as many commands as we can for this time period */
    while (true)
    {
        /* Advance wait timer if active, returning if waiting */
        if (m_waitCountdown)
        {
            m_waitCountdown -= ticks;
            ticks = 0;
            if (m_waitCountdown > 0)
                return false;
        }

        /* Load next command */
        if (*reinterpret_cast<const uint16_t*>(m_data) == 0xffff)
        {
            /* End of channel */
            m_data = nullptr;
            return true;
        }
        else if (m_data[0] & 0x80)
        {
            /* Control change */
            uint8_t val = m_data[0] & 0x7f;
            uint8_t ctrl = m_data[1] & 0x7f;
            seq.setCtrlValue(m_midiChan, ctrl, val);
            m_data += 2;
        }
        else
        {
            /* Note */
            uint8_t note = m_data[0] & 0x7f;
            uint8_t vel = m_data[1] & 0x7f;
            uint16_t length = SBig(*reinterpret_cast<const uint16_t*>(m_data + 2));
            seq.keyOn(m_midiChan, note, vel);
            m_remNoteLengths[note] = length;
            m_data += 4;
        }

        /* Set next delta-time */
        m_waitCountdown += int32_t(DecodeTimeRLE(m_data));
    }

    return false;
}

bool SongState::advance(Sequencer& seq, double dt)
{
    /* Stopped */
    if (m_songState == SongPlayState::Stopped)
        return true;

    bool done = false;
    m_curDt += dt;
    while (m_curDt > 0.0)
    {
        done = true;

        /* Compute ticks to compute based on current tempo */
        double ticksPerSecond = m_tempo * 384 / 60;
        int32_t remTicks = std::ceil(m_curDt * ticksPerSecond);
        if (!remTicks)
            break;

        /* See if there's an upcoming tempo change in this interval */
        if (m_tempoPtr && m_tempoPtr->m_tick != 0xffffffff)
        {
            TempoChange change = *m_tempoPtr;
            change.swapBig();

            if (m_curTick + remTicks > change.m_tick)
                remTicks = change.m_tick - m_curTick;

            if (remTicks <= 0)
            {
                /* Turn over tempo */
                m_tempo = change.m_tempo;
                seq.setTempo(m_tempo * 384 / 60);
                ++m_tempoPtr;
                continue;
            }
        }

        /* Advance all channels */
        for (std::experimental::optional<Channel>& chan : m_channels)
            if (chan)
                done &= chan->advance(seq, remTicks);

        m_curTick += remTicks;

        if (m_tempo == 0)
            m_curDt = 0.0;
        else
            m_curDt -= remTicks / ticksPerSecond;
    }

    if (done)
        m_songState = SongPlayState::Stopped;
    return done;
}

}

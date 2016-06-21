#include "amuse/SongConverter.hpp"
#include "amuse/SongState.hpp"
#include "amuse/Common.hpp"
#include <stdlib.h>
#include <map>

namespace amuse
{

static inline uint8_t clamp7(uint8_t val) {return std::max(0, std::min(127, int(val)));}

enum class Status
{
    NoteOff = 0x80,
    NoteOn = 0x90,
    NotePressure = 0xA0,
    ControlChange = 0xB0,
    ProgramChange = 0xC0,
    ChannelPressure = 0xD0,
    PitchBend = 0xE0,
    SysEx = 0xF0,
    TimecodeQuarterFrame = 0xF1,
    SongPositionPointer = 0xF2,
    SongSelect = 0xF3,
    TuneRequest = 0xF6,
    SysExTerm = 0xF7,
    TimingClock = 0xF8,
    Start = 0xFA,
    Continue = 0xFB,
    Stop = 0xFC,
    ActiveSensing = 0xFE,
    Reset = 0xFF,
};

class IMIDIReader
{
public:
    virtual void noteOff(uint8_t chan, uint8_t key, uint8_t velocity)=0;
    virtual void noteOn(uint8_t chan, uint8_t key, uint8_t velocity)=0;
    virtual void notePressure(uint8_t chan, uint8_t key, uint8_t pressure)=0;
    virtual void controlChange(uint8_t chan, uint8_t control, uint8_t value)=0;
    virtual void programChange(uint8_t chan, uint8_t program)=0;
    virtual void channelPressure(uint8_t chan, uint8_t pressure)=0;
    virtual void pitchBend(uint8_t chan, int16_t pitch)=0;

    virtual void allSoundOff(uint8_t chan)=0;
    virtual void resetAllControllers(uint8_t chan)=0;
    virtual void localControl(uint8_t chan, bool on)=0;
    virtual void allNotesOff(uint8_t chan)=0;
    virtual void omniMode(uint8_t chan, bool on)=0;
    virtual void polyMode(uint8_t chan, bool on)=0;

    virtual void sysex(const void* data, size_t len)=0;
    virtual void timeCodeQuarterFrame(uint8_t message, uint8_t value)=0;
    virtual void songPositionPointer(uint16_t pointer)=0;
    virtual void songSelect(uint8_t song)=0;
    virtual void tuneRequest()=0;

    virtual void startSeq()=0;
    virtual void continueSeq()=0;
    virtual void stopSeq()=0;

    virtual void reset()=0;
};

class MIDIDecoder
{
    IMIDIReader& m_out;
    uint8_t m_status = 0;
    bool _readContinuedValue(std::vector<uint8_t>::const_iterator& it,
                             std::vector<uint8_t>::const_iterator end,
                             uint32_t& valOut)
    {
        uint8_t a = *it++;
        valOut = a & 0x7f;

        if (a & 0x80)
        {
            if (it == end)
                return false;
            valOut <<= 7;
            a = *it++;
            valOut |= a & 0x7f;

            if (a & 0x80)
            {
                if (it == end)
                    return false;
                valOut <<= 7;
                a = *it++;
                valOut |= a & 0x7f;
            }
        }

        return true;
    }
public:
    MIDIDecoder(IMIDIReader& out) : m_out(out) {}
    std::vector<uint8_t>::const_iterator
    receiveBytes(std::vector<uint8_t>::const_iterator begin,
                 std::vector<uint8_t>::const_iterator end)
    {
        std::vector<uint8_t>::const_iterator it = begin;
        if (it == end)
            return begin;

        uint8_t a = *it++;
        uint8_t b;
        if (a & 0x80)
            m_status = a;
        else
            it--;

        uint8_t chan = m_status & 0xf;
        switch (Status(m_status & 0xf0))
        {
        case Status::NoteOff:
        {
            if (it == end)
                return begin;
            a = *it++;
            if (it == end)
                return begin;
            b = *it++;
            m_out.noteOff(chan, clamp7(a), clamp7(b));
            break;
        }
        case Status::NoteOn:
        {
            if (it == end)
                return begin;
            a = *it++;
            if (it == end)
                return begin;
            b = *it++;
            m_out.noteOn(chan, clamp7(a), clamp7(b));
            break;
        }
        case Status::NotePressure:
        {
            if (it == end)
                return begin;
            a = *it++;
            if (it == end)
                return begin;
            b = *it++;
            m_out.notePressure(chan, clamp7(a), clamp7(b));
            break;
        }
        case Status::ControlChange:
        {
            if (it == end)
                return begin;
            a = *it++;
            if (it == end)
                return begin;
            b = *it++;
            m_out.controlChange(chan, clamp7(a), clamp7(b));
            break;
        }
        case Status::ProgramChange:
        {
            if (it == end)
                return begin;
            a = *it++;
            m_out.programChange(chan, clamp7(a));
            break;
        }
        case Status::ChannelPressure:
        {
            if (it == end)
                return begin;
            a = *it++;
            m_out.channelPressure(chan, clamp7(a));
            break;
        }
        case Status::PitchBend:
        {
            if (it == end)
                return begin;
            a = *it++;
            if (it == end)
                return begin;
            b = *it++;
            m_out.pitchBend(chan, clamp7(b) * 128 + clamp7(a));
            break;
        }
        case Status::SysEx:
        {
            switch (Status(m_status & 0xff))
            {
            case Status::SysEx:
            {
                uint32_t len;
                if (!_readContinuedValue(it, end, len) || end - it < len)
                    return begin;
                m_out.sysex(&*it, len);
                break;
            }
            case Status::TimecodeQuarterFrame:
            {
                if (it == end)
                    return begin;
                a = *it++;
                m_out.timeCodeQuarterFrame(a >> 4 & 0x7, a & 0xf);
                break;
            }
            case Status::SongPositionPointer:
            {
                if (it == end)
                    return begin;
                a = *it++;
                if (it == end)
                    return begin;
                b = *it++;
                m_out.songPositionPointer(clamp7(b) * 128 + clamp7(a));
                break;
            }
            case Status::SongSelect:
            {
                if (it == end)
                    return begin;
                a = *it++;
                m_out.songSelect(clamp7(a));
                break;
            }
            case Status::TuneRequest:
                m_out.tuneRequest();
                break;
            case Status::Start:
                m_out.startSeq();
                break;
            case Status::Continue:
                m_out.continueSeq();
                break;
            case Status::Stop:
                m_out.stopSeq();
                break;
            case Status::Reset:
                m_out.reset();
                break;
            case Status::SysExTerm:
            case Status::TimingClock:
            case Status::ActiveSensing:
            default: break;
            }
            break;
        }
        default: break;
        }

        return it;
    }
};

class MIDIEncoder : public IMIDIReader
{
    friend class SongConverter;
    std::vector<uint8_t> m_result;
    uint8_t m_status = 0;

    void _sendMessage(const uint8_t* data, size_t len)
    {
        if (data[0] == m_status)
        {
            for (size_t i=1 ; i<len ; ++i)
                m_result.push_back(data[i]);
        }
        else
        {
            if (data[0] & 0x80)
                m_status = data[0];
            for (size_t i=0 ; i<len ; ++i)
                m_result.push_back(data[i]);
        }
    }

    void _sendContinuedValue(uint32_t val)
    {
        uint32_t send[3] = {};
        uint32_t* ptr = nullptr;
        if (val >= 0x4000)
        {
            ptr = &send[0];
            send[0] = 0x80 | ((val / 0x4000) & 0x7f);
            send[1] = 0x80;
            val &= 0x3fff;
        }

        if (val >= 0x80)
        {
            if (!ptr)
                ptr = &send[1];
            send[1] = 0x80 | ((val / 0x80) & 0x7f);
        }

        if (!ptr)
            ptr = &send[2];
        send[2] = val & 0x7f;

        size_t len = 3 - (ptr - send);
        for (size_t i=0 ; i<len ; ++i)
            m_result.push_back(ptr[i]);
    }

public:
    void noteOff(uint8_t chan, uint8_t key, uint8_t velocity)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::NoteOff) | (chan & 0xf)),
                          uint8_t(key & 0x7f), uint8_t(velocity & 0x7f)};
        _sendMessage(cmd, 3);
    }

    void noteOn(uint8_t chan, uint8_t key, uint8_t velocity)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::NoteOn) | (chan & 0xf)),
                          uint8_t(key & 0x7f), uint8_t(velocity & 0x7f)};
        _sendMessage(cmd, 3);
    }

    void notePressure(uint8_t chan, uint8_t key, uint8_t pressure)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::NotePressure) | (chan & 0xf)),
                          uint8_t(key & 0x7f), uint8_t(pressure & 0x7f)};
        _sendMessage(cmd, 3);
    }

    void controlChange(uint8_t chan, uint8_t control, uint8_t value)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)),
                          uint8_t(control & 0x7f), uint8_t(value & 0x7f)};
        _sendMessage(cmd, 3);
    }

    void programChange(uint8_t chan, uint8_t program)
    {
        uint8_t cmd[2] = {uint8_t(int(Status::ProgramChange) | (chan & 0xf)),
                          uint8_t(program & 0x7f)};
        _sendMessage(cmd, 2);
    }

    void channelPressure(uint8_t chan, uint8_t pressure)
    {
        uint8_t cmd[2] = {uint8_t(int(Status::ChannelPressure) | (chan & 0xf)),
                          uint8_t(pressure & 0x7f)};
        _sendMessage(cmd, 2);
    }

    void pitchBend(uint8_t chan, int16_t pitch)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::PitchBend) | (chan & 0xf)),
                          uint8_t((pitch % 128) & 0x7f), uint8_t((pitch / 128) & 0x7f)};
        _sendMessage(cmd, 3);
    }


    void allSoundOff(uint8_t chan)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)),
                          120, 0};
        _sendMessage(cmd, 3);
    }

    void resetAllControllers(uint8_t chan)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)),
                          121, 0};
        _sendMessage(cmd, 3);
    }

    void localControl(uint8_t chan, bool on)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)),
                          122, uint8_t(on ? 127 : 0)};
        _sendMessage(cmd, 3);
    }

    void allNotesOff(uint8_t chan)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)),
                          123, 0};
        _sendMessage(cmd, 3);
    }

    void omniMode(uint8_t chan, bool on)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)),
                          uint8_t(on ? 125 : 124), 0};
        _sendMessage(cmd, 3);
    }

    void polyMode(uint8_t chan, bool on)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)),
                          uint8_t(on ? 127 : 126), 0};
        _sendMessage(cmd, 3);
    }


    void sysex(const void* data, size_t len)
    {
        uint8_t cmd = uint8_t(Status::SysEx);
        _sendMessage(&cmd, 1);
        _sendContinuedValue(len);
        for (size_t i=0 ; i<len ; ++i)
            m_result.push_back(reinterpret_cast<const uint8_t*>(data)[i]);
        cmd = uint8_t(Status::SysExTerm);
        _sendMessage(&cmd, 1);
    }

    void timeCodeQuarterFrame(uint8_t message, uint8_t value)
    {
        uint8_t cmd[2] = {uint8_t(int(Status::TimecodeQuarterFrame)),
                          uint8_t((message & 0x7 << 4) | (value & 0xf))};
        _sendMessage(cmd, 2);
    }

    void songPositionPointer(uint16_t pointer)
    {
        uint8_t cmd[3] = {uint8_t(int(Status::SongPositionPointer)),
                          uint8_t((pointer % 128) & 0x7f), uint8_t((pointer / 128) & 0x7f)};
        _sendMessage(cmd, 3);
    }

    void songSelect(uint8_t song)
    {
        uint8_t cmd[2] = {uint8_t(int(Status::TimecodeQuarterFrame)),
                          uint8_t(song & 0x7f)};
        _sendMessage(cmd, 2);
    }

    void tuneRequest()
    {
        uint8_t cmd = uint8_t(Status::TuneRequest);
        _sendMessage(&cmd, 1);
    }


    void startSeq()
    {
        uint8_t cmd = uint8_t(Status::Start);
        _sendMessage(&cmd, 1);
    }

    void continueSeq()
    {
        uint8_t cmd = uint8_t(Status::Continue);
        _sendMessage(&cmd, 1);
    }

    void stopSeq()
    {
        uint8_t cmd = uint8_t(Status::Stop);
        _sendMessage(&cmd, 1);
    }


    void reset()
    {
        uint8_t cmd = uint8_t(Status::Reset);
        _sendMessage(&cmd, 1);
    }

    const std::vector<uint8_t>& getResult() const {return m_result;}
    std::vector<uint8_t>& getResult() {return m_result;}
};

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

std::vector<uint8_t> SongConverter::SongToMIDI(const unsigned char* data, Target& targetOut)
{
    std::vector<uint8_t> ret = {'M', 'T', 'h', 'd'};
    uint32_t six32 = SBig(uint32_t(6));
    for (int i=0 ; i<4 ; ++i)
        ret.push_back(reinterpret_cast<uint8_t*>(&six32)[i]);

    ret.push_back(0);
    ret.push_back(1);

    SongState song;
    song.initialize(data);

    size_t trkCount = 1;
    for (std::experimental::optional<SongState::Track>& trk : song.m_tracks)
        if (trk)
            ++trkCount;

    uint16_t trkCount16 = SBig(uint16_t(trkCount));
    ret.push_back(reinterpret_cast<uint8_t*>(&trkCount16)[0]);
    ret.push_back(reinterpret_cast<uint8_t*>(&trkCount16)[1]);

    uint16_t tickDiv16 = SBig(uint16_t(384));
    ret.push_back(reinterpret_cast<uint8_t*>(&tickDiv16)[0]);
    ret.push_back(reinterpret_cast<uint8_t*>(&tickDiv16)[1]);

    /* Intermediate event */
    struct Event
    {
        bool endEvent = false;
        bool controlChange = false;
        uint8_t channel;
        uint8_t noteOrCtrl;
        uint8_t velOrVal;
        uint16_t length;

        bool isPitchBend = false;
        int16_t pitchBend;

        Event(int16_t pBend) : isPitchBend(true), pitchBend(pBend) {}

        Event(bool ctrlCh, uint8_t chan, uint8_t note, uint8_t vel, uint16_t len)
        : controlChange(ctrlCh), channel(chan), noteOrCtrl(note), velOrVal(vel), length(len) {}
    };

    /* Write tempo track */
    {
        MIDIEncoder encoder;

        /* Initial tempo */
        encoder._sendContinuedValue(0);
        encoder.getResult().push_back(0xff);
        encoder.getResult().push_back(0x51);
        encoder.getResult().push_back(3);

        uint32_t tempo24 = SBig(60000000 / song.m_tempo);
        for (int i=1 ; i<4 ; ++i)
            encoder.getResult().push_back(reinterpret_cast<uint8_t*>(&tempo24)[i]);

        /* Write out tempo changes */
        int lastTick = 0;
        while (song.m_tempoPtr && song.m_tempoPtr->m_tick != 0xffffffff)
        {
            SongState::TempoChange change = *song.m_tempoPtr;
            if (song.m_bigEndian)
                change.swapBig();

            encoder._sendContinuedValue(change.m_tick - lastTick);
            lastTick = change.m_tick;
            encoder.getResult().push_back(0xff);
            encoder.getResult().push_back(0x51);
            encoder.getResult().push_back(3);

            uint32_t tempo24 = SBig(60000000 / change.m_tempo);
            for (int i=1 ; i<4 ; ++i)
                encoder.getResult().push_back(reinterpret_cast<uint8_t*>(&tempo24)[i]);

            ++song.m_tempoPtr;
        }

        encoder.getResult().push_back(0);
        encoder.getResult().push_back(0xff);
        encoder.getResult().push_back(0x2f);
        encoder.getResult().push_back(0);

        ret.push_back('M');
        ret.push_back('T');
        ret.push_back('r');
        ret.push_back('k');
        uint32_t trkSz = SBig(uint32_t(encoder.getResult().size()));
        for (int i=0 ; i<4 ; ++i)
            ret.push_back(reinterpret_cast<uint8_t*>(&trkSz)[i]);
        ret.insert(ret.cend(), encoder.getResult().begin(), encoder.getResult().end());
    }

    /* Iterate each SNG track into type-1 MIDI track */
    for (std::experimental::optional<SongState::Track>& trk : song.m_tracks)
    {
        if (trk)
        {
            std::multimap<int, Event> events;

            /* Iterate all regions */
            while (trk->m_nextRegion->indexValid())
            {
                trk->advanceRegion(nullptr);
                uint32_t regStart = song.m_bigEndian ? SBig(trk->m_curRegion->m_startTick) : trk->m_curRegion->m_startTick;

                /* Update continuous pitch data */
                if (trk->m_pitchWheelData)
                {
                    while (true)
                    {
                        /* See if there's an upcoming pitch change in this interval */
                        const unsigned char* ptr = trk->m_pitchWheelData;
                        uint32_t deltaTicks = DecodeRLE(ptr);
                        if (deltaTicks != 0xffffffff)
                        {
                            int32_t nextTick = trk->m_lastPitchTick + deltaTicks;
                            int32_t pitchDelta = DecodeContinuousRLE(ptr);
                            trk->m_lastPitchVal += pitchDelta;
                            trk->m_pitchWheelData = ptr;
                            trk->m_lastPitchTick = nextTick;
                            events.emplace(regStart + nextTick, Event{clamp(0, trk->m_lastPitchVal / 2 + 0x2000, 0x4000)});
                        }
                        else
                            break;
                    }
                }

                /* Update continuous modulation data */
                if (trk->m_modWheelData)
                {
                    while (true)
                    {
                        /* See if there's an upcoming modulation change in this interval */
                        const unsigned char* ptr = trk->m_modWheelData;
                        uint32_t deltaTicks = DecodeRLE(ptr);
                        if (deltaTicks != 0xffffffff)
                        {
                            int32_t nextTick = trk->m_lastModTick + deltaTicks;
                            int32_t modDelta = DecodeContinuousRLE(ptr);
                            trk->m_lastModVal += modDelta;
                            trk->m_modWheelData = ptr;
                            trk->m_lastModTick = nextTick;
                            events.emplace(regStart + nextTick, Event{true, trk->m_midiChan, 1, clamp(0, trk->m_lastModVal * 128 / 16384, 127), 0});
                        }
                        else
                            break;
                    }
                }

                /* Loop through as many commands as we can for this time period */
                if (song.m_header.m_trackIdxOff == 0x18 || song.m_header.m_trackIdxOff == 0x58)
                {
                    /* GameCube */
                    while (true)
                    {
                        /* Load next command */
                        if (*reinterpret_cast<const uint16_t*>(trk->m_data) == 0xffff)
                        {
                            /* End of channel */
                            trk->m_data = nullptr;
                            break;
                        }
                        else if (trk->m_data[0] & 0x80)
                        {
                            /* Control change */
                            uint8_t val = trk->m_data[0] & 0x7f;
                            uint8_t ctrl = trk->m_data[1] & 0x7f;
                            events.emplace(regStart + trk->m_eventWaitCountdown, Event{true, trk->m_midiChan, ctrl, val, 0});
                            trk->m_data += 2;
                        }
                        else
                        {
                            /* Note */
                            uint8_t note = trk->m_data[0] & 0x7f;
                            uint8_t vel = trk->m_data[1] & 0x7f;
                            uint16_t length = (song.m_bigEndian ? SBig(*reinterpret_cast<const uint16_t*>(trk->m_data + 2)) :
                                                                       *reinterpret_cast<const uint16_t*>(trk->m_data + 2));
                            if (length)
                                events.emplace(regStart + trk->m_eventWaitCountdown, Event{false, trk->m_midiChan, note, vel, length});
                            trk->m_data += 4;
                        }

                        /* Set next delta-time */
                        trk->m_eventWaitCountdown += int32_t(DecodeTimeRLE(trk->m_data));
                    }
                }
                else
                {
                    /* N64 */
                    while (true)
                    {
                        /* Load next command */
                        if (*reinterpret_cast<const uint32_t*>(trk->m_data) == 0xffff0000)
                        {
                            /* End of channel */
                            trk->m_data = nullptr;
                            break;
                        }
                        else if (trk->m_data[0] & 0x80)
                        {
                            /* Control change */
                            uint8_t val = trk->m_data[0] & 0x7f;
                            uint8_t ctrl = trk->m_data[1] & 0x7f;
                            events.emplace(regStart + trk->m_eventWaitCountdown, Event{true, trk->m_midiChan, ctrl, val, 0});
                            trk->m_data += 2;
                        }
                        else
                        {
                            if ((trk->m_data[2] & 0x80) != 0x80)
                            {
                                /* Note */
                                uint16_t length = (song.m_bigEndian ? SBig(*reinterpret_cast<const uint16_t*>(trk->m_data)) :
                                                                           *reinterpret_cast<const uint16_t*>(trk->m_data));
                                uint8_t note = trk->m_data[2] & 0x7f;
                                uint8_t vel = trk->m_data[3] & 0x7f;
                                if (length)
                                    events.emplace(regStart + trk->m_eventWaitCountdown, Event{false, trk->m_midiChan, note, vel, length});
                            }
                            trk->m_data += 4;
                        }

                        /* Set next delta-time */
                        int32_t absTick = (song.m_bigEndian ? SBig(*reinterpret_cast<const int32_t*>(trk->m_data)) :
                                                                   *reinterpret_cast<const int32_t*>(trk->m_data));
                        trk->m_eventWaitCountdown += absTick - trk->m_lastN64EventTick;
                        trk->m_lastN64EventTick = absTick;
                        trk->m_data += 4;
                    }
                }
            }

            /* Resolve key-off events */
            std::multimap<int, Event> offEvents;
            for (auto& pair : events)
            {
                if (!pair.second.controlChange)
                {
                    auto it = offEvents.emplace(pair.first + pair.second.length, pair.second);
                    it->second.endEvent = true;
                }
            }

            /* Merge key-off events */
            events.insert(offEvents.begin(), offEvents.end());

            /* Emit MIDI events */
            MIDIEncoder encoder;
            int lastTime = 0;
            for (auto& pair : events)
            {
                encoder._sendContinuedValue(pair.first - lastTime);
                lastTime = pair.first;

                if (pair.second.controlChange)
                {
                    encoder.controlChange(pair.second.channel, pair.second.noteOrCtrl, pair.second.velOrVal);
                }
                else if (pair.second.isPitchBend)
                {
                    encoder.pitchBend(trk->m_midiChan, pair.second.pitchBend);
                }
                else
                {
                    if (pair.second.endEvent)
                        encoder.noteOff(pair.second.channel, pair.second.noteOrCtrl, pair.second.velOrVal);
                    else
                        encoder.noteOn(pair.second.channel, pair.second.noteOrCtrl, pair.second.velOrVal);
                }
            }

            encoder.getResult().push_back(0);
            encoder.getResult().push_back(0xff);
            encoder.getResult().push_back(0x2f);
            encoder.getResult().push_back(0);

            /* Write out */
            ret.push_back('M');
            ret.push_back('T');
            ret.push_back('r');
            ret.push_back('k');
            uint32_t trkSz = SBig(uint32_t(encoder.getResult().size()));
            for (int i=0 ; i<4 ; ++i)
                ret.push_back(reinterpret_cast<uint8_t*>(&trkSz)[i]);
            ret.insert(ret.cend(), encoder.getResult().begin(), encoder.getResult().end());
        }
    }

    return ret;
}

std::vector<uint8_t> SongConverter::MIDIToSong(const unsigned char* data, Target target)
{

}

}

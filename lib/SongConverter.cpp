#include "amuse/SongConverter.hpp"
#include "amuse/SongState.hpp"
#include "amuse/Common.hpp"
#include <cstdlib>
#include <map>

namespace amuse {

static inline uint8_t clamp7(uint8_t val) { return std::max(0, std::min(127, int(val))); }

enum class Status {
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

/* Event tags */
struct NoteEvent {};
struct CtrlEvent {};
struct ProgEvent {};
struct PitchEvent {};

/* Intermediate event */
struct Event {
  enum class Type : uint8_t { Note, Control, Program, Pitch } m_type;

  bool endEvent = false;
  uint8_t channel;
  uint8_t noteOrCtrl;
  uint8_t velOrVal;
  uint8_t program;
  int length;
  int pitchBend;

  Event(NoteEvent, uint8_t chan, uint8_t note, uint8_t vel, int len)
  : m_type(Type::Note), channel(chan), noteOrCtrl(note), velOrVal(vel), length(len) {}

  Event(CtrlEvent, uint8_t chan, uint8_t note, uint8_t vel, int len)
  : m_type(Type::Control), channel(chan), noteOrCtrl(note), velOrVal(vel), length(len) {}

  Event(ProgEvent, uint8_t chan, uint8_t prog) : m_type(Type::Program), channel(chan), program(prog) {}

  Event(PitchEvent, uint8_t chan, int pBend) : m_type(Type::Pitch), channel(chan), pitchBend(pBend) {}
};

class MIDIDecoder {
  int m_tick = 0;
  std::vector<std::multimap<int, Event>> m_results[16];
  std::multimap<int, int> m_tempos;
  std::array<std::multimap<int, Event>::iterator, 128> m_notes[16];
  int m_minLoopStart[16];
  int m_minLoopEnd[16];

  bool isEmptyIterator(int chan, std::multimap<int, Event>::iterator it) const {
    for (const auto& res : m_results[chan])
      if (res.end() == it)
        return true;
    return false;
  }

  void _addRegionChange(int chan) {
    auto& results = m_results[chan];
    results.reserve(2);
    results.emplace_back();
    if (results.size() == 1)
      for (size_t i = 0; i < 128; ++i)
        m_notes[chan][i] = results.back().end();
  }

  uint8_t m_status = 0;
  bool _readContinuedValue(std::vector<uint8_t>::const_iterator& it, std::vector<uint8_t>::const_iterator end,
                           uint32_t& valOut) {
    uint8_t a = *it++;
    valOut = a & 0x7f;

    if (a & 0x80) {
      if (it == end)
        return false;
      valOut <<= 7;
      a = *it++;
      valOut |= a & 0x7f;

      if (a & 0x80) {
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
  MIDIDecoder() {
    std::fill(std::begin(m_minLoopStart), std::end(m_minLoopStart), INT_MAX);
    std::fill(std::begin(m_minLoopEnd), std::end(m_minLoopEnd), INT_MAX);
  }

  std::vector<uint8_t>::const_iterator receiveBytes(std::vector<uint8_t>::const_iterator begin,
                                                    std::vector<uint8_t>::const_iterator end,
                                                    int loopStart[16] = nullptr, int loopEnd[16] = nullptr) {
    std::vector<uint8_t>::const_iterator it = begin;
    while (it != end) {
      uint32_t deltaTime;
      _readContinuedValue(it, end, deltaTime);
      m_tick += deltaTime;

      uint8_t a = *it++;
      uint8_t b;
      if (a & 0x80)
        m_status = a;
      else
        it--;

      if (m_status == 0xff) {
        /* Meta events */
        if (it == end)
          break;
        a = *it++;

        uint32_t length;
        _readContinuedValue(it, end, length);

        switch (a) {
        case 0x51: {
          uint32_t tempo = 0;
          memcpy(&reinterpret_cast<uint8_t*>(&tempo)[1], &*it, 3);
          m_tempos.emplace(m_tick, 60000000 / SBig(tempo));
        }
        default:
          it += length;
        }
      } else {
        uint8_t chan = m_status & 0xf;
        auto& results = m_results[chan];

        if (loopEnd && loopEnd[chan] != INT_MAX && m_tick >= loopEnd[chan])
          break;

        /* Split region at loop start point */
        if (loopStart && loopStart[chan] != INT_MAX && m_tick >= loopStart[chan]) {
          _addRegionChange(chan);
          loopStart[chan] = INT_MAX;
        } else if (results.empty()) {
          _addRegionChange(chan);
        }

        std::multimap<int, Event>& res = results.back();

        switch (Status(m_status & 0xf0)) {
        case Status::NoteOff: {
          if (it == end)
            break;
          a = *it++;
          if (it == end)
            break;
          b = *it++;

          uint8_t notenum = clamp7(a);
          std::multimap<int, Event>::iterator note = m_notes[chan][notenum];
          if (!isEmptyIterator(chan, note)) {
            note->second.length = m_tick - note->first;
            m_notes[chan][notenum] = res.end();
          }
          break;
        }
        case Status::NoteOn: {
          if (it == end)
            break;
          a = *it++;
          if (it == end)
            break;
          b = *it++;

          uint8_t notenum = clamp7(a);
          uint8_t vel = clamp7(b);
          std::multimap<int, Event>::iterator note = m_notes[chan][notenum];
          if (!isEmptyIterator(chan, note))
            note->second.length = m_tick - note->first;

          if (vel != 0)
            m_notes[chan][notenum] = res.emplace(m_tick, Event{NoteEvent{}, chan, notenum, vel, 0});
          else
            m_notes[chan][notenum] = res.end();

          break;
        }
        case Status::NotePressure: {
          if (it == end)
            break;
          a = *it++;
          if (it == end)
            break;
          b = *it++;
          break;
        }
        case Status::ControlChange: {
          if (it == end)
            break;
          a = *it++;
          if (it == end)
            break;
          b = *it++;
          if (a == 0x66)
            m_minLoopStart[chan] = std::min(m_tick, m_minLoopStart[chan]);
          else if (a == 0x67)
            m_minLoopEnd[chan] = std::min(m_tick, m_minLoopEnd[chan]);
          else
            res.emplace(m_tick, Event{CtrlEvent{}, chan, clamp7(a), clamp7(b), 0});
          break;
        }
        case Status::ProgramChange: {
          if (it == end)
            break;
          a = *it++;
          res.emplace(m_tick, Event{ProgEvent{}, chan, a});
          break;
        }
        case Status::ChannelPressure: {
          if (it == end)
            break;
          a = *it++;
          break;
        }
        case Status::PitchBend: {
          if (it == end)
            break;
          a = *it++;
          if (it == end)
            break;
          b = *it++;
          res.emplace(m_tick, Event{PitchEvent{}, chan, clamp7(b) * 128 + clamp7(a)});
          break;
        }
        case Status::SysEx: {
          switch (Status(m_status & 0xff)) {
          case Status::SysEx: {
            uint32_t len;
            if (!_readContinuedValue(it, end, len) || end - it < len)
              break;
            break;
          }
          case Status::TimecodeQuarterFrame: {
            if (it == end)
              break;
            a = *it++;
            break;
          }
          case Status::SongPositionPointer: {
            if (it == end)
              break;
            a = *it++;
            if (it == end)
              break;
            b = *it++;
            break;
          }
          case Status::SongSelect: {
            if (it == end)
              break;
            a = *it++;
            break;
          }
          case Status::TuneRequest:
          case Status::Start:
          case Status::Continue:
          case Status::Stop:
          case Status::Reset:
          case Status::SysExTerm:
          case Status::TimingClock:
          case Status::ActiveSensing:
          default:
            break;
          }
          break;
        }
        default:
          break;
        }
      }
    }

    return it;
  }

  std::vector<std::multimap<int, Event>>& getResults(int chan) { return m_results[chan]; }
  std::multimap<int, int>& getTempos() { return m_tempos; }
  int getMinLoopStart(int chan) const { return m_minLoopStart[chan]; }
  int getMinLoopEnd(int chan) const { return m_minLoopEnd[chan]; }
};

class MIDIEncoder {
  friend class SongConverter;
  std::vector<uint8_t> m_result;
  uint8_t m_status = 0;

  void _sendMessage(const uint8_t* data, size_t len) {
    if (data[0] == m_status) {
      for (size_t i = 1; i < len; ++i)
        m_result.push_back(data[i]);
    } else {
      if (data[0] & 0x80)
        m_status = data[0];
      for (size_t i = 0; i < len; ++i)
        m_result.push_back(data[i]);
    }
  }

  void _sendContinuedValue(uint32_t val) {
    uint8_t send[3] = {};
    uint8_t* ptr = nullptr;
    if (val >= 0x4000) {
      ptr = &send[0];
      send[0] = 0x80 | ((val / 0x4000) & 0x7f);
      send[1] = 0x80;
      val &= 0x3fff;
    }

    if (val >= 0x80) {
      if (!ptr)
        ptr = &send[1];
      send[1] = 0x80 | ((val / 0x80) & 0x7f);
    }

    if (!ptr)
      ptr = &send[2];
    send[2] = val & 0x7f;

    size_t len = 3 - (ptr - send);
    for (size_t i = 0; i < len; ++i)
      m_result.push_back(ptr[i]);
  }

public:
  void noteOff(uint8_t chan, uint8_t key, uint8_t velocity) {
    uint8_t cmd[3] = {uint8_t(int(Status::NoteOff) | (chan & 0xf)), uint8_t(key & 0x7f), uint8_t(velocity & 0x7f)};
    _sendMessage(cmd, 3);
  }

  void noteOn(uint8_t chan, uint8_t key, uint8_t velocity) {
    uint8_t cmd[3] = {uint8_t(int(Status::NoteOn) | (chan & 0xf)), uint8_t(key & 0x7f), uint8_t(velocity & 0x7f)};
    _sendMessage(cmd, 3);
  }

  void notePressure(uint8_t chan, uint8_t key, uint8_t pressure) {
    uint8_t cmd[3] = {uint8_t(int(Status::NotePressure) | (chan & 0xf)), uint8_t(key & 0x7f), uint8_t(pressure & 0x7f)};
    _sendMessage(cmd, 3);
  }

  void controlChange(uint8_t chan, uint8_t control, uint8_t value) {
    uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)), uint8_t(control & 0x7f),
                      uint8_t(value & 0x7f)};
    _sendMessage(cmd, 3);
  }

  void programChange(uint8_t chan, uint8_t program) {
    uint8_t cmd[2] = {uint8_t(int(Status::ProgramChange) | (chan & 0xf)), uint8_t(program & 0x7f)};
    _sendMessage(cmd, 2);
  }

  void channelPressure(uint8_t chan, uint8_t pressure) {
    uint8_t cmd[2] = {uint8_t(int(Status::ChannelPressure) | (chan & 0xf)), uint8_t(pressure & 0x7f)};
    _sendMessage(cmd, 2);
  }

  void pitchBend(uint8_t chan, int16_t pitch) {
    uint8_t cmd[3] = {uint8_t(int(Status::PitchBend) | (chan & 0xf)), uint8_t((pitch % 128) & 0x7f),
                      uint8_t((pitch / 128) & 0x7f)};
    _sendMessage(cmd, 3);
  }

  void allSoundOff(uint8_t chan) {
    uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)), 120, 0};
    _sendMessage(cmd, 3);
  }

  void resetAllControllers(uint8_t chan) {
    uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)), 121, 0};
    _sendMessage(cmd, 3);
  }

  void localControl(uint8_t chan, bool on) {
    uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)), 122, uint8_t(on ? 127 : 0)};
    _sendMessage(cmd, 3);
  }

  void allNotesOff(uint8_t chan) {
    uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)), 123, 0};
    _sendMessage(cmd, 3);
  }

  void omniMode(uint8_t chan, bool on) {
    uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)), uint8_t(on ? 125 : 124), 0};
    _sendMessage(cmd, 3);
  }

  void polyMode(uint8_t chan, bool on) {
    uint8_t cmd[3] = {uint8_t(int(Status::ControlChange) | (chan & 0xf)), uint8_t(on ? 127 : 126), 0};
    _sendMessage(cmd, 3);
  }

  void sysex(const void* data, size_t len) {
    uint8_t cmd = uint8_t(Status::SysEx);
    _sendMessage(&cmd, 1);
    _sendContinuedValue(len);
    for (size_t i = 0; i < len; ++i)
      m_result.push_back(reinterpret_cast<const uint8_t*>(data)[i]);
    cmd = uint8_t(Status::SysExTerm);
    _sendMessage(&cmd, 1);
  }

  void timeCodeQuarterFrame(uint8_t message, uint8_t value) {
    uint8_t cmd[2] = {uint8_t(int(Status::TimecodeQuarterFrame)), uint8_t((message & 0x7 << 4) | (value & 0xf))};
    _sendMessage(cmd, 2);
  }

  void songPositionPointer(uint16_t pointer) {
    uint8_t cmd[3] = {uint8_t(int(Status::SongPositionPointer)), uint8_t((pointer % 128) & 0x7f),
                      uint8_t((pointer / 128) & 0x7f)};
    _sendMessage(cmd, 3);
  }

  void songSelect(uint8_t song) {
    uint8_t cmd[2] = {uint8_t(int(Status::TimecodeQuarterFrame)), uint8_t(song & 0x7f)};
    _sendMessage(cmd, 2);
  }

  void tuneRequest() {
    uint8_t cmd = uint8_t(Status::TuneRequest);
    _sendMessage(&cmd, 1);
  }

  void startSeq() {
    uint8_t cmd = uint8_t(Status::Start);
    _sendMessage(&cmd, 1);
  }

  void continueSeq() {
    uint8_t cmd = uint8_t(Status::Continue);
    _sendMessage(&cmd, 1);
  }

  void stopSeq() {
    uint8_t cmd = uint8_t(Status::Stop);
    _sendMessage(&cmd, 1);
  }

  void reset() {
    uint8_t cmd = uint8_t(Status::Reset);
    _sendMessage(&cmd, 1);
  }

  const std::vector<uint8_t>& getResult() const { return m_result; }
  std::vector<uint8_t>& getResult() { return m_result; }
};

static uint16_t DecodeUnsignedValue(const unsigned char*& data) {
  uint16_t ret;
  if (data[0] & 0x80) {
    ret = data[1] | ((data[0] & 0x7f) << 8);
    data += 2;
  } else {
    ret = data[0];
    data += 1;
  }
  return ret;
}

static void EncodeUnsignedValue(std::vector<uint8_t>& vecOut, uint16_t val) {
  if (val >= 128) {
    vecOut.push_back(0x80 | ((val >> 8) & 0x7f));
    vecOut.push_back(val & 0xff);
  } else {
    vecOut.push_back(val & 0x7f);
  }
}

static int16_t DecodeSignedValue(const unsigned char*& data) {
  int16_t ret;
  if (data[0] & 0x80) {
    ret = data[1] | ((data[0] & 0x7f) << 8);
    ret |= ((ret << 1) & 0x8000);
    data += 2;
  } else {
    ret = int8_t(data[0] | ((data[0] << 1) & 0x80));
    data += 1;
  }
  return ret;
}

static void EncodeSignedValue(std::vector<uint8_t>& vecOut, int16_t val) {
  if (val >= 64 || val < -64) {
    vecOut.push_back(0x80 | ((val >> 8) & 0x7f));
    vecOut.push_back(val & 0xff);
  } else {
    vecOut.push_back(val & 0x7f);
  }
}

static std::pair<uint32_t, int32_t> DecodeDelta(const unsigned char*& data) {
  std::pair<uint32_t, int32_t> ret = {};
  do {
    if (data[0] == 0x80 && data[1] == 0x00)
      break;
    ret.first += DecodeUnsignedValue(data);
    ret.second = DecodeSignedValue(data);
  } while (ret.second == 0);
  return ret;
}

static void EncodeDelta(std::vector<uint8_t>& vecOut, uint32_t deltaTime, int32_t val) {
  while (deltaTime > 32767) {
    EncodeUnsignedValue(vecOut, 32767);
    EncodeSignedValue(vecOut, 0);
    deltaTime -= 32767;
  }
  EncodeUnsignedValue(vecOut, deltaTime);
  EncodeSignedValue(vecOut, val);
}

static uint32_t DecodeTime(const unsigned char*& data) {
  uint32_t ret = 0;

  while (true) {
    uint16_t thisPart = SBig(*reinterpret_cast<const uint16_t*>(data));
    uint16_t nextPart = *reinterpret_cast<const uint16_t*>(data + 2);
    if (nextPart == 0) {
      // Automatically consume no-op command as continued time
      ret += thisPart;
      data += 4;
      continue;
    }

    ret += thisPart;
    data += 2;
    break;
  }

  return ret;
}

static void EncodeTime(std::vector<uint8_t>& vecOut, uint32_t val) {
  while (val >= 65535) {
    // Automatically emit no-op command as continued time
    vecOut.push_back(0xff);
    vecOut.push_back(0xff);
    vecOut.push_back(0);
    vecOut.push_back(0);
    val -= 65535;
  }

  uint16_t lastPart = SBig(uint16_t(val));
  vecOut.push_back(reinterpret_cast<const uint8_t*>(&lastPart)[0]);
  vecOut.push_back(reinterpret_cast<const uint8_t*>(&lastPart)[1]);
}

std::vector<uint8_t> SongConverter::SongToMIDI(const unsigned char* data, int& versionOut, bool& isBig) {
  std::vector<uint8_t> ret = {'M', 'T', 'h', 'd'};
  uint32_t six32 = SBig(uint32_t(6));
  for (int i = 0; i < 4; ++i)
    ret.push_back(reinterpret_cast<uint8_t*>(&six32)[i]);

  ret.push_back(0);
  ret.push_back(1);

  SongState song;
  if (!song.initialize(data, false))
    return {};
  versionOut = song.m_sngVersion;
  isBig = song.m_bigEndian;

  size_t trkCount = 1;
  for (SongState::Track& trk : song.m_tracks)
    if (trk)
      ++trkCount;

  uint16_t trkCount16 = SBig(uint16_t(trkCount));
  ret.push_back(reinterpret_cast<uint8_t*>(&trkCount16)[0]);
  ret.push_back(reinterpret_cast<uint8_t*>(&trkCount16)[1]);

  uint16_t tickDiv16 = SBig(uint16_t(384));
  ret.push_back(reinterpret_cast<uint8_t*>(&tickDiv16)[0]);
  ret.push_back(reinterpret_cast<uint8_t*>(&tickDiv16)[1]);

  /* Write tempo track */
  {
    MIDIEncoder encoder;

    /* Initial tempo */
    encoder._sendContinuedValue(0);
    encoder.getResult().push_back(0xff);
    encoder.getResult().push_back(0x51);
    encoder.getResult().push_back(3);

    uint32_t tempo24 = SBig(60000000 / (song.m_header.m_initialTempo & 0x7fffffff));
    for (int i = 1; i < 4; ++i)
      encoder.getResult().push_back(reinterpret_cast<uint8_t*>(&tempo24)[i]);

    /* Write out tempo changes */
    int lastTick = 0;
    const SongState::TempoChange* tempoPtr = nullptr;
    if (song.m_header.m_tempoTableOff)
      tempoPtr = reinterpret_cast<const SongState::TempoChange*>(song.m_songData + song.m_header.m_tempoTableOff);
    while (tempoPtr && tempoPtr->m_tick != 0xffffffff) {
      SongState::TempoChange change = *tempoPtr;
      if (song.m_bigEndian)
        change.swapBig();

      encoder._sendContinuedValue(change.m_tick - lastTick);
      lastTick = change.m_tick;
      encoder.getResult().push_back(0xff);
      encoder.getResult().push_back(0x51);
      encoder.getResult().push_back(3);

      uint32_t tempo24 = SBig(60000000 / (change.m_tempo & 0x7fffffff));
      for (int i = 1; i < 4; ++i)
        encoder.getResult().push_back(reinterpret_cast<uint8_t*>(&tempo24)[i]);

      ++tempoPtr;
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
    for (int i = 0; i < 4; ++i)
      ret.push_back(reinterpret_cast<uint8_t*>(&trkSz)[i]);
    ret.insert(ret.cend(), encoder.getResult().begin(), encoder.getResult().end());
  }

  bool loopsAdded = false;

  /* Iterate each SNG track into type-1 MIDI track */
  for (SongState::Track& trk : song.m_tracks) {
    if (trk) {
      MIDIEncoder encoder;
      std::multimap<int, Event> allEvents;

      /* Iterate all regions */
      while (trk.m_nextRegion->indexValid(song.m_bigEndian)) {
        std::multimap<int, Event> events;
        trk.advanceRegion();
        uint32_t regStart = song.m_bigEndian ? SBig(trk.m_curRegion->m_startTick) : trk.m_curRegion->m_startTick;

        /* Initial program change */
        if (trk.m_curRegion->m_progNum != 0xff)
          events.emplace(regStart, Event{ProgEvent{}, trk.m_midiChan, trk.m_curRegion->m_progNum});

        /* Update continuous pitch data */
        if (trk.m_pitchWheelData) {
          while (true) {
            /* Update pitch */
            trk.m_pitchVal += trk.m_nextPitchDelta;
            events.emplace(regStart + trk.m_nextPitchTick,
                           Event{PitchEvent{}, trk.m_midiChan, clamp(0, trk.m_pitchVal + 0x2000, 0x4000)});
            if (trk.m_pitchWheelData[0] != 0x80 || trk.m_pitchWheelData[1] != 0x00) {
              auto delta = DecodeDelta(trk.m_pitchWheelData);
              trk.m_nextPitchTick += delta.first;
              trk.m_nextPitchDelta = delta.second;
            } else {
              break;
            }
          }
        }

        /* Update continuous modulation data */
        if (trk.m_modWheelData) {
          while (true) {
            /* Update modulation */
            trk.m_modVal += trk.m_nextModDelta;
            events.emplace(regStart + trk.m_nextModTick,
                           Event{CtrlEvent{}, trk.m_midiChan, 1, uint8_t(clamp(0, trk.m_modVal / 128, 127)), 0});
            if (trk.m_modWheelData[0] != 0x80 || trk.m_modWheelData[1] != 0x00) {
              auto delta = DecodeDelta(trk.m_modWheelData);
              trk.m_nextModTick += delta.first;
              trk.m_nextModDelta = delta.second;
            } else {
              break;
            }
          }
        }

        /* Loop through as many commands as we can for this time period */
        if (song.m_sngVersion == 1) {
          /* Revision */
          while (true) {
            /* Load next command */
            if (*reinterpret_cast<const uint16_t*>(trk.m_data) == 0xffff) {
              /* End of channel */
              trk.m_data = nullptr;
              break;
            } else if (trk.m_data[0] & 0x80 && trk.m_data[1] & 0x80) {
              /* Control change */
              uint8_t val = trk.m_data[0] & 0x7f;
              uint8_t ctrl = trk.m_data[1] & 0x7f;
              events.emplace(regStart + trk.m_eventWaitCountdown, Event{CtrlEvent{}, trk.m_midiChan, ctrl, val, 0});
              trk.m_data += 2;
            } else if (trk.m_data[0] & 0x80) {
              /* Program change */
              uint8_t prog = trk.m_data[0] & 0x7f;
              events.emplace(regStart + trk.m_eventWaitCountdown, Event{ProgEvent{}, trk.m_midiChan, prog});
              trk.m_data += 2;
            } else {
              /* Note */
              uint8_t note = trk.m_data[0] & 0x7f;
              uint8_t vel = trk.m_data[1] & 0x7f;
              uint16_t length = (song.m_bigEndian ? SBig(*reinterpret_cast<const uint16_t*>(trk.m_data + 2))
                                                  : *reinterpret_cast<const uint16_t*>(trk.m_data + 2));
              events.emplace(regStart + trk.m_eventWaitCountdown,
                             Event{NoteEvent{}, trk.m_midiChan, note, vel, length});
              trk.m_data += 4;
            }

            /* Set next delta-time */
            trk.m_eventWaitCountdown += int32_t(DecodeTime(trk.m_data));
          }
        } else {
          /* Legacy */
          while (true) {
            /* Load next command */
            if (*reinterpret_cast<const uint16_t*>(&trk.m_data[2]) == 0xffff) {
              /* End of channel */
              trk.m_data = nullptr;
              break;
            } else {
              if ((trk.m_data[2] & 0x80) != 0x80) {
                /* Note */
                uint16_t length = (song.m_bigEndian ? SBig(*reinterpret_cast<const uint16_t*>(trk.m_data))
                                                    : *reinterpret_cast<const uint16_t*>(trk.m_data));
                uint8_t note = trk.m_data[2] & 0x7f;
                uint8_t vel = trk.m_data[3] & 0x7f;
                events.emplace(regStart + trk.m_eventWaitCountdown,
                               Event{NoteEvent{}, trk.m_midiChan, note, vel, length});
              } else if (trk.m_data[2] & 0x80 && trk.m_data[3] & 0x80) {
                /* Control change */
                uint8_t val = trk.m_data[2] & 0x7f;
                uint8_t ctrl = trk.m_data[3] & 0x7f;
                events.emplace(regStart + trk.m_eventWaitCountdown, Event{CtrlEvent{}, trk.m_midiChan, ctrl, val, 0});
              } else if (trk.m_data[2] & 0x80) {
                /* Program change */
                uint8_t prog = trk.m_data[2] & 0x7f;
                events.emplace(regStart + trk.m_eventWaitCountdown, Event{ProgEvent{}, trk.m_midiChan, prog});
              }
              trk.m_data += 4;
            }

            /* Set next delta-time */
            int32_t absTick = (song.m_bigEndian ? SBig(*reinterpret_cast<const int32_t*>(trk.m_data))
                                                : *reinterpret_cast<const int32_t*>(trk.m_data));
            trk.m_eventWaitCountdown += absTick - trk.m_lastN64EventTick;
            trk.m_lastN64EventTick = absTick;
            trk.m_data += 4;
          }
        }

        /* Merge events */
        allEvents.insert(events.begin(), events.end());

        /* Resolve key-off events */
        for (auto& pair : events) {
          if (pair.second.m_type == Event::Type::Note) {
            auto it = allEvents.emplace(pair.first + pair.second.length, pair.second);
            it->second.endEvent = true;
          }
        }
      }

      /* Add loop events */
      if (!loopsAdded && trk.m_nextRegion->indexLoop(song.m_bigEndian) != -1) {
        uint32_t loopEnd = song.m_bigEndian ? SBig(trk.m_nextRegion->m_startTick) : trk.m_nextRegion->m_startTick;
        allEvents.emplace(trk.m_loopStartTick, Event{CtrlEvent{}, trk.m_midiChan, 0x66, 0, 0});
        allEvents.emplace(loopEnd, Event{CtrlEvent{}, trk.m_midiChan, 0x67, 0, 0});
        if (!(song.m_header.m_initialTempo & 0x80000000))
          loopsAdded = true;
      }

      /* Emit MIDI events */
      int lastTime = 0;
      for (auto& pair : allEvents) {
        encoder._sendContinuedValue(pair.first - lastTime);
        lastTime = pair.first;

        switch (pair.second.m_type) {
        case Event::Type::Control:
          encoder.controlChange(pair.second.channel, pair.second.noteOrCtrl, pair.second.velOrVal);
          break;
        case Event::Type::Program:
          encoder.programChange(trk.m_midiChan, pair.second.program);
          break;
        case Event::Type::Pitch:
          encoder.pitchBend(trk.m_midiChan, pair.second.pitchBend);
          break;
        case Event::Type::Note:
          if (pair.second.endEvent)
            encoder.noteOff(pair.second.channel, pair.second.noteOrCtrl, pair.second.velOrVal);
          else
            encoder.noteOn(pair.second.channel, pair.second.noteOrCtrl, pair.second.velOrVal);
          break;
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
      for (int i = 0; i < 4; ++i)
        ret.push_back(reinterpret_cast<uint8_t*>(&trkSz)[i]);
      ret.insert(ret.cend(), encoder.getResult().begin(), encoder.getResult().end());
    }
  }

  return ret;
}

std::vector<uint8_t> SongConverter::MIDIToSong(const std::vector<uint8_t>& data, int version, bool big) {
  std::vector<uint8_t> ret;
  std::vector<uint8_t>::const_iterator it = data.cbegin();

  struct MIDIHeader {
    char magic[4];
    uint32_t length;
    uint16_t type;
    uint16_t count;
    uint16_t div;

    void swapBig() {
      length = SBig(length);
      type = SBig(type);
      count = SBig(count);
      div = SBig(div);
    }
  };

  MIDIHeader header = *reinterpret_cast<const MIDIHeader*>(&*it);
  header.swapBig();
  it += 8 + header.length;

  if (memcmp(header.magic, "MThd", 4))
    return {};

  /* Only Type 0 and 1 MIDI files supported as input */
  if (header.type == 0)
    header.count = 1;
  else if (header.type != 1)
    return {};

  std::vector<uint32_t> trackRegionIdxArr;
  std::vector<uint32_t> regionDataIdxArr;
  std::vector<SongState::TrackRegion> regionBuf;
  uint32_t initTempo = 120;
  std::vector<std::pair<uint32_t, uint32_t>> tempoBuf;

  std::array<uint8_t, 64> chanMap;
  for (int i = 0; i < 64; ++i)
    chanMap[i] = 0xff;

  struct Region {
    std::vector<uint8_t> eventBuf;
    std::vector<uint8_t> pitchBuf;
    std::vector<uint8_t> modBuf;
    int padding = 0;

    bool operator==(const Region& other) const {
      if (eventBuf.size() != other.eventBuf.size())
        return false;
      if (pitchBuf.size() != other.pitchBuf.size())
        return false;
      if (modBuf.size() != other.modBuf.size())
        return false;

      if (eventBuf.size() && memcmp(eventBuf.data(), other.eventBuf.data(), eventBuf.size()))
        return false;
      if (pitchBuf.size() && memcmp(pitchBuf.data(), other.pitchBuf.data(), pitchBuf.size()))
        return false;
      if (modBuf.size() && memcmp(modBuf.data(), other.modBuf.data(), modBuf.size()))
        return false;

      return true;
    }
  };
  std::vector<Region> regions;
  int curRegionOff = 0;

  /* Pre-iterate to extract loop events */
  int loopStart[16];
  int loopEnd[16];
  int loopChanCount = 0;
  {
    int loopChanIdx = -1;
    for (int c = 0; c < 16; ++c) {
      loopStart[c] = INT_MAX;
      loopEnd[c] = INT_MAX;
      std::vector<uint8_t>::const_iterator tmpIt = it;
      for (int i = 0; i < header.count; ++i) {
        if (memcmp(&*tmpIt, "MTrk", 4))
          return {};
        tmpIt += 4;
        uint32_t length = SBig(*reinterpret_cast<const uint32_t*>(&*tmpIt));
        tmpIt += 4;

        std::vector<uint8_t>::const_iterator begin = tmpIt;
        std::vector<uint8_t>::const_iterator end = tmpIt + length;
        tmpIt = end;

        MIDIDecoder dec;
        dec.receiveBytes(begin, end);
        loopStart[c] = std::min(dec.getMinLoopStart(c), loopStart[c]);
        loopEnd[c] = std::min(dec.getMinLoopEnd(c), loopEnd[c]);
      }
      if (loopStart[c] == INT_MAX || loopEnd[c] == INT_MAX) {
        loopStart[c] = INT_MAX;
        loopEnd[c] = INT_MAX;
      } else {
        ++loopChanCount;
        loopChanIdx = c;
      }
    }
    if (loopChanCount == 1) {
      for (int c = 0; c < 16; ++c) {
        loopStart[c] = loopStart[loopChanIdx];
        loopEnd[c] = loopEnd[loopChanIdx];
      }
    }
  }

  for (int i = 0; i < header.count; ++i) {
    if (memcmp(&*it, "MTrk", 4))
      return {};
    it += 4;
    uint32_t length = SBig(*reinterpret_cast<const uint32_t*>(&*it));
    it += 4;

    if (i == 0) {
      /* Extract tempo events from first track */
      std::vector<uint8_t>::const_iterator begin = it;
      std::vector<uint8_t>::const_iterator end = it + length;

      MIDIDecoder dec;
      dec.receiveBytes(begin, end);

      std::multimap<int, int>& tempos = dec.getTempos();
      if (tempos.size() == 1)
        initTempo = tempos.begin()->second;
      else if (tempos.size() > 1) {
        auto it = tempos.begin();
        initTempo = it->second;
        ++it;
        for (auto& pair : tempos) {
          if (big)
            tempoBuf.emplace_back(SBig(uint32_t(pair.first * 384 / header.div)), SBig(uint32_t(pair.second)));
          else
            tempoBuf.emplace_back(pair.first * 384 / header.div, pair.second);
        }
      }

      if (header.type == 1) {
        it = end;
        continue;
      }
    }

    /* Extract channel events */
    std::vector<uint8_t>::const_iterator begin = it;
    std::vector<uint8_t>::const_iterator end = it + length;
    it = end;

    MIDIDecoder dec;
    int tmpLoopStart[16];
    int tmpLoopEnd[16];
    std::copy(std::begin(loopStart), std::end(loopStart), std::begin(tmpLoopStart));
    std::copy(std::begin(loopEnd), std::end(loopEnd), std::begin(tmpLoopEnd));
    dec.receiveBytes(begin, end, tmpLoopStart, tmpLoopEnd);

    for (int c = 0; c < 16; ++c) {
      std::vector<std::multimap<int, Event>>& results = dec.getResults(c);
      bool didChanInit = false;
      int lastEventTick = 0;
      for (auto& chanRegion : results) {
        bool didInit = false;
        int startTick = 0;
        lastEventTick = 0;
        int lastPitchTick = 0;
        int lastPitchVal = 0;
        int lastModTick = 0;
        int lastModVal = 0;
        Region region;

        for (auto& event : chanRegion) {
          uint32_t eventTick = event.first * 384 / header.div;

          if (event.second.channel == c) {
            if (!didInit) {
              didInit = true;
              startTick = eventTick;
              lastEventTick = startTick;
              lastPitchTick = startTick;
              lastPitchVal = 0;
              lastModTick = startTick;
              lastModVal = 0;
            }

            switch (event.second.m_type) {
            case Event::Type::Control: {
              if (event.second.noteOrCtrl == 1) {
                int newMod = event.second.velOrVal * 128;
                EncodeDelta(region.modBuf, eventTick - lastModTick, newMod - lastModVal);
                lastModTick = eventTick;
                lastModVal = newMod;
              } else {
                if (version == 1) {
                  EncodeTime(region.eventBuf, uint32_t(eventTick - lastEventTick));
                  lastEventTick = eventTick;
                  region.eventBuf.push_back(0x80 | event.second.velOrVal);
                  region.eventBuf.push_back(0x80 | event.second.noteOrCtrl);
                } else {
                  if (big) {
                    uint32_t tickBig = SBig(uint32_t(eventTick - startTick));
                    for (int i = 0; i < 4; ++i)
                      region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&tickBig)[i]);
                    region.eventBuf.push_back(0x80 | event.second.velOrVal);
                    region.eventBuf.push_back(0x80 | event.second.noteOrCtrl);
                  } else {
                    uint32_t tick = uint32_t(eventTick - startTick);
                    for (int i = 0; i < 4; ++i)
                      region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&tick)[i]);
                    region.eventBuf.push_back(0x80 | event.second.velOrVal);
                    region.eventBuf.push_back(0x80 | event.second.noteOrCtrl);
                  }
                }
              }
              break;
            }
            case Event::Type::Program: {
              if (version == 1) {
                EncodeTime(region.eventBuf, uint32_t(eventTick - lastEventTick));
                lastEventTick = eventTick;
                region.eventBuf.push_back(0x80 | event.second.program);
                region.eventBuf.push_back(0);
              } else {
                if (big) {
                  uint32_t tickBig = SBig(uint32_t(eventTick - startTick));
                  for (int i = 0; i < 4; ++i)
                    region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&tickBig)[i]);
                  region.eventBuf.push_back(0x80 | event.second.program);
                  region.eventBuf.push_back(0);
                } else {
                  uint32_t tick = uint32_t(eventTick - startTick);
                  for (int i = 0; i < 4; ++i)
                    region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&tick)[i]);
                  region.eventBuf.push_back(0x80 | event.second.program);
                  region.eventBuf.push_back(0);
                }
              }
              break;
            }
            case Event::Type::Pitch: {
              int newPitch = event.second.pitchBend - 0x2000;
              EncodeDelta(region.pitchBuf, eventTick - lastPitchTick, newPitch - lastPitchVal);
              lastPitchTick = eventTick;
              lastPitchVal = newPitch;
              break;
            }
            case Event::Type::Note: {
              int lenTicks = event.second.length * 384 / header.div;
              if (version == 1) {
                EncodeTime(region.eventBuf, uint32_t(eventTick - lastEventTick));
                lastEventTick = eventTick;
                region.eventBuf.push_back(event.second.noteOrCtrl);
                region.eventBuf.push_back(event.second.velOrVal);
                uint16_t lenBig = SBig(uint16_t(lenTicks));
                region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&lenBig)[0]);
                region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&lenBig)[1]);
              } else {
                if (big) {
                  uint32_t tickBig = SBig(uint32_t(eventTick - startTick));
                  for (int i = 0; i < 4; ++i)
                    region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&tickBig)[i]);
                  uint16_t lenBig = SBig(uint16_t(lenTicks));
                  region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&lenBig)[0]);
                  region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&lenBig)[1]);
                  region.eventBuf.push_back(event.second.noteOrCtrl);
                  region.eventBuf.push_back(event.second.velOrVal);
                } else {
                  uint32_t tick = uint32_t(eventTick - startTick);
                  for (int i = 0; i < 4; ++i)
                    region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&tick)[i]);
                  uint16_t len = uint16_t(lenTicks);
                  region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&len)[0]);
                  region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&len)[1]);
                  region.eventBuf.push_back(event.second.noteOrCtrl);
                  region.eventBuf.push_back(event.second.velOrVal);
                }
              }
              break;
            }
            }
          }
        }

        if (didInit) {
          if (!didChanInit) {
            didChanInit = true;
            if (trackRegionIdxArr.size() == 64)
              return {};
            chanMap[trackRegionIdxArr.size()] = c;
            trackRegionIdxArr.push_back(regionBuf.size());
          }

          /* Terminate region */
          if (version == 1) {
            size_t pitchDelta = 0;
            size_t modDelta = 0;
            if (lastPitchTick > lastEventTick)
              pitchDelta = lastPitchTick - lastEventTick;
            if (lastModTick > lastEventTick)
              modDelta = lastModTick - lastEventTick;

            EncodeTime(region.eventBuf, std::max(pitchDelta, modDelta));
            region.eventBuf.push_back(0xff);
            region.eventBuf.push_back(0xff);
          } else {
            if (big) {
              uint32_t selTick =
                  std::max(std::max(lastEventTick - startTick, lastPitchTick - startTick), lastModTick - startTick);
              uint32_t tickBig = SBig(uint32_t(selTick));
              for (int i = 0; i < 4; ++i)
                region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&tickBig)[i]);
              region.eventBuf.push_back(0);
              region.eventBuf.push_back(0);
              region.eventBuf.push_back(0xff);
              region.eventBuf.push_back(0xff);
            } else {
              uint32_t selTick =
                  std::max(std::max(lastEventTick - startTick, lastPitchTick - startTick), lastModTick - startTick);
              uint32_t tick = uint32_t(selTick);
              for (int i = 0; i < 4; ++i)
                region.eventBuf.push_back(reinterpret_cast<const uint8_t*>(&tick)[i]);
              region.eventBuf.push_back(0);
              region.eventBuf.push_back(0);
              region.eventBuf.push_back(0xff);
              region.eventBuf.push_back(0xff);
            }
          }

          if (region.pitchBuf.size()) {
            region.pitchBuf.push_back(0x80);
            region.pitchBuf.push_back(0);
          }

          if (region.modBuf.size()) {
            region.modBuf.push_back(0x80);
            region.modBuf.push_back(0);
          }

          /* See if there's a matching region buffer already present */
          int regIdx = 0;
          for (Region& reg : regions) {
            if (reg == region)
              break;
            ++regIdx;
          }
          if (regIdx == regions.size()) {
            regionDataIdxArr.push_back(curRegionOff);
            curRegionOff += 12 + region.eventBuf.size() + region.pitchBuf.size() + region.modBuf.size();
            int paddedRegOff = ((curRegionOff + 3) & ~3);
            region.padding = paddedRegOff - curRegionOff;
            curRegionOff = paddedRegOff;
            regions.push_back(std::move(region));
          }

          /* Region header */
          regionBuf.emplace_back();
          SongState::TrackRegion& reg = regionBuf.back();
          if (big) {
            reg.m_startTick = SBig(uint32_t(startTick));
            reg.m_progNum = 0xff;
            reg.m_unk1 = 0xff;
            reg.m_unk2 = 0;
            reg.m_regionIndex = SBig(uint16_t(regIdx));
            reg.m_loopToRegion = 0;
          } else {
            reg.m_startTick = uint32_t(startTick);
            reg.m_progNum = 0xff;
            reg.m_unk1 = 0xff;
            reg.m_unk2 = 0;
            reg.m_regionIndex = uint16_t(regIdx);
            reg.m_loopToRegion = 0;
          }
        }
      }

      if (didChanInit) {
        /* Terminating region header */
        regionBuf.emplace_back();
        SongState::TrackRegion& reg = regionBuf.back();

        uint32_t termStartTick = 0;
        int16_t termRegionIdx = -1;
        int16_t termLoopToRegion = 0;
        if (loopEnd[c] != INT_MAX) {
          termStartTick = loopEnd[c];
          if (lastEventTick >= loopStart[c]) {
            termRegionIdx = -2;
            termLoopToRegion = results.size() - 1;
          }
        }

        if (big) {
          reg.m_startTick = SBig(termStartTick);
          reg.m_progNum = 0xff;
          reg.m_unk1 = 0xff;
          reg.m_unk2 = 0;
          reg.m_regionIndex = SBig(termRegionIdx);
          reg.m_loopToRegion = SBig(termLoopToRegion);
        } else {
          reg.m_startTick = termStartTick;
          reg.m_progNum = 0xff;
          reg.m_unk1 = 0xff;
          reg.m_unk2 = 0;
          reg.m_regionIndex = termRegionIdx;
          reg.m_loopToRegion = termLoopToRegion;
        }
      }
    }
  }

  if (version == 1) {
    SongState::Header head;
    head.m_initialTempo = initTempo;
    head.m_loopStartTicks[0] = 0;
    if (loopChanCount == 1) {
      head.m_loopStartTicks[0] = loopStart[0] == INT_MAX ? 0 : loopStart[0];
    } else if (loopChanCount > 1) {
      for (int i = 0; i < 16; ++i)
        head.m_loopStartTicks[i] = loopStart[i] == INT_MAX ? 0 : loopStart[i];
      head.m_initialTempo |= 0x80000000;
    }
    size_t headSz = (head.m_initialTempo & 0x80000000) ? 0x58 : 0x18;
    head.m_trackIdxOff = headSz;
    head.m_regionIdxOff = headSz + 4 * 64 + regionBuf.size() * 12;
    head.m_chanMapOff = head.m_regionIdxOff + 4 * regionDataIdxArr.size() + curRegionOff;
    head.m_tempoTableOff = tempoBuf.size() ? head.m_chanMapOff + 64 : 0;
    head.m_chanMapOff2 = head.m_chanMapOff;

    uint32_t regIdxOff = head.m_regionIdxOff;
    if (big)
      head.swapToBig();
    *reinterpret_cast<SongState::Header*>(&*ret.insert(ret.cend(), headSz, 0)) = head;

    for (int i = 0; i < 64; ++i) {
      if (i >= trackRegionIdxArr.size()) {
        ret.insert(ret.cend(), 4, 0);
        continue;
      }

      uint32_t idx = trackRegionIdxArr[i];
      *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) =
          big ? SBig(uint32_t(headSz + 4 * 64 + idx * 12)) : uint32_t(headSz + 4 * 64 + idx * 12);
    }

    for (SongState::TrackRegion& reg : regionBuf)
      *reinterpret_cast<SongState::TrackRegion*>(&*ret.insert(ret.cend(), 12, 0)) = reg;

    uint32_t regBase = regIdxOff + 4 * regionDataIdxArr.size();
    for (uint32_t regOff : regionDataIdxArr)
      *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) =
          big ? SBig(uint32_t(regBase + regOff)) : uint32_t(regBase + regOff);

    uint32_t curOffset = regBase;
    for (Region& reg : regions) {
      *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) = big ? SBig(uint32_t(8)) : 8;

      if (reg.pitchBuf.size())
        *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) =
            big ? SBig(uint32_t(curOffset + 12 + reg.eventBuf.size())) : uint32_t(curOffset + 12 + reg.eventBuf.size());
      else
        ret.insert(ret.cend(), 4, 0);

      if (reg.modBuf.size())
        *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) =
            big ? SBig(uint32_t(curOffset + 12 + reg.eventBuf.size() + reg.pitchBuf.size()))
                : uint32_t(curOffset + 12 + reg.eventBuf.size() + reg.pitchBuf.size());
      else
        ret.insert(ret.cend(), 4, 0);

      if (reg.eventBuf.size())
        memmove(&*ret.insert(ret.cend(), reg.eventBuf.size(), 0), reg.eventBuf.data(), reg.eventBuf.size());

      if (reg.pitchBuf.size())
        memmove(&*ret.insert(ret.cend(), reg.pitchBuf.size(), 0), reg.pitchBuf.data(), reg.pitchBuf.size());

      if (reg.modBuf.size())
        memmove(&*ret.insert(ret.cend(), reg.modBuf.size(), 0), reg.modBuf.data(), reg.modBuf.size());

      ret.insert(ret.cend(), reg.padding, 0);

      curOffset += 12 + reg.eventBuf.size() + reg.pitchBuf.size() + reg.modBuf.size() + reg.padding;
    }

    memmove(&*ret.insert(ret.cend(), 64, 0), chanMap.data(), 64);

    if (tempoBuf.size())
      memmove(&*ret.insert(ret.cend(), tempoBuf.size() * 8, 0), tempoBuf.data(), tempoBuf.size() * 8);

    *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) = uint32_t(0xffffffff);
  } else {
    SongState::Header head;
    head.m_initialTempo = initTempo;
    head.m_loopStartTicks[0] = 0;
    if (loopChanCount == 1) {
      head.m_loopStartTicks[0] = loopStart[0] == INT_MAX ? 0 : loopStart[0];
    } else if (loopChanCount > 1) {
      for (int i = 0; i < 16; ++i)
        head.m_loopStartTicks[i] = loopStart[i] == INT_MAX ? 0 : loopStart[i];
      head.m_initialTempo |= 0x80000000;
    }
    size_t headSz = (head.m_initialTempo & 0x80000000) ? 0x58 : 0x18;
    head.m_trackIdxOff = headSz + regionBuf.size() * 12;
    head.m_regionIdxOff = head.m_trackIdxOff + 4 * 64 + 64 + curRegionOff;
    head.m_chanMapOff = head.m_trackIdxOff + 4 * 64;
    head.m_tempoTableOff = tempoBuf.size() ? head.m_regionIdxOff + 4 * regionDataIdxArr.size() : 0;
    head.m_chanMapOff2 = head.m_chanMapOff;

    uint32_t chanMapOff = head.m_chanMapOff;
    if (big)
      head.swapToBig();
    *reinterpret_cast<SongState::Header*>(&*ret.insert(ret.cend(), headSz, 0)) = head;

    for (SongState::TrackRegion& reg : regionBuf)
      *reinterpret_cast<SongState::TrackRegion*>(&*ret.insert(ret.cend(), 12, 0)) = reg;

    for (int i = 0; i < 64; ++i) {
      if (i >= trackRegionIdxArr.size()) {
        ret.insert(ret.cend(), 4, 0);
        continue;
      }

      uint32_t idx = trackRegionIdxArr[i];
      *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) =
          big ? SBig(uint32_t(headSz + 4 * 64 + idx * 12)) : uint32_t(headSz + 4 * 64 + idx * 12);
    }

    memmove(&*ret.insert(ret.cend(), 64, 0), chanMap.data(), 64);

    uint32_t regBase = chanMapOff + 64;
    uint32_t curOffset = regBase;
    for (Region& reg : regions) {
      *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) = big ? SBig(uint32_t(8)) : 8;

      if (reg.pitchBuf.size())
        *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) =
            big ? SBig(uint32_t(curOffset + 12 + reg.eventBuf.size())) : uint32_t(curOffset + 12 + reg.eventBuf.size());
      else
        ret.insert(ret.cend(), 4, 0);

      if (reg.modBuf.size())
        *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) =
            big ? SBig(uint32_t(curOffset + 12 + reg.eventBuf.size() + reg.pitchBuf.size()))
                : uint32_t(curOffset + 12 + reg.eventBuf.size() + reg.pitchBuf.size());
      else
        ret.insert(ret.cend(), 4, 0);

      if (reg.eventBuf.size())
        memmove(&*ret.insert(ret.cend(), reg.eventBuf.size(), 0), reg.eventBuf.data(), reg.eventBuf.size());

      if (reg.pitchBuf.size())
        memmove(&*ret.insert(ret.cend(), reg.pitchBuf.size(), 0), reg.pitchBuf.data(), reg.pitchBuf.size());

      if (reg.modBuf.size())
        memmove(&*ret.insert(ret.cend(), reg.modBuf.size(), 0), reg.modBuf.data(), reg.modBuf.size());

      ret.insert(ret.cend(), reg.padding, 0);

      curOffset += 12 + reg.eventBuf.size() + reg.pitchBuf.size() + reg.modBuf.size();
    }

    for (uint32_t regOff : regionDataIdxArr)
      *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) =
          big ? SBig(uint32_t(regBase + regOff)) : uint32_t(regBase + regOff);

    if (tempoBuf.size())
      memmove(&*ret.insert(ret.cend(), tempoBuf.size() * 8, 0), tempoBuf.data(), tempoBuf.size() * 8);

    *reinterpret_cast<uint32_t*>(&*ret.insert(ret.cend(), 4, 0)) = uint32_t(0xffffffff);
  }

  return ret;
}
} // namespace amuse

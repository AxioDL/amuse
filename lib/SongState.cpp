#include "amuse/SongState.hpp"

#include <cmath>

#include "amuse/Common.hpp"
#include "amuse/Sequencer.hpp"

namespace amuse {

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

void SongState::Header::swapFromBig() {
  m_trackIdxOff = SBig(m_trackIdxOff);
  m_regionIdxOff = SBig(m_regionIdxOff);
  m_chanMapOff = SBig(m_chanMapOff);
  m_tempoTableOff = SBig(m_tempoTableOff);
  m_initialTempo = SBig(m_initialTempo);
  if (m_initialTempo & 0x80000000) {
    for (int i = 0; i < 16; ++i)
      m_loopStartTicks[i] = SBig(m_loopStartTicks[i]);
    m_chanMapOff2 = SBig(m_chanMapOff2);
  } else {
    m_loopStartTicks[0] = SBig(m_loopStartTicks[0]);
  }
}

void SongState::Header::swapToBig() {
  m_trackIdxOff = SBig(m_trackIdxOff);
  m_regionIdxOff = SBig(m_regionIdxOff);
  m_chanMapOff = SBig(m_chanMapOff);
  m_tempoTableOff = SBig(m_tempoTableOff);
  m_initialTempo = SBig(m_initialTempo);
  if (m_initialTempo & 0x00000080) {
    for (int i = 0; i < 16; ++i)
      m_loopStartTicks[i] = SBig(m_loopStartTicks[i]);
    m_chanMapOff2 = SBig(m_chanMapOff2);
  } else {
    m_loopStartTicks[0] = SBig(m_loopStartTicks[0]);
  }
}

SongState::Header& SongState::Header::operator=(const Header& other) {
  m_trackIdxOff = other.m_trackIdxOff;
  m_regionIdxOff = other.m_regionIdxOff;
  m_chanMapOff = other.m_chanMapOff;
  m_tempoTableOff = other.m_tempoTableOff;
  m_initialTempo = other.m_initialTempo;
  if (SBig(m_initialTempo) & 0x80000000) {
    for (int i = 0; i < 16; ++i)
      m_loopStartTicks[i] = other.m_loopStartTicks[i];
    m_chanMapOff2 = other.m_chanMapOff2;
  } else {
    m_loopStartTicks[0] = other.m_loopStartTicks[0];
  }
  return *this;
}

bool SongState::TrackRegion::indexDone(bool bigEndian, bool loop) const {
  int16_t idx = (bigEndian ? SBig(m_regionIndex) : m_regionIndex);
  return loop ? (idx == -1) : (idx < 0);
}

bool SongState::TrackRegion::indexValid(bool bigEndian) const {
  return (bigEndian ? SBig(m_regionIndex) : m_regionIndex) >= 0;
}

int SongState::TrackRegion::indexLoop(bool bigEndian) const {
  if ((bigEndian ? SBig(m_regionIndex) : m_regionIndex) != -2)
    return -1;
  return (bigEndian ? SBig(m_loopToRegion) : m_loopToRegion);
}

void SongState::TempoChange::swapBig() {
  m_tick = SBig(m_tick);
  m_tempo = SBig(m_tempo);
}

void SongState::Track::Header::swapBig() {
  m_type = SBig(m_type);
  m_pitchOff = SBig(m_pitchOff);
  m_modOff = SBig(m_modOff);
}

SongState::Track::Track(SongState& parent, uint8_t midiChan, uint32_t loopStart, const TrackRegion* regions,
                        uint32_t tempo)
: m_parent(&parent)
, m_midiChan(midiChan)
, m_initRegion(regions)
, m_curRegion(nullptr)
, m_nextRegion(regions)
, m_loopStartTick(loopStart)
, m_tempo(tempo) {
  resetTempo();
}

void SongState::Track::setRegion(const TrackRegion* region) {
  m_curRegion = region;
  uint32_t regionIdx = (m_parent->m_bigEndian ? SBig(m_curRegion->m_regionIndex) : m_curRegion->m_regionIndex);
  m_nextRegion = &m_curRegion[1];

  m_data = m_parent->m_songData +
           (m_parent->m_bigEndian ? SBig(m_parent->m_regionIdx[regionIdx]) : m_parent->m_regionIdx[regionIdx]);

  Header header = *reinterpret_cast<const Header*>(m_data);
  if (m_parent->m_bigEndian)
    header.swapBig();
  m_data += 12;

  m_pitchWheelData = nullptr;
  m_nextPitchTick = 0x7fffffff;
  m_nextPitchDelta = 0;
  m_pitchVal = 0;
  if (header.m_pitchOff) {
    m_pitchWheelData = m_parent->m_songData + header.m_pitchOff;
    if (m_pitchWheelData[0] != 0x80 || m_pitchWheelData[1] != 0x00) {
      auto delta = DecodeDelta(m_pitchWheelData);
      m_nextPitchTick = m_curTick + delta.first;
      m_nextPitchDelta = delta.second;
    }
  }

  m_modWheelData = nullptr;
  m_nextModTick = 0x7fffffff;
  m_nextModDelta = 0;
  m_modVal = 0;
  if (header.m_modOff) {
    m_modWheelData = m_parent->m_songData + header.m_modOff;
    if (m_modWheelData[0] != 0x80 || m_modWheelData[1] != 0x00) {
      auto delta = DecodeDelta(m_modWheelData);
      m_nextModTick = m_curTick + delta.first;
      m_nextModDelta = delta.second;
    }
  }

  m_eventWaitCountdown = 0;
  if (m_parent->m_sngVersion == 1) {
    m_eventWaitCountdown = int32_t(DecodeTime(m_data));
  } else {
    int32_t absTick = (m_parent->m_bigEndian ? SBig(*reinterpret_cast<const int32_t*>(m_data))
                                             : *reinterpret_cast<const int32_t*>(m_data));
    m_eventWaitCountdown = absTick;
    m_lastN64EventTick = absTick;
    m_data += 4;
  }
}

void SongState::Track::advanceRegion() { setRegion(m_nextRegion); }

int SongState::DetectVersion(const unsigned char* ptr, bool& isBig) {
  isBig = ptr[0] == 0;
  Header header = *reinterpret_cast<const Header*>(ptr);
  if (isBig)
    header.swapFromBig();
  const uint32_t* trackIdx = reinterpret_cast<const uint32_t*>(ptr + header.m_trackIdxOff);
  const uint32_t* regionIdxTable = reinterpret_cast<const uint32_t*>(ptr + header.m_regionIdxOff);

  /* First determine maximum index of MIDI regions across all tracks */
  uint32_t maxRegionIdx = 0;
  for (int i = 0; i < 64; ++i) {
    if (trackIdx[i]) {
      const TrackRegion* region = nullptr;
      const TrackRegion* nextRegion =
          reinterpret_cast<const TrackRegion*>(ptr + (isBig ? SBig(trackIdx[i]) : trackIdx[i]));

      /* Iterate all regions */
      while (nextRegion->indexValid(isBig)) {
        region = nextRegion;
        uint32_t regionIdx = (isBig ? SBig(region->m_regionIndex) : region->m_regionIndex);
        maxRegionIdx = std::max(maxRegionIdx, regionIdx);
        nextRegion = &region[1];
      }
    }
  }

  /* Perform 2 trials, first assuming revised format (more likely) */
  int v = 1;
  for (; v >= 0; --v) {
    bool bad = false;

    /* Validate all tracks */
    for (int i = 0; i < 64; ++i) {
      if (trackIdx[i]) {
        const TrackRegion* region = nullptr;
        const TrackRegion* nextRegion =
            reinterpret_cast<const TrackRegion*>(ptr + (isBig ? SBig(trackIdx[i]) : trackIdx[i]));

        /* Iterate all regions */
        while (nextRegion->indexValid(isBig)) {
          region = nextRegion;
          uint32_t regionIdx = (isBig ? SBig(region->m_regionIndex) : region->m_regionIndex);
          nextRegion = &region[1];

          const unsigned char* data = ptr + (isBig ? SBig(regionIdxTable[regionIdx]) : regionIdxTable[regionIdx]);

          /* Can't reliably validate final region */
          if (regionIdx == maxRegionIdx)
            continue;

          /* Expected end pointer (next region) */
          const unsigned char* expectedEnd =
              ptr + (isBig ? SBig(regionIdxTable[regionIdx + 1]) : regionIdxTable[regionIdx + 1]);

          auto header2 = *reinterpret_cast<const Track::Header*>(data);
          if (isBig)
            header2.swapBig();
          data += 12;

          /* continuous pitch data */
          if (header2.m_pitchOff) {
            const unsigned char* dptr = ptr + header2.m_pitchOff;
            while (dptr[0] != 0x80 || dptr[1] != 0x00)
              DecodeDelta(dptr);
            dptr += 2;
            if (dptr >= (expectedEnd - 4) && (dptr <= expectedEnd))
              continue;
          }

          /* continuous modulation data */
          if (header2.m_modOff) {
            const unsigned char* dptr = ptr + header2.m_modOff;
            while (dptr[0] != 0x80 || dptr[1] != 0x00)
              DecodeDelta(dptr);
            dptr += 2;
            if (dptr >= (expectedEnd - 4) && (dptr <= expectedEnd))
              continue;
          }

          /* Loop through as many commands as we can for this time period */
          if (v == 1) {
            /* Revised */
            while (true) {
              /* Delta time */
              DecodeTime(data);

              /* Load next command */
              if (*reinterpret_cast<const uint16_t*>(data) == 0xffff) {
                /* End of channel */
                data += 2;
                break;
              } else if (data[0] & 0x80 && data[1] & 0x80) {
                /* Control change */
                data += 2;
              } else if (data[0] & 0x80) {
                /* Program change */
                data += 2;
              } else {
                /* Note */
                data += 4;
              }
            }
          } else {
            /* Legacy */
            while (true) {
              /* Delta-time */
              data += 4;

              /* Load next command */
              if (*reinterpret_cast<const uint16_t*>(&data[2]) == 0xffff) {
                /* End of channel */
                data += 4;
                break;
              } else {
                if ((data[2] & 0x80) != 0x80) {
                  /* Note */
                } else if (data[2] & 0x80 && data[3] & 0x80) {
                  /* Control change */
                } else if (data[2] & 0x80) {
                  /* Program change */
                }
                data += 4;
              }
            }
          }

          if (data < (expectedEnd - 4) || (data > expectedEnd)) {
            bad = true;
            break;
          }
        }
        if (bad)
          break;
      }
    }
    if (bad)
      continue;
    break;
  }

  return v;
}

bool SongState::initialize(const unsigned char* ptr, bool loop) {
  m_loop = loop;
  m_sngVersion = DetectVersion(ptr, m_bigEndian);
  if (m_sngVersion < 0)
    return false;

  m_songData = ptr;
  m_header = *reinterpret_cast<const Header*>(ptr);
  if (m_bigEndian)
    m_header.swapFromBig();
  const uint32_t* trackIdx = reinterpret_cast<const uint32_t*>(ptr + m_header.m_trackIdxOff);
  m_regionIdx = reinterpret_cast<const uint32_t*>(ptr + m_header.m_regionIdxOff);
  const uint8_t* chanMap = reinterpret_cast<const uint8_t*>(ptr + m_header.m_chanMapOff);

  /* Initialize all tracks */
  for (int i = 0; i < 64; ++i) {
    if (trackIdx[i]) {
      const TrackRegion* region =
          reinterpret_cast<const TrackRegion*>(ptr + (m_bigEndian ? SBig(trackIdx[i]) : trackIdx[i]));
      uint8_t chan = chanMap[i];
      uint32_t loopStart =
          (m_header.m_initialTempo & 0x80000000) ? m_header.m_loopStartTicks[chan] : m_header.m_loopStartTicks[0];
      m_tracks[i] = Track(*this, chan, loopStart, region, m_header.m_initialTempo & 0x7fffffff);
    } else
      m_tracks[i] = Track();
  }

  m_songState = SongPlayState::Playing;

  return true;
}

void SongState::Track::resetTempo() {
  if (m_parent->m_header.m_tempoTableOff)
    m_tempoPtr = reinterpret_cast<const TempoChange*>(m_parent->m_songData + m_parent->m_header.m_tempoTableOff);
  else
    m_tempoPtr = nullptr;
}

bool SongState::Track::advance(Sequencer& seq, double dt) {
  m_remDt += dt;

  /* Compute ticks to compute based on current tempo */
  double ticksPerSecond = m_tempo * 384 / 60;
  uint32_t ticks = uint32_t(std::floor(m_remDt * ticksPerSecond));

  /* See if there's an upcoming tempo change in this interval */
  while (m_tempoPtr && m_tempoPtr->m_tick != 0xffffffff) {
    TempoChange change = *m_tempoPtr;
    if (m_parent->m_bigEndian)
      change.swapBig();

    if (m_curTick + ticks > change.m_tick)
      ticks = change.m_tick - m_curTick;

    if (ticks <= 0) {
      /* Turn over tempo */
      m_tempo = change.m_tempo & 0x7fffffff;
      ticksPerSecond = m_tempo * 384 / 60;
      ticks = uint32_t(std::floor(m_remDt * ticksPerSecond));
      seq.setTempo(m_midiChan, m_tempo * 384 / 60.0);
      ++m_tempoPtr;
      continue;
    }
    break;
  }

  m_remDt -= ticks / ticksPerSecond;
  uint32_t endTick = m_curTick + ticks;

  /* Advance region if needed */
  while (m_nextRegion->indexValid(m_parent->m_bigEndian)) {
    uint32_t nextRegTick = (m_parent->m_bigEndian ? SBig(m_nextRegion->m_startTick) : m_nextRegion->m_startTick);
    if (uint32_t(endTick) > nextRegTick)
      advanceRegion();
    else
      break;
  }

  /* Stop finished notes */
  for (int i = 0; i < 128; ++i) {
    if (m_remNoteLengths[i] > 0) {
      m_remNoteLengths[i] -= ticks;
      if (m_remNoteLengths[i] <= 0)
        seq.keyOff(m_midiChan, i, 0);
    }
  }

  if (m_data) {
    /* Update continuous pitch data */
    if (m_pitchWheelData) {
      int32_t pitchTick = m_curTick;
      int32_t remPitchTicks = ticks;
      while (pitchTick < int32_t(endTick)) {
        /* See if there's an upcoming pitch change in this interval */
        int32_t nextTick = m_nextPitchTick;
        if (pitchTick + remPitchTicks > nextTick) {
          /* Update pitch */
          m_pitchVal += m_nextPitchDelta;
          seq.setPitchWheel(m_midiChan, std::clamp(-1.f, m_pitchVal / 8191.f, 1.f));
          if (m_pitchWheelData[0] != 0x80 || m_pitchWheelData[1] != 0x00) {
            auto delta = DecodeDelta(m_pitchWheelData);
            m_nextPitchTick += delta.first;
            m_nextPitchDelta = delta.second;
          } else {
            m_nextPitchTick = 0x7fffffff;
          }
        }
        remPitchTicks -= (nextTick - pitchTick);
        pitchTick = nextTick;
      }
    }

    /* Update continuous modulation data */
    if (m_modWheelData) {
      int32_t modTick = m_curTick;
      int32_t remModTicks = ticks;
      while (modTick < int32_t(endTick)) {
        /* See if there's an upcoming modulation change in this interval */
        int32_t nextTick = m_nextModTick;
        if (modTick + remModTicks > nextTick) {
          /* Update modulation */
          m_modVal += m_nextModDelta;
          seq.setCtrlValue(m_midiChan, 1, int8_t(std::clamp(0, m_modVal / 127, 127)));
          if (m_modWheelData[0] != 0x80 || m_modWheelData[1] != 0x00) {
            auto delta = DecodeDelta(m_modWheelData);
            m_nextModTick += delta.first;
            m_nextModDelta = delta.second;
          } else {
            m_nextModTick = 0x7fffffff;
          }
        }
        remModTicks -= (nextTick - modTick);
        modTick = nextTick;
      }
    }

    /* Loop through as many commands as we can for this time period */
    if (m_parent->m_sngVersion == 1) {
      /* Revision */
      while (true) {
        /* Advance wait timer if active, returning if waiting */
        if (m_eventWaitCountdown) {
          m_eventWaitCountdown -= ticks;
          ticks = 0;
          if (m_eventWaitCountdown > 0)
            break;
        }

        /* Load next command */
        if (*reinterpret_cast<const uint16_t*>(m_data) == 0xffff) {
          /* End of channel */
          m_data = nullptr;
          break;
        } else if (m_data[0] & 0x80 && m_data[1] & 0x80) {
          /* Control change */
          uint8_t val = m_data[0] & 0x7f;
          uint8_t ctrl = m_data[1] & 0x7f;
          seq.setCtrlValue(m_midiChan, ctrl, val);
          m_data += 2;
        } else if (m_data[0] & 0x80) {
          /* Program change */
          uint8_t prog = m_data[0] & 0x7f;
          seq.setChanProgram(m_midiChan, prog);
          m_data += 2;
        } else {
          /* Note */
          uint8_t note = m_data[0] & 0x7f;
          uint8_t vel = m_data[1] & 0x7f;
          uint16_t length = (m_parent->m_bigEndian ? SBig(*reinterpret_cast<const uint16_t*>(m_data + 2))
                                                   : *reinterpret_cast<const uint16_t*>(m_data + 2));
          seq.keyOn(m_midiChan, note, vel);
          if (length == 0)
            seq.keyOff(m_midiChan, note, 0);
          m_remNoteLengths[note] = length;
          m_data += 4;
        }

        /* Set next delta-time */
        m_eventWaitCountdown += int32_t(DecodeTime(m_data));
      }
    } else {
      /* Legacy */
      while (true) {
        /* Advance wait timer if active, returning if waiting */
        if (m_eventWaitCountdown) {
          m_eventWaitCountdown -= ticks;
          ticks = 0;
          if (m_eventWaitCountdown > 0)
            break;
        }

        /* Load next command */
        if (*reinterpret_cast<const uint16_t*>(&m_data[2]) == 0xffff) {
          /* End of channel */
          m_data = nullptr;
          break;
        } else {
          if ((m_data[2] & 0x80) != 0x80) {
            /* Note */
            uint16_t length = (m_parent->m_bigEndian ? SBig(*reinterpret_cast<const uint16_t*>(m_data))
                                                     : *reinterpret_cast<const uint16_t*>(m_data));
            uint8_t note = m_data[2] & 0x7f;
            uint8_t vel = m_data[3] & 0x7f;
            seq.keyOn(m_midiChan, note, vel);
            if (length == 0)
              seq.keyOff(m_midiChan, note, 0);
            m_remNoteLengths[note] = length;
          } else if (m_data[2] & 0x80 && m_data[3] & 0x80) {
            /* Control change */
            uint8_t val = m_data[2] & 0x7f;
            uint8_t ctrl = m_data[3] & 0x7f;
            seq.setCtrlValue(m_midiChan, ctrl, val);
          } else if (m_data[2] & 0x80) {
            /* Program change */
            uint8_t prog = m_data[2] & 0x7f;
            seq.setChanProgram(m_midiChan, prog);
          }
          m_data += 4;
        }

        /* Set next delta-time */
        int32_t absTick = (m_parent->m_bigEndian ? SBig(*reinterpret_cast<const int32_t*>(m_data))
                                                 : *reinterpret_cast<const int32_t*>(m_data));
        m_eventWaitCountdown += absTick - m_lastN64EventTick;
        m_lastN64EventTick = absTick;
        m_data += 4;
      }
    }
  }

  m_curTick = endTick;

  /* Handle loop end */
  if (m_parent->m_loop) {
    int loopTo;
    if ((loopTo = m_nextRegion->indexLoop(m_parent->m_bigEndian)) != -1) {
      uint32_t loopEndTick = (m_parent->m_bigEndian ? SBig(m_nextRegion->m_startTick) : m_nextRegion->m_startTick);
      if (uint32_t(endTick) > loopEndTick) {
        m_nextRegion = &m_initRegion[loopTo];
        m_curRegion = nullptr;
        m_data = nullptr;
        m_curTick = m_loopStartTick;
        resetTempo();
        return false;
      }
    }
  }

  if (!m_data)
    return m_nextRegion->indexDone(m_parent->m_bigEndian, m_parent->m_loop);

  return false;
}

bool SongState::advance(Sequencer& seq, double dt) {
  /* Stopped */
  if (m_songState == SongPlayState::Stopped)
    return true;

  /* Advance all tracks */
  bool done = true;
  for (Track& trk : m_tracks)
    if (trk)
      done &= trk.advance(seq, dt);

  if (done)
    m_songState = SongPlayState::Stopped;
  return done;
}
} // namespace amuse

#include "MIDIReader.hpp"
#include "MainWindow.hpp"

MIDIReader::MIDIReader(amuse::Engine& engine, bool useLock) : amuse::BooBackendMIDIReader(engine, useLock) {}

void MIDIReader::noteOff(uint8_t chan, uint8_t key, uint8_t velocity) {
  if (g_MainWindow->m_interactiveSeq) {
    g_MainWindow->m_interactiveSeq->keyOff(chan, key, velocity);
    return;
  }

  auto keySearch = m_chanVoxs.find(key);
  if (keySearch == m_chanVoxs.cend())
    return;

  if ((m_lastVoice && m_lastVoice->isDestroyed()) || keySearch->second == m_lastVoice)
    m_lastVoice.reset();
  keySearch->second->keyOff();
  m_keyoffVoxs.emplace(std::move(keySearch->second));
  m_chanVoxs.erase(keySearch);
}

void MIDIReader::noteOn(uint8_t chan, uint8_t key, uint8_t velocity) {
  if (g_MainWindow->m_interactiveSeq) {
    g_MainWindow->m_interactiveSeq->keyOn(chan, key, velocity);
    return;
  }

  if (m_lastVoice && m_lastVoice->isDestroyed())
    m_lastVoice.reset();

  /* If portamento is enabled for voice, pre-empt spawning new voices */
  if (amuse::ObjToken<amuse::Voice> lastVoice = m_lastVoice) {
    uint8_t lastNote = lastVoice->getLastNote();
    if (lastVoice->doPortamento(key)) {
      m_chanVoxs.erase(lastNote);
      m_chanVoxs[key] = lastVoice;
      return;
    }
  }

  /* Ensure keyoff sent first */
  auto keySearch = m_chanVoxs.find(key);
  if (keySearch != m_chanVoxs.cend()) {
    if (keySearch->second == m_lastVoice)
      m_lastVoice.reset();
    keySearch->second->keyOff();
    keySearch->second->setPedal(false);
    m_keyoffVoxs.emplace(std::move(keySearch->second));
    m_chanVoxs.erase(keySearch);
  }

  amuse::ObjToken<amuse::Voice> newVox = g_MainWindow->startEditorVoice(key, velocity);
  if (newVox) {
    m_chanVoxs[key] = newVox;
    m_lastVoice = newVox;
  }
}

void MIDIReader::notePressure(uint8_t /*chan*/, uint8_t /*key*/, uint8_t /*pressure*/) {}

void MIDIReader::controlChange(uint8_t chan, uint8_t control, uint8_t value) {
  if (g_MainWindow->m_interactiveSeq) {
    g_MainWindow->m_interactiveSeq->setCtrlValue(chan, control, value);
    return;
  }

  if (control == 1) {
    g_MainWindow->m_ui.modulationSlider->setValue(int(value));
  } else if (control == 64) {
    g_MainWindow->setSustain(value >= 0x40);
  } else {
    for (auto& v : m_engine.getActiveVoices())
      v->setCtrlValue(control, value);
  }
  g_MainWindow->m_ctrlVals[control] = value;
}

void MIDIReader::programChange(uint8_t chan, uint8_t program) {
  if (g_MainWindow->m_interactiveSeq)
    g_MainWindow->m_interactiveSeq->setChanProgram(chan, program);
}

void MIDIReader::channelPressure(uint8_t /*chan*/, uint8_t /*pressure*/) {}

void MIDIReader::pitchBend(uint8_t chan, int16_t pitch) {
  float pWheel = (pitch - 0x2000) / float(0x2000);
  if (g_MainWindow->m_interactiveSeq)
    g_MainWindow->m_interactiveSeq->setPitchWheel(chan, pWheel);
  else
    g_MainWindow->m_ui.pitchSlider->setValue(int(pWheel * 2048.f));
}

void MIDIReader::allSoundOff(uint8_t chan) {
  if (g_MainWindow->m_interactiveSeq) {
    g_MainWindow->m_interactiveSeq->kill();
    return;
  }

  for (auto& v : m_engine.getActiveVoices())
    v->kill();
}

void MIDIReader::resetAllControllers(uint8_t /*chan*/) {}

void MIDIReader::localControl(uint8_t /*chan*/, bool /*on*/) {}

void MIDIReader::allNotesOff(uint8_t chan) {
  if (g_MainWindow->m_interactiveSeq) {
    g_MainWindow->m_interactiveSeq->kill();
    return;
  }

  for (auto& v : m_engine.getActiveVoices())
    v->kill();
}

void MIDIReader::omniMode(uint8_t /*chan*/, bool /*on*/) {}

void MIDIReader::polyMode(uint8_t /*chan*/, bool /*on*/) {}

void MIDIReader::sysex(const void* /*data*/, size_t /*len*/) {}

void MIDIReader::timeCodeQuarterFrame(uint8_t /*message*/, uint8_t /*value*/) {}

void MIDIReader::songPositionPointer(uint16_t /*pointer*/) {}

void MIDIReader::songSelect(uint8_t /*song*/) {}

void MIDIReader::tuneRequest() {}

void MIDIReader::startSeq() {}

void MIDIReader::continueSeq() {}

void MIDIReader::stopSeq() {}

void MIDIReader::reset() {}

VoiceAllocator::VoiceAllocator(boo::IAudioVoiceEngine& booEngine) : amuse::BooBackendVoiceAllocator(booEngine) {}

std::unique_ptr<amuse::IMIDIReader> VoiceAllocator::allocateMIDIReader(amuse::Engine& engine) {
  return std::make_unique<MIDIReader>(engine, m_booEngine.useMIDILock());
}

#include "MIDIReader.hpp"
#include "MainWindow.hpp"

MIDIReader::MIDIReader(amuse::Engine& engine, const char* name, bool useLock)
: amuse::BooBackendMIDIReader(engine, name, useLock) {}

void MIDIReader::noteOff(uint8_t chan, uint8_t key, uint8_t velocity)
{
    auto keySearch = m_chanVoxs.find(key);
    if (keySearch == m_chanVoxs.cend())
        return;

    if (keySearch->second == m_lastVoice.lock())
        m_lastVoice.reset();
    keySearch->second->keyOff();
    m_keyoffVoxs.emplace(std::move(keySearch->second));
    m_chanVoxs.erase(keySearch);
}

void MIDIReader::noteOn(uint8_t chan, uint8_t key, uint8_t velocity)
{
    /* If portamento is enabled for voice, pre-empt spawning new voices */
    if (std::shared_ptr<amuse::Voice> lastVoice = m_lastVoice.lock())
    {
        uint8_t lastNote = lastVoice->getLastNote();
        if (lastVoice->doPortamento(key))
        {
            m_chanVoxs.erase(lastNote);
            m_chanVoxs[key] = lastVoice;
            return;
        }
    }

    /* Ensure keyoff sent first */
    auto keySearch = m_chanVoxs.find(key);
    if (keySearch != m_chanVoxs.cend())
    {
        if (keySearch->second == m_lastVoice.lock())
            m_lastVoice.reset();
        keySearch->second->keyOff();
        keySearch->second->setPedal(false);
        m_keyoffVoxs.emplace(std::move(keySearch->second));
        m_chanVoxs.erase(keySearch);
    }

    ProjectModel::INode* node = g_MainWindow->getEditorNode();
    if (node && node->type() == ProjectModel::INode::Type::SoundMacro)
    {
        ProjectModel::SoundMacroNode* cNode = static_cast<ProjectModel::SoundMacroNode*>(node);
        amuse::AudioGroupDatabase* group = g_MainWindow->projectModel()->getGroupNode(node)->getAudioGroup();
        std::shared_ptr<amuse::Voice>& vox = m_chanVoxs[key];
        vox = m_engine.macroStart(group, cNode->id(), key, velocity, g_MainWindow->m_modulation);
        vox->setPedal(g_MainWindow->m_sustain);
        vox->setPitchWheel(g_MainWindow->m_pitch);
    }
}

void MIDIReader::notePressure(uint8_t /*chan*/, uint8_t /*key*/, uint8_t /*pressure*/) {}

void MIDIReader::controlChange(uint8_t chan, uint8_t control, uint8_t value)
{
    if (control == 1)
    {
        g_MainWindow->m_ui.modulationSlider->setValue(int(value));
    }
    else if (control == 64)
    {
        g_MainWindow->setSustain(value >= 0x40);
    }
    else
    {
        for (auto& v : m_engine.getActiveVoices())
            v->setCtrlValue(control, value);
    }
}

void MIDIReader::programChange(uint8_t chan, uint8_t program) {}

void MIDIReader::channelPressure(uint8_t /*chan*/, uint8_t /*pressure*/) {}

void MIDIReader::pitchBend(uint8_t chan, int16_t pitch)
{
    g_MainWindow->m_ui.pitchSlider->setValue(int((pitch - 0x2000) / float(0x2000) * 2048.f));
}

void MIDIReader::allSoundOff(uint8_t chan)
{
    for (auto& v : m_engine.getActiveVoices())
        v->kill();
}

void MIDIReader::resetAllControllers(uint8_t /*chan*/) {}

void MIDIReader::localControl(uint8_t /*chan*/, bool /*on*/) {}

void MIDIReader::allNotesOff(uint8_t chan)
{
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

VoiceAllocator::VoiceAllocator(boo::IAudioVoiceEngine& booEngine)
: amuse::BooBackendVoiceAllocator(booEngine) {}

std::unique_ptr<amuse::IMIDIReader>
VoiceAllocator::allocateMIDIReader(amuse::Engine& engine, const char* name)
{
    std::unique_ptr<amuse::IMIDIReader> ret = std::make_unique<MIDIReader>(engine, name, m_booEngine.useMIDILock());
    if (!static_cast<MIDIReader&>(*ret).getMidiIn())
        return {};
    return ret;
}

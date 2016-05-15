#include "amuse/Sequencer.hpp"
#include "amuse/Submix.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Engine.hpp"

namespace amuse
{

void Sequencer::ChannelState::_bringOutYourDead()
{
    for (auto it = m_chanVoxs.begin() ; it != m_chanVoxs.end() ;)
    {
        Voice* vox = it->second.get();
        vox->_bringOutYourDead();
        if (vox->_isRecursivelyDead())
        {
            it = m_chanVoxs.erase(it);
            continue;
        }
        ++it;
    }
}

void Sequencer::_bringOutYourDead()
{
    for (auto& chan : m_chanStates)
        chan.second->_bringOutYourDead();

    if (!m_arrData && m_dieOnEnd && getVoiceCount() == 0)
        m_state = SequencerState::Dead;
}

void Sequencer::_destroy()
{
    Entity::_destroy();
    if (m_submix)
        m_submix->m_activeSequencers.erase(this);

    for (const auto& chan : m_chanStates)
        for (const auto& vox : chan.second->m_chanVoxs)
            vox.second->_destroy();
}

Sequencer::~Sequencer() {}

Sequencer::Sequencer(Engine& engine, const AudioGroup& group, int groupId,
                     const SongGroupIndex& songGroup, int setupId, Submix* smx)
: Entity(engine, group, groupId), m_songGroup(songGroup), m_submix(smx)
{
    auto it = m_songGroup.m_midiSetups.find(setupId);
    if (it != m_songGroup.m_midiSetups.cend())
        m_midiSetup = it->second->data();
    if (m_submix)
        m_submix->m_activeSequencers.insert(this);
}

Sequencer::ChannelState::~ChannelState()
{
    if (m_submix)
        m_parent.m_engine.removeSubmix(m_submix);
}

Sequencer::ChannelState::ChannelState(Sequencer& parent, uint8_t chanId)
: m_parent(parent), m_setup(m_parent.m_midiSetup[chanId])
{
    if (chanId == 10)
    {
        auto it = m_parent.m_songGroup.m_drumPages.find(m_setup.programNo);
        if (it != m_parent.m_songGroup.m_drumPages.cend())
            m_page = it->second;
    }
    else
    {
        auto it = m_parent.m_songGroup.m_normPages.find(m_setup.programNo);
        if (it != m_parent.m_songGroup.m_normPages.cend())
            m_page = it->second;
    }

    m_submix = m_parent.m_engine.addSubmix(m_parent.m_submix);
    if (m_setup.reverb)
        m_submix->makeReverbStd(0.5f, m_setup.reverb / 127.f, 5.f, 0.5f, 0.f);
    if (m_setup.chorus)
        m_submix->makeChorus(15, m_setup.chorus * 5 / 127, 5000);
}

size_t Sequencer::getVoiceCount() const
{
    size_t ret = 0;
    for (const auto& chan : m_chanStates)
        for (const auto& vox : chan.second->m_chanVoxs)
            ret += vox.second->getTotalVoices();
    return ret;
}

std::shared_ptr<Voice> Sequencer::ChannelState::keyOn(uint8_t note, uint8_t velocity)
{
    if (!m_page)
        return {};

    std::shared_ptr<Voice> ret = m_parent.m_engine._allocateVoice(m_parent.m_audioGroup,
                                                                  m_parent.m_groupId, 32000.0,
                                                                  true, false, m_submix);
    m_chanVoxs[note] = ret;
    ret->installCtrlValues(m_ctrlVals);
    if (!ret->loadSoundObject(SBig(m_page->objId), 0, 1000.f, note, velocity, m_ctrlVals[1]))
    {
        m_parent.m_engine._destroyVoice(ret.get());
        return {};
    }
    ret->setVolume(m_setup.volume / 127.f);
    ret->setPan(m_setup.panning / 64.f - 127.f);
    return ret;
}

std::shared_ptr<Voice> Sequencer::keyOn(uint8_t chan, uint8_t note, uint8_t velocity)
{
    auto chanSearch = m_chanStates.find(chan);
    if (chanSearch == m_chanStates.cend())
    {
        auto it = m_chanStates.emplace(std::make_pair(chan, std::make_unique<ChannelState>(*this, chan)));
        return it.first->second->keyOn(note, velocity);
    }

    return chanSearch->second->keyOn(note, velocity);
}

void Sequencer::ChannelState::keyOff(uint8_t note, uint8_t velocity)
{
    auto keySearch = m_chanVoxs.find(note);
    if (keySearch == m_chanVoxs.cend())
        return;

    keySearch->second->keyOff();
    m_chanVoxs.erase(keySearch);
}

void Sequencer::keyOff(uint8_t chan, uint8_t note, uint8_t velocity)
{
    auto chanSearch = m_chanStates.find(chan);
    if (chanSearch == m_chanStates.cend())
        return;

    chanSearch->second->keyOff(note, velocity);
}

void Sequencer::ChannelState::setCtrlValue(uint8_t ctrl, int8_t val)
{
    m_ctrlVals[ctrl] = val;
}

void Sequencer::setCtrlValue(uint8_t chan, uint8_t ctrl, int8_t val)
{
    auto chanSearch = m_chanStates.find(chan);
    if (chanSearch == m_chanStates.cend())
        return;

    chanSearch->second->setCtrlValue(ctrl, val);
}

void Sequencer::ChannelState::setPitchWheel(float pitchWheel)
{
    for (const auto& vox : m_chanVoxs)
        vox.second->setPitchWheel(pitchWheel);
}

void Sequencer::setPitchWheel(uint8_t chan, float pitchWheel)
{
    auto chanSearch = m_chanStates.find(chan);
    if (chanSearch == m_chanStates.cend())
        return;

    chanSearch->second->setPitchWheel(pitchWheel);
}

void Sequencer::setTempo(double ticksPerSec)
{
    m_ticksPerSec = ticksPerSec;
}

void Sequencer::ChannelState::allOff()
{
    for (const auto& vox : m_chanVoxs)
        vox.second->keyOff();
}

void Sequencer::allOff(bool now)
{
    if (now)
        for (auto& chan : m_chanStates)
            chan.second->m_chanVoxs.clear();
    else
        for (auto& chan : m_chanStates)
            chan.second->allOff();
}

void Sequencer::ChannelState::killKeygroup(uint8_t kg, bool now)
{
    for (auto it = m_chanVoxs.begin() ; it != m_chanVoxs.end() ;)
    {
        Voice* vox = it->second.get();
        if (vox->m_keygroup == kg)
        {
            if (now)
            {
                it = m_chanVoxs.erase(it);
                continue;
            }
            vox->keyOff();
        }
        ++it;
    }
}

void Sequencer::killKeygroup(uint8_t kg, bool now)
{
    for (auto& chan : m_chanStates)
        chan.second->killKeygroup(kg, now);
}

std::shared_ptr<Voice> Sequencer::ChannelState::findVoice(int vid)
{
    for (const auto& vox : m_chanVoxs)
        if (vox.second->vid() == vid)
            return vox.second;
    return {};
}

std::shared_ptr<Voice> Sequencer::findVoice(int vid)
{
    for (auto& chan : m_chanStates)
    {
        std::shared_ptr<Voice> ret = chan.second->findVoice(vid);
        if (ret)
            return ret;
    }
    return {};
}

void Sequencer::ChannelState::sendMacroMessage(ObjectId macroId, int32_t val)
{
    for (const auto& v : m_chanVoxs)
    {
        Voice* vox = v.second.get();
        if (vox->getObjectId() == macroId)
            vox->message(val);
    }
}

void Sequencer::sendMacroMessage(ObjectId macroId, int32_t val)
{
    for (auto& chan : m_chanStates)
        chan.second->sendMacroMessage(macroId, val);
}

void Sequencer::playSong(const unsigned char* arrData, bool dieOnEnd)
{
    m_arrData = arrData;
    m_dieOnEnd = dieOnEnd;
    m_state = SequencerState::Playing;
}

}

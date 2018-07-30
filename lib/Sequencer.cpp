#include "amuse/Sequencer.hpp"
#include "amuse/Submix.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Engine.hpp"
#include <map>

namespace amuse
{

void Sequencer::ChannelState::_bringOutYourDead()
{
    for (auto it = m_chanVoxs.begin(); it != m_chanVoxs.end();)
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

    for (auto it = m_keyoffVoxs.begin(); it != m_keyoffVoxs.end();)
    {
        Voice* vox = it->get();
        vox->_bringOutYourDead();
        if (vox->_isRecursivelyDead())
        {
            it = m_keyoffVoxs.erase(it);
            continue;
        }
        ++it;
    }
}

void Sequencer::_bringOutYourDead()
{
    for (auto& chan : m_chanStates)
        if (chan)
            chan._bringOutYourDead();

    if (!m_arrData && m_dieOnEnd && getVoiceCount() == 0)
        m_state = SequencerState::Dead;
}

void Sequencer::_destroy()
{
    Entity::_destroy();
    if (m_studio)
        m_studio.reset();
}

Sequencer::~Sequencer()
{
    if (m_studio)
        m_studio.reset();
}

Sequencer::Sequencer(Engine& engine, const AudioGroup& group, int groupId, const SongGroupIndex* songGroup, int setupId,
                     ObjToken<Studio> studio)
: Entity(engine, group, groupId), m_songGroup(songGroup), m_studio(studio)
{
    auto it = m_songGroup->m_midiSetups.find(setupId);
    if (it != m_songGroup->m_midiSetups.cend())
        m_midiSetup = it->second.data();
}

Sequencer::Sequencer(Engine& engine, const AudioGroup& group, int groupId, const SFXGroupIndex* sfxGroup,
                     ObjToken<Studio> studio)
: Entity(engine, group, groupId), m_sfxGroup(sfxGroup), m_studio(studio)
{
    std::map<uint16_t, const SFXGroupIndex::SFXEntry*> sortSFX;
    for (const auto& sfx : sfxGroup->m_sfxEntries)
        sortSFX[sfx.first] = &sfx.second;

    m_sfxMappings.reserve(sortSFX.size());
    for (const auto& sfx : sortSFX)
        m_sfxMappings.push_back(sfx.second);
}

Sequencer::ChannelState::~ChannelState() {}

Sequencer::ChannelState::ChannelState(Sequencer& parent, uint8_t chanId)
: m_parent(&parent), m_chanId(chanId)
{
    if (m_parent->m_songGroup)
    {
        if (m_parent->m_midiSetup)
        {
            m_setup = &m_parent->m_midiSetup[chanId];

            if (chanId == 9)
            {
                auto it = m_parent->m_songGroup->m_drumPages.find(m_setup->programNo);
                if (it != m_parent->m_songGroup->m_drumPages.cend())
                {
                    m_page = &it->second;
                    m_curProgram = m_setup->programNo;
                }
            }
            else
            {
                auto it = m_parent->m_songGroup->m_normPages.find(m_setup->programNo);
                if (it != m_parent->m_songGroup->m_normPages.cend())
                {
                    m_page = &it->second;
                    m_curProgram = m_setup->programNo;
                }
            }

            m_curVol = m_setup->volume / 127.f;
            m_curPan = m_setup->panning / 64.f - 1.f;
            m_ctrlVals[0x5b] = m_setup->reverb;
            m_ctrlVals[0x5d] = m_setup->chorus;
        }
        else
        {
            if (chanId == 9)
            {
                auto it = m_parent->m_songGroup->m_drumPages.find(0);
                if (it != m_parent->m_songGroup->m_drumPages.cend())
                    m_page = &it->second;
            }
            else
            {
                auto it = m_parent->m_songGroup->m_normPages.find(0);
                if (it != m_parent->m_songGroup->m_normPages.cend())
                    m_page = &it->second;
            }

            m_curVol = 1.f;
            m_curPan = 0.f;
            m_ctrlVals[0x5b] = 0;
            m_ctrlVals[0x5d] = 0;
        }
    }
    else if (m_parent->m_sfxGroup)
    {
        m_curVol = 1.f;
        m_curPan = 0.f;
        m_ctrlVals[0x5b] = 0;
        m_ctrlVals[0x5d] = 0;
    }

    m_ctrlVals[7] = 127;
    m_ctrlVals[10] = 64;
}

void Sequencer::advance(double dt)
{
    if (m_state == SequencerState::Playing)
    {
        if (m_stopFadeTime)
        {
            float step = dt / m_stopFadeTime * m_stopFadeBeginVol;
            float vol = std::max(0.f, m_curVol - step);
            if (vol == 0.f)
            {
                m_arrData = nullptr;
                m_state = SequencerState::Interactive;
                allOff(true);
                m_stopFadeTime = 0.f;
                return;
            }
            else
            {
                setVolume(vol);
            }
        }
        else if (m_volFadeTime)
        {
            float step = dt / m_volFadeTime * std::fabs(m_volFadeTarget - m_volFadeStart);
            float vol;
            if (m_curVol < m_volFadeTarget)
                vol = std::min(m_volFadeTarget, m_curVol + step);
            else
                vol = std::max(m_volFadeTarget, m_curVol - step);
            if (vol == m_volFadeTarget)
                m_volFadeTime = 0.f;
            else
                setVolume(vol);
        }

        if (m_songState.advance(*this, dt))
        {
            m_arrData = nullptr;
            m_state = SequencerState::Interactive;
            allOff();
        }
    }
}

size_t Sequencer::ChannelState::getVoiceCount() const
{
    size_t ret = 0;
    for (const auto& vox : m_chanVoxs)
        ret += vox.second->getTotalVoices();
    for (const auto& vox : m_keyoffVoxs)
        ret += vox->getTotalVoices();
    return ret;
}

size_t Sequencer::getVoiceCount() const
{
    size_t ret = 0;
    for (const auto& chan : m_chanStates)
        if (chan)
            ret += chan.getVoiceCount();
    return ret;
}

ObjToken<Voice> Sequencer::ChannelState::keyOn(uint8_t note, uint8_t velocity)
{
    if (m_parent->m_songGroup && !m_page)
        return {};

    if (m_lastVoice && m_lastVoice->isDestroyed())
        m_lastVoice.reset();

    /* If portamento is enabled for voice, pre-empt spawning new voices */
    if (ObjToken<Voice> lastVoice = m_lastVoice)
    {
        uint8_t lastNote = lastVoice->getLastNote();
        if (lastVoice->doPortamento(note))
        {
            m_chanVoxs.erase(lastNote);
            m_chanVoxs[note] = lastVoice;
            return lastVoice;
        }
    }

    /* Ensure keyoff sent first */
    auto keySearch = m_chanVoxs.find(note);
    if (keySearch != m_chanVoxs.cend())
    {
        if (keySearch->second == m_lastVoice)
            m_lastVoice.reset();
        keySearch->second->keyOff();
        keySearch->second->setPedal(false);
        m_keyoffVoxs.emplace(std::move(keySearch->second));
        m_chanVoxs.erase(keySearch);
    }

    std::list<ObjToken<Voice>>::iterator ret = m_parent->m_engine._allocateVoice(
        m_parent->m_audioGroup, m_parent->m_groupId, NativeSampleRate, true, false, m_parent->m_studio);
    if (*ret)
    {
        m_chanVoxs[note] = *ret;
        (*ret)->installCtrlValues(m_ctrlVals);

        ObjectId oid;
        bool res;
        if (m_parent->m_songGroup)
        {
            oid = m_page->objId;
            res = (*ret)->loadPageObject(oid, m_parent->m_ticksPerSec, note, velocity, m_ctrlVals[1]);
        }
        else if (m_parent->m_sfxMappings.size())
        {
            size_t lookupIdx = note % m_parent->m_sfxMappings.size();
            const SFXGroupIndex::SFXEntry* sfxEntry = m_parent->m_sfxMappings[lookupIdx];
            oid = sfxEntry->macro;
            note = sfxEntry->defKey;
            res = (*ret)->loadMacroObject(oid, 0, m_parent->m_ticksPerSec, note, velocity, m_ctrlVals[1]);
        }
        else
            return {};

        if (!res)
        {
            m_parent->m_engine._destroyVoice(ret);
            return {};
        }
        (*ret)->setVolume(m_parent->m_curVol * m_curVol);
        (*ret)->setReverbVol(m_ctrlVals[0x5b] / 127.f);
        (*ret)->setAuxBVol(m_ctrlVals[0x5d] / 127.f);
        (*ret)->setPan(m_curPan);
        (*ret)->setPitchWheel(m_curPitchWheel);

        if (m_ctrlVals[64] > 64)
            (*ret)->setPedal(true);

        m_lastVoice = *ret;
    }

    return *ret;
}

ObjToken<Voice> Sequencer::keyOn(uint8_t chan, uint8_t note, uint8_t velocity)
{
    if (chan > 15)
        return {};

    if (!m_chanStates[chan])
        m_chanStates[chan] = ChannelState(*this, chan);

    return m_chanStates[chan].keyOn(note, velocity);
}

void Sequencer::ChannelState::keyOff(uint8_t note, uint8_t velocity)
{
    auto keySearch = m_chanVoxs.find(note);
    if (keySearch == m_chanVoxs.cend())
        return;

    if ((m_lastVoice && m_lastVoice->isDestroyed()) || keySearch->second == m_lastVoice)
        m_lastVoice.reset();
    keySearch->second->keyOff();
    m_keyoffVoxs.emplace(std::move(keySearch->second));
    m_chanVoxs.erase(keySearch);
}

void Sequencer::keyOff(uint8_t chan, uint8_t note, uint8_t velocity)
{
    if (chan > 15 || !m_chanStates[chan])
        return;

    m_chanStates[chan].keyOff(note, velocity);
}

void Sequencer::ChannelState::setCtrlValue(uint8_t ctrl, int8_t val)
{
    m_ctrlVals[ctrl] = val;
    for (const auto& vox : m_chanVoxs)
        vox.second->_notifyCtrlChange(ctrl, val);
    for (const auto& vox : m_keyoffVoxs)
        vox->_notifyCtrlChange(ctrl, val);

    switch (ctrl)
    {
    case 7:
        setVolume(val / 127.f);
        break;
    case 10:
        setPan(val / 64.f - 1.f);
        break;
    default:
        break;
    }
}

bool Sequencer::ChannelState::programChange(int8_t prog)
{
    if (m_parent->m_songGroup)
    {
        if (m_chanId == 9)
        {
            auto it = m_parent->m_songGroup->m_drumPages.find(prog);
            if (it != m_parent->m_songGroup->m_drumPages.cend())
            {
                m_page = &it->second;
                m_curProgram = prog;
                return true;
            }
        }
        else
        {
            auto it = m_parent->m_songGroup->m_normPages.find(prog);
            if (it != m_parent->m_songGroup->m_normPages.cend())
            {
                m_page = &it->second;
                m_curProgram = prog;
                return true;
            }
        }
    }
    return false;
}

void Sequencer::ChannelState::nextProgram()
{
    int newProg = m_curProgram;
    while ((newProg += 1) <= 127)
        if (programChange(newProg))
            break;
}

void Sequencer::ChannelState::prevProgram()
{
    int newProg = m_curProgram;
    while ((newProg -= 1) >= 0)
        if (programChange(newProg))
            break;
}

void Sequencer::setCtrlValue(uint8_t chan, uint8_t ctrl, int8_t val)
{
    if (chan > 15)
        return;

    if (ctrl == 0x66)
    {
        printf("Loop Start\n");
    }
    else if (ctrl == 0x67)
    {
        printf("Loop End\n");
    }

    if (!m_chanStates[chan])
        m_chanStates[chan] = ChannelState(*this, chan);

    m_chanStates[chan].setCtrlValue(ctrl, val);
}

void Sequencer::ChannelState::setPitchWheel(float pitchWheel)
{
    m_curPitchWheel = pitchWheel;
    for (const auto& vox : m_chanVoxs)
        vox.second->setPitchWheel(pitchWheel);
    for (const auto& vox : m_keyoffVoxs)
        vox->setPitchWheel(pitchWheel);
}

void Sequencer::setPitchWheel(uint8_t chan, float pitchWheel)
{
    if (chan > 15)
        return;

    if (!m_chanStates[chan])
        m_chanStates[chan] = ChannelState(*this, chan);

    m_chanStates[chan].setPitchWheel(pitchWheel);
}

void Sequencer::setTempo(double ticksPerSec) { m_ticksPerSec = ticksPerSec; }

void Sequencer::ChannelState::allOff()
{
    if (m_lastVoice && m_lastVoice->isDestroyed())
        m_lastVoice.reset();
    for (auto it = m_chanVoxs.begin(); it != m_chanVoxs.end();)
    {
        if (it->second == m_lastVoice)
            m_lastVoice.reset();
        it->second->keyOff();
        m_keyoffVoxs.emplace(std::move(it->second));
        it = m_chanVoxs.erase(it);
    }
}

void Sequencer::allOff(bool now)
{
    if (now)
        for (auto& chan : m_chanStates)
        {
            if (chan)
            {
                for (const auto& vox : chan.m_chanVoxs)
                    vox.second->kill();
                for (const auto& vox : chan.m_keyoffVoxs)
                    vox->kill();
                chan.m_chanVoxs.clear();
                chan.m_keyoffVoxs.clear();
            }
        }
    else
        for (auto& chan : m_chanStates)
            if (chan)
                chan.allOff();
}

void Sequencer::allOff(uint8_t chan, bool now)
{
    if (chan > 15 || !m_chanStates[chan])
        return;

    if (now)
    {
        for (const auto& vox : m_chanStates[chan].m_chanVoxs)
            vox.second->kill();
        for (const auto& vox : m_chanStates[chan].m_keyoffVoxs)
            vox->kill();
        m_chanStates[chan].m_chanVoxs.clear();
        m_chanStates[chan].m_keyoffVoxs.clear();
    }
    else
        m_chanStates[chan].allOff();
}

void Sequencer::ChannelState::killKeygroup(uint8_t kg, bool now)
{
    if (m_lastVoice && m_lastVoice->isDestroyed())
        m_lastVoice.reset();

    for (auto it = m_chanVoxs.begin(); it != m_chanVoxs.end();)
    {
        Voice* vox = it->second.get();
        if (vox->m_keygroup == kg)
        {
            if (it->second == m_lastVoice)
                m_lastVoice.reset();
            if (now)
            {
                it = m_chanVoxs.erase(it);
                continue;
            }
            vox->keyOff();
            m_keyoffVoxs.emplace(std::move(it->second));
            it = m_chanVoxs.erase(it);
            continue;
        }
        ++it;
    }

    if (now)
    {
        for (auto it = m_keyoffVoxs.begin(); it != m_keyoffVoxs.end();)
        {
            Voice* vox = it->get();
            if (vox->m_keygroup == kg)
            {
                it = m_keyoffVoxs.erase(it);
                continue;
            }
            ++it;
        }
    }
}

void Sequencer::killKeygroup(uint8_t kg, bool now)
{
    for (auto& chan : m_chanStates)
        if (chan)
            chan.killKeygroup(kg, now);
}

ObjToken<Voice> Sequencer::ChannelState::findVoice(int vid)
{
    for (const auto& vox : m_chanVoxs)
        if (vox.second->vid() == vid)
            return vox.second;
    for (const auto& vox : m_keyoffVoxs)
        if (vox->vid() == vid)
            return vox;
    return {};
}

ObjToken<Voice> Sequencer::findVoice(int vid)
{
    for (auto& chan : m_chanStates)
    {
        if (chan)
        {
            ObjToken<Voice> ret = chan.findVoice(vid);
            if (ret)
                return ret;
        }
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
    for (const auto& v : m_keyoffVoxs)
    {
        Voice* vox = v.get();
        if (vox->getObjectId() == macroId)
            vox->message(val);
    }
}

void Sequencer::sendMacroMessage(ObjectId macroId, int32_t val)
{
    for (auto& chan : m_chanStates)
        if (chan)
            chan.sendMacroMessage(macroId, val);
}

void Sequencer::playSong(const unsigned char* arrData, bool dieOnEnd)
{
    m_arrData = arrData;
    m_dieOnEnd = dieOnEnd;
    m_songState.initialize(arrData);
    setTempo(m_songState.getTempo() * 384 / 60);
    m_state = SequencerState::Playing;
}

void Sequencer::stopSong(float fadeTime, bool now)
{
    if (fadeTime == 0.f)
    {
        allOff(now);
        m_arrData = nullptr;
        m_state = SequencerState::Interactive;
    }
    else
    {
        m_stopFadeTime = fadeTime;
        m_stopFadeBeginVol = m_curVol;
    }
}

void Sequencer::ChannelState::setVolume(float vol)
{
    m_curVol = vol;
    float voxVol = m_parent->m_curVol * m_curVol;
    for (const auto& v : m_chanVoxs)
    {
        Voice* vox = v.second.get();
        vox->setVolume(voxVol);
    }
    for (const auto& v : m_keyoffVoxs)
    {
        Voice* vox = v.get();
        vox->setVolume(voxVol);
    }
}

void Sequencer::ChannelState::setPan(float pan)
{
    m_curPan = pan;
    for (const auto& v : m_chanVoxs)
    {
        Voice* vox = v.second.get();
        vox->setPan(m_curPan);
    }
    for (const auto& v : m_keyoffVoxs)
    {
        Voice* vox = v.get();
        vox->setPan(m_curPan);
    }
}

void Sequencer::setVolume(float vol, float fadeTime)
{
    if (fadeTime == 0.f)
    {
        m_curVol = vol;
        for (auto& chan : m_chanStates)
            if (chan)
                chan.setVolume(chan.m_curVol);
    }
    else
    {
        m_volFadeTime = fadeTime;
        m_volFadeTarget = vol;
        m_volFadeStart = m_curVol;
    }
}

int8_t Sequencer::getChanProgram(int8_t chanId) const
{
    if (chanId > 15)
        return 0;

    if (!m_chanStates[chanId])
    {
        if (!m_midiSetup)
            return 0;
        return m_midiSetup[chanId].programNo;
    }

    return m_chanStates[chanId].m_curProgram;
}

bool Sequencer::setChanProgram(int8_t chanId, int8_t prog)
{
    if (chanId > 15)
        return false;

    if (!m_chanStates[chanId])
        m_chanStates[chanId] = ChannelState(*this, chanId);

    return m_chanStates[chanId].programChange(prog);
}

void Sequencer::nextChanProgram(int8_t chanId)
{
    if (chanId > 15)
        return;

    if (!m_chanStates[chanId])
        m_chanStates[chanId] = ChannelState(*this, chanId);

    return m_chanStates[chanId].nextProgram();
}

void Sequencer::prevChanProgram(int8_t chanId)
{
    if (chanId > 15)
        return;

    if (!m_chanStates[chanId])
        m_chanStates[chanId] = ChannelState(*this, chanId);

    return m_chanStates[chanId].prevProgram();
}
}

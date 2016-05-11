#include "amuse/Engine.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Submix.hpp"
#include "amuse/Sequencer.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"
#include "amuse/AudioGroupData.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/Common.hpp"

namespace amuse
{

Engine::Engine(IBackendVoiceAllocator& backend)
: m_backend(backend)
{}

Voice* Engine::_allocateVoice(const AudioGroup& group, double sampleRate,
                              bool dynamicPitch, bool emitter, Submix* smx)
{
    auto it = m_activeVoices.emplace(m_activeVoices.end(), *this, group, m_nextVid++, emitter, smx);
    m_activeVoices.back().m_backendVoice =
        m_backend.allocateVoice(m_activeVoices.back(), sampleRate, dynamicPitch);
    m_activeVoices.back().m_engineIt = it;
    return &m_activeVoices.back();
}

Submix* Engine::_allocateSubmix(Submix* smx)
{
    auto it = m_activeSubmixes.emplace(m_activeSubmixes.end(), *this, smx);
    m_activeSubmixes.back().m_backendSubmix = m_backend.allocateSubmix(m_activeSubmixes.back());
    m_activeSubmixes.back().m_engineIt = it;
    return &m_activeSubmixes.back();
}

std::list<Voice>::iterator Engine::_destroyVoice(Voice* voice)
{
#ifndef NDEBUG
    assert(this == &voice->getEngine());
#endif
    voice->_destroy();
    return m_activeVoices.erase(voice->m_engineIt);
}

std::list<Submix>::iterator Engine::_destroySubmix(Submix* smx)
{
#ifndef NDEBUG
    assert(this == &smx->getEngine());
#endif
    smx->_destroy();
    return m_activeSubmixes.erase(smx->m_engineIt);
}

/** Update all active audio entities and fill OS audio buffers as needed */
void Engine::pumpEngine()
{
    int maxVid = -1;
    for (Voice& vox : m_activeVoices)
    {
        maxVid = std::max(maxVid, vox.vid());
    }
    m_nextVid = maxVid + 1;
}

/** Add audio group data pointers to engine; must remain resident! */
const AudioGroup* Engine::addAudioGroup(int groupId, const AudioGroupData& data)
{
    std::unique_ptr<AudioGroup> grp = std::make_unique<AudioGroup>(groupId, data);
    if (!grp)
        return nullptr;
    AudioGroup* ret = grp.get();
    m_audioGroups.emplace(std::make_pair(groupId, std::move(grp)));

    /* setup SFX index for contained objects */
    for (const auto& pair : ret->getProj().sfxGroups())
    {
        const SFXGroupIndex& sfxGroup = pair.second;
        m_sfxLookup.reserve(m_sfxLookup.size() + sfxGroup.m_sfxEntries.size());
        for (const auto& pair : sfxGroup.m_sfxEntries)
            m_sfxLookup[pair.first] = std::make_pair(ret, pair.second->objId);
    }

    return ret;
}

/** Remove audio group from engine */
void Engine::removeAudioGroup(int groupId)
{
    auto search = m_audioGroups.find(groupId);
    if (search == m_audioGroups.end())
        return;
    AudioGroup* grp = search->second.get();

    /* Destroy runtime entities within group */
    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ;)
    {
        if (it->getAudioGroup().groupId() == groupId)
        {
            it->_destroy();
            it = m_activeVoices.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeEmitters.begin() ; it != m_activeEmitters.end() ;)
    {
        if (it->getAudioGroup().groupId() == groupId)
        {
            it->_destroy();
            it = m_activeEmitters.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeSequencers.begin() ; it != m_activeSequencers.end() ;)
    {
        if (it->getAudioGroup().groupId() == groupId)
        {
            it->_destroy();
            it = m_activeSequencers.erase(it);
            continue;
        }
        ++it;
    }

    /* teardown SFX index for contained objects */
    for (const auto& pair : grp->getProj().sfxGroups())
    {
        const SFXGroupIndex& sfxGroup = pair.second;
        for (const auto& pair : sfxGroup.m_sfxEntries)
            m_sfxLookup.erase(pair.first);
    }

    m_audioGroups.erase(groupId);
}

/** Create new Submix (a.k.a 'Studio') within root mix engine */
Submix* Engine::addSubmix(Submix* smx)
{
    return _allocateSubmix(smx);
}

/** Remove Submix and deallocate */
void Engine::removeSubmix(Submix* smx)
{
    /* Delete all voices bound to submix */
    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ;)
    {
        Submix* vsmx = it->getSubmix();
        if (vsmx && vsmx == smx)
        {
            it->_destroy();
            it = m_activeVoices.erase(it);
            continue;
        }
        ++it;
    }

    /* Delete all submixes bound to submix */
    for (auto it = m_activeSubmixes.begin() ; it != m_activeSubmixes.end() ;)
    {
        Submix* ssmx = it->getParentSubmix();
        if (ssmx && ssmx == smx)
        {
            it->_destroy();
            it = m_activeSubmixes.erase(it);
            continue;
        }
        ++it;
    }

    /* Delete submix */
    _destroySubmix(smx);
}

/** Start soundFX playing from loaded audio groups */
Voice* Engine::fxStart(int sfxId, float vol, float pan, Submix* smx)
{
    auto search = m_sfxLookup.find(sfxId);
    if (search == m_sfxLookup.end())
        return nullptr;

    AudioGroup* grp = search->second.first;
    if (!grp)
        return nullptr;

    Voice* ret = _allocateVoice(*grp, 32000.0, true, false, smx);
    ret->setVolume(vol);
    ret->setPanning(pan);
    return ret;
}

/** Start soundFX playing from loaded audio groups, attach to positional emitter */
Emitter* Engine::addEmitter(const Vector3f& pos, const Vector3f& dir, float maxDist,
                            float falloff, int sfxId, float minVol, float maxVol, Submix* smx)
{
    auto search = m_sfxLookup.find(sfxId);
    if (search == m_sfxLookup.end())
        return nullptr;

    AudioGroup* grp = search->second.first;
    if (!grp)
        return nullptr;

    Voice* vox = _allocateVoice(*grp, 32000.0, true, true, smx);
    m_activeEmitters.emplace_back(*this, *grp, *vox);
    Emitter& ret = m_activeEmitters.back();
    ret.setPos(pos);
    ret.setDir(dir);
    ret.setMaxDist(maxDist);
    ret.setFalloff(falloff);
    ret.setMinVol(minVol);
    ret.setMaxVol(maxVol);

    return &ret;
}

/** Start song playing from loaded audio groups */
Sequencer* Engine::seqPlay(int groupId, int songId, const unsigned char* arrData)
{
}

/** Find voice from VoiceId */
Voice* Engine::findVoice(int vid)
{
    for (Voice& vox : m_activeVoices)
        if (vox.vid() == vid)
            return &vox;
    return nullptr;
}

/** Stop all voices in `kg`, stops immediately (no KeyOff) when `flag` set */
void Engine::killKeygroup(uint8_t kg, uint8_t flag)
{
    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ;)
    {
        if (it->m_keygroup == kg)
        {
            if (flag)
            {
                it = _destroyVoice(&*it);
                continue;
            }
            it->keyOff();
        }
        ++it;
    }
}

/** Send all voices using `macroId` the message `val` */
void Engine::sendMacroMessage(ObjectId macroId, int32_t val)
{
    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ; ++it)
        if (it->getObjectId() == macroId)
            it->message(val);
}

}

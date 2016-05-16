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

Engine::~Engine() {}

Engine::Engine(IBackendVoiceAllocator& backend)
: m_backend(backend)
{}

std::pair<AudioGroup*, const SongGroupIndex*> Engine::_findSongGroup(int groupId) const
{
    for (const auto& pair : m_audioGroups)
    {
        const SongGroupIndex* ret = pair.second->getProj().getSongGroupIndex(groupId);
        if (ret)
            return {pair.second.get(), ret};
    }
    return {};
}

std::pair<AudioGroup*, const SFXGroupIndex*> Engine::_findSFXGroup(int groupId) const
{
    for (const auto& pair : m_audioGroups)
    {
        const SFXGroupIndex* ret = pair.second->getProj().getSFXGroupIndex(groupId);
        if (ret)
            return {pair.second.get(), ret};
    }
    return {};
}

std::shared_ptr<Voice> Engine::_allocateVoice(const AudioGroup& group, int groupId, double sampleRate,
                                              bool dynamicPitch, bool emitter, Submix* smx)
{
    auto it = m_activeVoices.emplace(m_activeVoices.end(),
        new Voice(*this, group, groupId, m_nextVid++, emitter, smx));
    m_activeVoices.back()->m_backendVoice =
        m_backend.allocateVoice(*m_activeVoices.back(), sampleRate, dynamicPitch);
    m_activeVoices.back()->m_engineIt = it;
    return m_activeVoices.back();
}

std::shared_ptr<Sequencer> Engine::_allocateSequencer(const AudioGroup& group, int groupId,
                                                      int setupId, Submix* smx)
{
    const SongGroupIndex* songGroup = group.getProj().getSongGroupIndex(groupId);
    if (!songGroup)
        return {};
    auto it = m_activeSequencers.emplace(m_activeSequencers.end(),
        new Sequencer(*this, group, groupId, *songGroup, setupId, smx));
    m_activeSequencers.back()->m_engineIt = it;
    return m_activeSequencers.back();
}

Submix* Engine::_allocateSubmix(Submix* smx)
{
    auto it = m_activeSubmixes.emplace(m_activeSubmixes.end(), *this, smx);
    m_activeSubmixes.back().m_backendSubmix = m_backend.allocateSubmix(m_activeSubmixes.back());
    m_activeSubmixes.back().m_engineIt = it;
    return &m_activeSubmixes.back();
}

std::list<std::shared_ptr<Voice>>::iterator Engine::_destroyVoice(Voice* voice)
{
#ifndef NDEBUG
    assert(this == &voice->getEngine());
#endif
    voice->_destroy();
    return m_activeVoices.erase(voice->m_engineIt);
}

std::list<std::shared_ptr<Sequencer>>::iterator Engine::_destroySequencer(Sequencer* sequencer)
{
#ifndef NDEBUG
    assert(this == &sequencer->getEngine());
#endif
    sequencer->_destroy();
    return m_activeSequencers.erase(sequencer->m_engineIt);
}

std::list<Submix>::iterator Engine::_destroySubmix(Submix* smx)
{
#ifndef NDEBUG
    assert(this == &smx->getEngine());
#endif
    smx->_destroy();
    return m_activeSubmixes.erase(smx->m_engineIt);
}

void Engine::_bringOutYourDead()
{
    for (auto it = m_activeEmitters.begin() ; it != m_activeEmitters.end() ;)
    {
        Emitter* emitter = it->get();
        if (emitter->getVoice()->_isRecursivelyDead())
        {
            emitter->_destroy();
            it = m_activeEmitters.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ;)
    {
        Voice* vox = it->get();
        vox->_bringOutYourDead();
        if (vox->_isRecursivelyDead())
        {
            it = _destroyVoice(vox);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeSequencers.begin() ; it != m_activeSequencers.end() ;)
    {
        Sequencer* seq = it->get();
        seq->_bringOutYourDead();
        if (seq->m_state == SequencerState::Dead)
        {
            it = _destroySequencer(seq);
            continue;
        }
        ++it;
    }
}

/** Update all active audio entities and fill OS audio buffers as needed */
void Engine::pumpEngine()
{
    m_backend.pumpAndMixVoices();
    _bringOutYourDead();

    /* Determine lowest available free vid */
    int maxVid = -1;
    for (std::shared_ptr<Voice>& vox : m_activeVoices)
        maxVid = std::max(maxVid, vox->maxVid());
    m_nextVid = maxVid + 1;
}

/** Add audio group data pointers to engine; must remain resident! */
const AudioGroup* Engine::addAudioGroup(const AudioGroupData& data)
{
    removeAudioGroup(data);

    std::unique_ptr<AudioGroup> grp = std::make_unique<AudioGroup>(data);
    if (!grp)
        return nullptr;
    AudioGroup* ret = grp.get();
    m_audioGroups.emplace(std::make_pair(&data, std::move(grp)));

    /* setup SFX index for contained objects */
    for (const auto& grp : ret->getProj().sfxGroups())
    {
        const SFXGroupIndex& sfxGroup = grp.second;
        m_sfxLookup.reserve(m_sfxLookup.size() + sfxGroup.m_sfxEntries.size());
        for (const auto& ent : sfxGroup.m_sfxEntries)
            m_sfxLookup[ent.first] = std::make_tuple(ret, grp.first, ent.second);
    }

    return ret;
}

/** Remove audio group from engine */
void Engine::removeAudioGroup(const AudioGroupData& data)
{
    auto search = m_audioGroups.find(&data);
    if (search == m_audioGroups.cend())
        return;
    AudioGroup* grp = search->second.get();

    /* Destroy runtime entities within group */
    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ;)
    {
        Voice* vox = it->get();
        if (&vox->getAudioGroup() == grp)
        {
            vox->_destroy();
            it = m_activeVoices.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeEmitters.begin() ; it != m_activeEmitters.end() ;)
    {
        Emitter* emitter = it->get();
        if (&emitter->getAudioGroup() == grp)
        {
            emitter->_destroy();
            it = m_activeEmitters.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeSequencers.begin() ; it != m_activeSequencers.end() ;)
    {
        Sequencer* seq = it->get();
        if (&seq->getAudioGroup() == grp)
        {
            seq->_destroy();
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

    m_audioGroups.erase(search);
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
        Voice* vox = it->get();

        Submix* vsmx = vox->getSubmix();
        if (vsmx && vsmx == smx)
        {
            vox->_destroy();
            it = m_activeVoices.erase(it);
            continue;
        }
        ++it;
    }

    /* Delete all sequencers bound to submix */
    for (auto it = m_activeSequencers.begin() ; it != m_activeSequencers.end() ;)
    {
        Sequencer* seq = it->get();

        Submix* ssmx = seq->getSubmix();
        if (ssmx && ssmx == smx)
        {
            seq->_destroy();
            it = m_activeSequencers.erase(it);
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
std::shared_ptr<Voice> Engine::fxStart(int sfxId, float vol, float pan, Submix* smx)
{
    auto search = m_sfxLookup.find(sfxId);
    if (search == m_sfxLookup.end())
        return nullptr;

    AudioGroup* grp = std::get<0>(search->second);
    const SFXGroupIndex::SFXEntry* entry = std::get<2>(search->second);
    if (!grp)
        return nullptr;

    std::shared_ptr<Voice> ret = _allocateVoice(*grp, std::get<1>(search->second), 32000.0, true, false, smx);
    if (!ret->loadSoundObject(SBig(entry->objId), 0, 1000.f, entry->defKey, entry->defVel, 0))
    {
        _destroyVoice(ret.get());
        return {};
    }
    ret->setVolume(vol);
    ret->setPan(pan);
    return ret;
}

/** Start soundFX playing from loaded audio groups, attach to positional emitter */
std::shared_ptr<Emitter> Engine::addEmitter(const Vector3f& pos, const Vector3f& dir, float maxDist,
                                            float falloff, int sfxId, float minVol, float maxVol, Submix* smx)
{
    auto search = m_sfxLookup.find(sfxId);
    if (search == m_sfxLookup.end())
        return nullptr;

    AudioGroup* grp = std::get<0>(search->second);
    const SFXGroupIndex::SFXEntry* entry = std::get<2>(search->second);
    if (!grp)
        return nullptr;

    std::shared_ptr<Voice> vox = _allocateVoice(*grp, std::get<1>(search->second), 32000.0, true, true, smx);
    m_activeEmitters.emplace(m_activeEmitters.end(), new Emitter(*this, *grp, std::move(vox)));
    Emitter& ret = *m_activeEmitters.back();
    if (!ret.getVoice()->loadSoundObject(entry->objId, 0, 1000.f, entry->defKey, entry->defVel, 0))
    {
        ret._destroy();
        m_activeEmitters.pop_back();
        return {};
    }
    vox->setPan(entry->panning);
    ret.setPos(pos);
    ret.setDir(dir);
    ret.setMaxDist(maxDist);
    ret.setFalloff(falloff);
    ret.setMinVol(minVol);
    ret.setMaxVol(maxVol);

    return m_activeEmitters.back();
}

/** Start song playing from loaded audio groups */
std::shared_ptr<Sequencer> Engine::seqPlay(int groupId, int songId,
                                           const unsigned char* arrData, Submix* smx)
{
    std::pair<AudioGroup*, const SongGroupIndex*> songGrp = _findSongGroup(groupId);
    if (!songGrp.second)
        return {};

    std::shared_ptr<Sequencer> ret = _allocateSequencer(*songGrp.first, groupId, songId, smx);
    if (!ret)
        return {};

    if (arrData)
        ret->playSong(arrData);
    return ret;
}

/** Find voice from VoiceId */
std::shared_ptr<Voice> Engine::findVoice(int vid)
{
    for (std::shared_ptr<Voice>& vox : m_activeVoices)
    {
        std::shared_ptr<Voice> ret = vox->_findVoice(vid, vox);
        if (ret)
            return ret;
    }

    for (std::shared_ptr<Sequencer>& seq : m_activeSequencers)
    {
        std::shared_ptr<Voice> ret = seq->findVoice(vid);
        if (ret)
            return ret;
    }

    return {};
}

/** Stop all voices in `kg`, stops immediately (no KeyOff) when `flag` set */
void Engine::killKeygroup(uint8_t kg, bool now)
{
    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ;)
    {
        Voice* vox = it->get();
        if (vox->m_keygroup == kg)
        {
            if (now)
            {
                it = _destroyVoice(vox);
                continue;
            }
            vox->keyOff();
        }
        ++it;
    }

    for (std::shared_ptr<Sequencer>& seq : m_activeSequencers)
        seq->killKeygroup(kg, now);
}

/** Send all voices using `macroId` the message `val` */
void Engine::sendMacroMessage(ObjectId macroId, int32_t val)
{
    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ; ++it)
    {
        Voice* vox = it->get();
        if (vox->getObjectId() == macroId)
            vox->message(val);
    }

    for (std::shared_ptr<Sequencer>& seq : m_activeSequencers)
        seq->sendMacroMessage(macroId, val);
}

}

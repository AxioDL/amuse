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

Engine::~Engine()
{
    while (m_activeSequencers.size())
        m_activeSequencers.front()->_destroy();
    while (m_activeSubmixes.size())
        removeSubmix(&m_activeSubmixes.front());
    for (std::shared_ptr<Emitter>& emitter : m_activeEmitters)
        emitter->_destroy();
    for (std::shared_ptr<Voice>& vox : m_activeVoices)
        vox->_destroy();
}

Engine::Engine(IBackendVoiceAllocator& backend, AmplitudeMode ampMode)
: m_backend(backend), m_ampMode(ampMode)
{
    backend.register5MsCallback(std::bind(&Engine::_5MsCallback, this, std::placeholders::_1));
    m_midiReader = backend.allocateMIDIReader(*this);
}

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

std::list<std::shared_ptr<Voice>>::iterator
Engine::_allocateVoice(const AudioGroup& group, int groupId, double sampleRate,
                       bool dynamicPitch, bool emitter, Submix* smx)
{
    auto it = m_activeVoices.emplace(m_activeVoices.end(),
        new Voice(*this, group, groupId, m_nextVid++, emitter, smx));
    if (smx)
        m_activeVoices.back()->m_backendVoice =
            smx->m_backendSubmix->allocateVoice(*m_activeVoices.back(), sampleRate, dynamicPitch);
    else
        m_activeVoices.back()->m_backendVoice =
            m_backend.allocateVoice(*m_activeVoices.back(), sampleRate, dynamicPitch);
    return it;
}

std::list<std::shared_ptr<Sequencer>>::iterator
Engine::_allocateSequencer(const AudioGroup& group, int groupId,
                           int setupId, Submix* smx)
{
    const SongGroupIndex* songGroup = group.getProj().getSongGroupIndex(groupId);
    if (!songGroup)
        return {};
    auto it = m_activeSequencers.emplace(m_activeSequencers.end(),
        new Sequencer(*this, group, groupId, *songGroup, setupId, smx));
    printf("INSERT %p\n", it->get());
    return it;
}

std::list<Submix>::iterator Engine::_allocateSubmix(Submix* smx)
{
    auto it = m_activeSubmixes.emplace(m_activeSubmixes.end(), *this, smx);
    m_activeSubmixes.back().m_backendSubmix = m_backend.allocateSubmix(m_activeSubmixes.back());
    return it;
}

std::list<std::shared_ptr<Voice>>::iterator Engine::_destroyVoice(std::list<std::shared_ptr<Voice>>::iterator it)
{
#ifndef NDEBUG
    assert(this == &(*it)->getEngine());
#endif
    if ((*it)->m_destroyed)
        return m_activeVoices.begin();
    (*it)->_destroy();
    return m_activeVoices.erase(it);
}

std::list<std::shared_ptr<Sequencer>>::iterator Engine::_destroySequencer(std::list<std::shared_ptr<Sequencer>>::iterator it)
{
#ifndef NDEBUG
    assert(this == &(*it)->getEngine());
#endif
    if ((*it)->m_destroyed)
        return m_activeSequencers.begin();
    printf("TO DESTROY %p\n", it->get());
    (*it)->_destroy();
    printf("ERASE %p\n", it->get());
    return m_activeSequencers.erase(it);
}

std::list<Submix>::iterator Engine::_destroySubmix(std::list<Submix>::iterator it)
{
#ifndef NDEBUG
    assert(this == &it->getEngine());
#endif
    if (it->m_destroyed)
        return m_activeSubmixes.begin();
    it->_destroy();
    return m_activeSubmixes.erase(it);
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
            it = _destroyVoice(it);
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
            it = _destroySequencer(it);
            continue;
        }
        ++it;
    }
}

void Engine::_5MsCallback(double dt)
{
    if (m_midiReader)
        m_midiReader->pumpReader(dt);
    for (std::shared_ptr<Sequencer>& seq : m_activeSequencers)
        seq->advance(dt);
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

AudioGroup* Engine::_addAudioGroup(const AudioGroupData& data, std::unique_ptr<AudioGroup>&& grp)
{
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

/** Add GameCube audio group data pointers to engine; must remain resident! */
const AudioGroup* Engine::addAudioGroup(const AudioGroupData& data)
{
    removeAudioGroup(data);

    std::unique_ptr<AudioGroup> grp;
    switch (data.m_fmt)
    {
    case DataFormat::GCN:
        grp = std::make_unique<AudioGroup>(data, GCNDataTag{});
        break;
    case DataFormat::N64:
        grp = std::make_unique<AudioGroup>(data, data.m_absOffs, N64DataTag{});
        break;
    case DataFormat::PC:
        grp = std::make_unique<AudioGroup>(data, data.m_absOffs, PCDataTag{});
        break;
    }
    if (!grp)
        return nullptr;

    return _addAudioGroup(data, std::move(grp));
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
    return &*_allocateSubmix(smx);
}

std::list<Submix>::iterator Engine::_removeSubmix(std::list<Submix>::iterator smx)
{
    /* Delete all voices bound to submix */
    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ;)
    {
        Voice* vox = it->get();

        Submix* vsmx = vox->getSubmix();
        if (vsmx == &*smx)
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
        if (ssmx == &*smx)
        {
            if (!seq->m_destroyed)
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
        if (ssmx == &*smx)
        {
            it = _removeSubmix(it);
            continue;
        }
        ++it;
    }

    /* Delete submix */
    return _destroySubmix(smx);
}

/** Remove Submix and deallocate */
void Engine::removeSubmix(Submix* smx)
{
    if (!smx)
        return;

    for (auto it = m_activeSubmixes.begin() ; it != m_activeSubmixes.end() ;)
    {
        if (&*it == &*smx)
        {
            it = _removeSubmix(it);
            break;
        }
        ++it;
    }
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

    std::list<std::shared_ptr<Voice>>::iterator ret =
        _allocateVoice(*grp, std::get<1>(search->second),
                       32000.0, true, false, smx);

    ObjectId oid = (grp->getDataFormat() == DataFormat::PC) ? entry->objId : SBig(entry->objId);
    if (!(*ret)->loadSoundObject(oid, 0, 1000.f, entry->defKey, entry->defVel, 0))
    {
        _destroyVoice(ret);
        return {};
    }

    (*ret)->setVolume(vol);
    (*ret)->setPan(pan);
    return *ret;
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

    std::list<std::shared_ptr<Voice>>::iterator vox =
        _allocateVoice(*grp, std::get<1>(search->second),
                       32000.0, true, true, smx);
    auto emitIt = m_activeEmitters.emplace(m_activeEmitters.end(), new Emitter(*this, *grp, std::move(*vox)));
    Emitter& ret = *(*emitIt);

    ObjectId oid = (grp->getDataFormat() == DataFormat::PC) ? entry->objId : SBig(entry->objId);
    if (!ret.getVoice()->loadSoundObject(oid, 0, 1000.f, entry->defKey, entry->defVel, 0))
    {
        ret._destroy();
        m_activeEmitters.erase(emitIt);
        return {};
    }

    (*vox)->setPan(entry->panning);
    ret.setPos(pos);
    ret.setDir(dir);
    ret.setMaxDist(maxDist);
    ret.setFalloff(falloff);
    ret.setMinVol(minVol);
    ret.setMaxVol(maxVol);

    return *emitIt;
}

/** Start song playing from loaded audio groups */
std::shared_ptr<Sequencer> Engine::seqPlay(int groupId, int songId,
                                           const unsigned char* arrData, Submix* smx)
{
    std::pair<AudioGroup*, const SongGroupIndex*> songGrp = _findSongGroup(groupId);
    if (!songGrp.second)
        return {};

    std::list<std::shared_ptr<Sequencer>>::iterator ret = _allocateSequencer(*songGrp.first, groupId, songId, smx);
    if (!*ret)
        return {};

    if (arrData)
        (*ret)->playSong(arrData);
    return *ret;
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
                it = _destroyVoice(it);
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

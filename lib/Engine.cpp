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

static const float FullLevels[8] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};

Engine::~Engine()
{
    m_backend.setCallbackInterface(nullptr);
    for (std::shared_ptr<Sequencer>& seq : m_activeSequencers)
        if (!seq->m_destroyed)
            seq->_destroy();
    for (std::shared_ptr<Emitter>& emitter : m_activeEmitters)
        emitter->_destroy();
    for (std::shared_ptr<Voice>& vox : m_activeVoices)
        vox->_destroy();
}

Engine::Engine(IBackendVoiceAllocator& backend, AmplitudeMode ampMode)
: m_backend(backend), m_ampMode(ampMode), m_defaultStudio(_allocateStudio(true))
{
    m_defaultStudio->getAuxA().makeReverbStd(0.5f, 0.8f, 3.0f, 0.5f, 0.1f);
    m_defaultStudio->getAuxB().makeChorus(15, 0, 500);
    m_defaultStudioReady = true;
    m_backend.setCallbackInterface(this);
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

std::list<std::shared_ptr<Voice>>::iterator Engine::_allocateVoice(const AudioGroup& group, int groupId,
                                                                   double sampleRate, bool dynamicPitch, bool emitter,
                                                                   std::weak_ptr<Studio> studio)
{
    std::shared_ptr<Studio> st = studio.lock();
    auto it =
        m_activeVoices.emplace(m_activeVoices.end(), new Voice(*this, group, groupId, m_nextVid++, emitter, studio));
    m_activeVoices.back()->m_backendVoice = m_backend.allocateVoice(*m_activeVoices.back(), sampleRate, dynamicPitch);
    m_activeVoices.back()->m_backendVoice->setChannelLevels(st->getMaster().m_backendSubmix.get(), FullLevels, false);
    m_activeVoices.back()->m_backendVoice->setChannelLevels(st->getAuxA().m_backendSubmix.get(), FullLevels, false);
    m_activeVoices.back()->m_backendVoice->setChannelLevels(st->getAuxB().m_backendSubmix.get(), FullLevels, false);
    return it;
}

std::list<std::shared_ptr<Sequencer>>::iterator Engine::_allocateSequencer(const AudioGroup& group, int groupId,
                                                                           int setupId, std::weak_ptr<Studio> studio)
{
    const SongGroupIndex* songGroup = group.getProj().getSongGroupIndex(groupId);
    if (songGroup)
    {
        auto it = m_activeSequencers.emplace(m_activeSequencers.end(),
                                             new Sequencer(*this, group, groupId, songGroup, setupId, studio));
        return it;
    }
    const SFXGroupIndex* sfxGroup = group.getProj().getSFXGroupIndex(groupId);
    if (sfxGroup)
    {
        auto it = m_activeSequencers.emplace(m_activeSequencers.end(),
                                             new Sequencer(*this, group, groupId, sfxGroup, studio));
        return it;
    }
    return {};
}

std::shared_ptr<Studio> Engine::_allocateStudio(bool mainOut)
{
    std::shared_ptr<Studio> ret = std::make_shared<Studio>(*this, mainOut);
    m_activeStudios.emplace(m_activeStudios.end(), ret);
    ret->m_master.m_backendSubmix = m_backend.allocateSubmix(ret->m_master, mainOut, 0);
    ret->m_auxA.m_backendSubmix = m_backend.allocateSubmix(ret->m_auxA, mainOut, 1);
    ret->m_auxB.m_backendSubmix = m_backend.allocateSubmix(ret->m_auxB, mainOut, 2);
    return ret;
}

std::list<std::shared_ptr<Voice>>::iterator Engine::_destroyVoice(std::list<std::shared_ptr<Voice>>::iterator it)
{
    assert(this == &(*it)->getEngine());
    if ((*it)->m_destroyed)
        return m_activeVoices.begin();
    (*it)->_destroy();
    return m_activeVoices.erase(it);
}

std::list<std::shared_ptr<Sequencer>>::iterator
Engine::_destroySequencer(std::list<std::shared_ptr<Sequencer>>::iterator it)
{
    assert(this == &(*it)->getEngine());
    if ((*it)->m_destroyed)
        return m_activeSequencers.begin();
    (*it)->_destroy();
    return m_activeSequencers.erase(it);
}

void Engine::_bringOutYourDead()
{
    for (auto it = m_activeEmitters.begin(); it != m_activeEmitters.end();)
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

    for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();)
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

    for (auto it = m_activeSequencers.begin(); it != m_activeSequencers.end();)
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

    for (auto it = m_activeStudios.begin(); it != m_activeStudios.end();)
    {
        std::shared_ptr<Studio> st = it->lock();
        if (!st)
            it = m_activeStudios.erase(it);
        else
            ++it;
    }
}

void Engine::_on5MsInterval(IBackendVoiceAllocator& engine, double dt)
{
    m_channelSet = engine.getAvailableSet();
    if (m_midiReader)
        m_midiReader->pumpReader(dt);
    for (std::shared_ptr<Sequencer>& seq : m_activeSequencers)
        seq->advance(dt);
    for (std::shared_ptr<Emitter>& emitter : m_activeEmitters)
        emitter->_update();
    for (std::shared_ptr<Listener>& listener : m_activeListeners)
        listener->m_dirty = false;
}

void Engine::_onPumpCycleComplete(IBackendVoiceAllocator& engine)
{
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
            m_sfxLookup[ent.first] = std::make_tuple(ret, grp.first, &ent.second);
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
    for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();)
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

    for (auto it = m_activeEmitters.begin(); it != m_activeEmitters.end();)
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

    for (auto it = m_activeSequencers.begin(); it != m_activeSequencers.end();)
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

/** Create new Studio within engine */
std::shared_ptr<Studio> Engine::addStudio(bool mainOut) { return _allocateStudio(mainOut); }

/** Start soundFX playing from loaded audio groups */
std::shared_ptr<Voice> Engine::fxStart(int sfxId, float vol, float pan, std::weak_ptr<Studio> smx)
{
    auto search = m_sfxLookup.find(sfxId);
    if (search == m_sfxLookup.end())
        return nullptr;

    AudioGroup* grp = std::get<0>(search->second);
    const SFXGroupIndex::SFXEntry* entry = std::get<2>(search->second);
    if (!grp)
        return nullptr;

    std::list<std::shared_ptr<Voice>>::iterator ret =
        _allocateVoice(*grp, std::get<1>(search->second), NativeSampleRate, true, false, smx);

    if (!(*ret)->loadSoundObject(entry->objId, 0, 1000.f, entry->defKey, entry->defVel, 0))
    {
        _destroyVoice(ret);
        return {};
    }

    (*ret)->setVolume(vol);
    (*ret)->setPan(pan);
    return *ret;
}

/** Start soundFX playing from loaded audio groups, attach to positional emitter */
std::shared_ptr<Emitter> Engine::addEmitter(const float* pos, const float* dir, float maxDist, float falloff,
                                            int sfxId, float minVol, float maxVol, bool doppler,
                                            std::weak_ptr<Studio> smx)
{
    auto search = m_sfxLookup.find(sfxId);
    if (search == m_sfxLookup.end())
        return nullptr;

    AudioGroup* grp = std::get<0>(search->second);
    const SFXGroupIndex::SFXEntry* entry = std::get<2>(search->second);
    if (!grp)
        return nullptr;

    std::list<std::shared_ptr<Voice>>::iterator vox =
        _allocateVoice(*grp, std::get<1>(search->second), NativeSampleRate, true, true, smx);

    if (!(*vox)->loadSoundObject(entry->objId, 0, 1000.f, entry->defKey, entry->defVel, 0))
    {
        _destroyVoice(vox);
        return {};
    }

    auto emitIt = m_activeEmitters.emplace(m_activeEmitters.end(),
                                           new Emitter(*this, *grp, *vox, maxDist, minVol, falloff, doppler));
    Emitter& ret = *(*emitIt);

    ret.getVoice()->setPan(entry->panning);
    ret.setVectors(pos, dir);
    ret.setMaxVol(maxVol);

    return *emitIt;
}

/** Build listener and add to engine's listener list */
std::shared_ptr<Listener> Engine::addListener(const float* pos, const float* dir, const float* heading, const float* up,
                                              float frontDiff, float backDiff, float soundSpeed, float volume)
{
    auto listenerIt = m_activeListeners.emplace(m_activeListeners.end(),
        new Listener(volume, frontDiff, backDiff, soundSpeed));
    Listener& ret = *(*listenerIt);
    ret.setVectors(pos, dir, heading, up);
    return *listenerIt;
}

/** Remove listener from engine's listener list */
void Engine::removeListener(Listener* listener)
{
    for (auto it = m_activeListeners.begin() ; it != m_activeListeners.end() ; ++it)
    {
        if (it->get() == listener)
        {
            m_activeListeners.erase(it);
            return;
        }
    }
}

/** Start song playing from loaded audio groups */
std::shared_ptr<Sequencer> Engine::seqPlay(int groupId, int songId, const unsigned char* arrData,
                                           std::weak_ptr<Studio> smx)
{
    std::pair<AudioGroup*, const SongGroupIndex*> songGrp = _findSongGroup(groupId);
    if (songGrp.second)
    {
        std::list<std::shared_ptr<Sequencer>>::iterator ret = _allocateSequencer(*songGrp.first, groupId, songId, smx);
        if (!*ret)
            return {};

        if (arrData)
            (*ret)->playSong(arrData);
        return *ret;
    }

    std::pair<AudioGroup*, const SFXGroupIndex*> sfxGrp = _findSFXGroup(groupId);
    if (sfxGrp.second)
    {
        std::list<std::shared_ptr<Sequencer>>::iterator ret = _allocateSequencer(*sfxGrp.first, groupId, songId, smx);
        if (!*ret)
            return {};
        return *ret;
    }

    return {};
}

/** Set total volume of engine */
void Engine::setVolume(float vol)
{
    m_masterVolume = vol;
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
    for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();)
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
    for (auto it = m_activeVoices.begin(); it != m_activeVoices.end(); ++it)
    {
        Voice* vox = it->get();
        if (vox->getObjectId() == macroId)
            vox->message(val);
    }

    for (std::shared_ptr<Sequencer>& seq : m_activeSequencers)
        seq->sendMacroMessage(macroId, val);
}
}

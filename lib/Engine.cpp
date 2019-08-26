#include "amuse/Engine.hpp"

#include "amuse/AudioGroup.hpp"
#include "amuse/AudioGroupData.hpp"
#include "amuse/Common.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"
#include "amuse/Sequencer.hpp"
#include "amuse/Submix.hpp"
#include "amuse/Voice.hpp"

namespace amuse {

static const float FullLevels[8] = {1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f, 1.f};

Engine::~Engine() {
  m_backend.setCallbackInterface(nullptr);
  for (ObjToken<Sequencer>& seq : m_activeSequencers)
    if (!seq->m_destroyed)
      seq->_destroy();
  for (ObjToken<Emitter>& emitter : m_activeEmitters)
    emitter->_destroy();
  for (ObjToken<Voice>& vox : m_activeVoices)
    vox->_destroy();
}

Engine::Engine(IBackendVoiceAllocator& backend, AmplitudeMode ampMode)
: m_backend(backend), m_ampMode(ampMode), m_defaultStudio(_allocateStudio(true)) {
  m_defaultStudio->getAuxA().makeReverbStd(0.5f, 0.8f, 3.0f, 0.5f, 0.1f);
  m_defaultStudio->getAuxB().makeChorus(15, 0, 500);
  m_defaultStudioReady = true;
  m_backend.setCallbackInterface(this);
  m_midiReader = backend.allocateMIDIReader(*this);
}

std::pair<AudioGroup*, const SongGroupIndex*> Engine::_findSongGroup(GroupId groupId) const {
  for (const auto& pair : m_audioGroups) {
    const SongGroupIndex* ret = pair.second->getProj().getSongGroupIndex(groupId);
    if (ret)
      return {pair.second.get(), ret};
  }
  return {};
}

std::pair<AudioGroup*, const SFXGroupIndex*> Engine::_findSFXGroup(GroupId groupId) const {
  for (const auto& pair : m_audioGroups) {
    const SFXGroupIndex* ret = pair.second->getProj().getSFXGroupIndex(groupId);
    if (ret)
      return {pair.second.get(), ret};
  }
  return {};
}

std::list<ObjToken<Voice>>::iterator Engine::_allocateVoice(const AudioGroup& group, GroupId groupId, double sampleRate,
                                                            bool dynamicPitch, bool emitter, ObjToken<Studio> studio) {
  auto it =
      m_activeVoices.emplace(m_activeVoices.end(), MakeObj<Voice>(*this, group, groupId, m_nextVid++, emitter, studio));
  m_activeVoices.back()->m_backendVoice = m_backend.allocateVoice(*m_activeVoices.back(), sampleRate, dynamicPitch);
  m_activeVoices.back()->m_backendVoice->setChannelLevels(studio->getMaster().m_backendSubmix.get(), FullLevels, false);
  m_activeVoices.back()->m_backendVoice->setChannelLevels(studio->getAuxA().m_backendSubmix.get(), FullLevels, false);
  m_activeVoices.back()->m_backendVoice->setChannelLevels(studio->getAuxB().m_backendSubmix.get(), FullLevels, false);
  return it;
}

std::list<ObjToken<Sequencer>>::iterator Engine::_allocateSequencer(const AudioGroup& group, GroupId groupId,
                                                                    SongId setupId, ObjToken<Studio> studio) {
  const SongGroupIndex* songGroup = group.getProj().getSongGroupIndex(groupId);
  if (songGroup) {
    amuse::ObjToken<Sequencer> tok = MakeObj<Sequencer>(*this, group, groupId, songGroup, setupId, studio);
    auto it = m_activeSequencers.emplace(m_activeSequencers.end(), tok);
    return it;
  }
  const SFXGroupIndex* sfxGroup = group.getProj().getSFXGroupIndex(groupId);
  if (sfxGroup) {
    amuse::ObjToken<Sequencer> tok = MakeObj<Sequencer>(*this, group, groupId, sfxGroup, studio);
    auto it = m_activeSequencers.emplace(m_activeSequencers.end(), tok);
    return it;
  }
  return {};
}

ObjToken<Studio> Engine::_allocateStudio(bool mainOut) {
  ObjToken<Studio> ret = MakeObj<Studio>(*this, mainOut);
  ret->m_master.m_backendSubmix = m_backend.allocateSubmix(ret->m_master, mainOut, 0);
  ret->m_auxA.m_backendSubmix = m_backend.allocateSubmix(ret->m_auxA, mainOut, 1);
  ret->m_auxB.m_backendSubmix = m_backend.allocateSubmix(ret->m_auxB, mainOut, 2);
  return ret;
}

std::list<ObjToken<Voice>>::iterator Engine::_destroyVoice(std::list<ObjToken<Voice>>::iterator it) {
  assert(this == &(*it)->getEngine());
  if ((*it)->m_destroyed)
    return m_activeVoices.begin();
  (*it)->_destroy();
  return m_activeVoices.erase(it);
}

std::list<ObjToken<Sequencer>>::iterator Engine::_destroySequencer(std::list<ObjToken<Sequencer>>::iterator it) {
  assert(this == &(*it)->getEngine());
  if ((*it)->m_destroyed)
    return m_activeSequencers.begin();
  (*it)->_destroy();
  return m_activeSequencers.erase(it);
}

void Engine::_bringOutYourDead() {
  for (auto it = m_activeEmitters.begin(); it != m_activeEmitters.end();) {
    Emitter* emitter = it->get();
    if (emitter->getVoice()->_isRecursivelyDead()) {
      emitter->_destroy();
      it = m_activeEmitters.erase(it);
      continue;
    }
    ++it;
  }

  for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();) {
    Voice* vox = it->get();
    vox->_bringOutYourDead();
    if (vox->_isRecursivelyDead()) {
      it = _destroyVoice(it);
      continue;
    }
    ++it;
  }

  for (auto it = m_activeSequencers.begin(); it != m_activeSequencers.end();) {
    Sequencer* seq = it->get();
    seq->_bringOutYourDead();
    if (seq->m_state == SequencerState::Dead) {
      it = _destroySequencer(it);
      continue;
    }
    ++it;
  }
}

void Engine::_on5MsInterval(IBackendVoiceAllocator& engine, double dt) {
  m_channelSet = engine.getAvailableSet();
  if (m_midiReader)
    m_midiReader->pumpReader(dt);
  for (ObjToken<Sequencer>& seq : m_activeSequencers)
    seq->advance(dt);
  for (ObjToken<Emitter>& emitter : m_activeEmitters)
    emitter->_update();
  for (ObjToken<Listener>& listener : m_activeListeners)
    listener->m_dirty = false;
}

void Engine::_onPumpCycleComplete(IBackendVoiceAllocator& engine) {
  _bringOutYourDead();

  /* Determine lowest available free vid */
  int maxVid = -1;
  for (ObjToken<Voice>& vox : m_activeVoices)
    maxVid = std::max(maxVid, vox->maxVid());
  m_nextVid = maxVid + 1;
}

AudioGroup* Engine::_addAudioGroup(const AudioGroupData& data, std::unique_ptr<AudioGroup>&& grp) {
  AudioGroup* ret = grp.get();
  m_audioGroups.emplace(std::make_pair(&data, std::move(grp)));

  /* setup SFX index for contained objects */
  for (const auto& [groupID, groupIndex] : ret->getProj().sfxGroups()) {
    const SFXGroupIndex& sfxGroup = *groupIndex;
    m_sfxLookup.reserve(m_sfxLookup.size() + sfxGroup.m_sfxEntries.size());
    for (const auto& ent : sfxGroup.m_sfxEntries)
      m_sfxLookup[ent.first] = std::make_tuple(ret, groupID, &ent.second);
  }

  return ret;
}

/** Add GameCube audio group data pointers to engine; must remain resident! */
const AudioGroup* Engine::addAudioGroup(const AudioGroupData& data) {
  removeAudioGroup(data);
  return _addAudioGroup(data, std::make_unique<AudioGroup>(data));
}

/** Remove audio group from engine */
void Engine::removeAudioGroup(const AudioGroupData& data) {
  auto search = m_audioGroups.find(&data);
  if (search == m_audioGroups.cend())
    return;
  AudioGroup* grp = search->second.get();

  /* Destroy runtime entities within group */
  for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();) {
    Voice* vox = it->get();
    if (&vox->getAudioGroup() == grp) {
      vox->_destroy();
      it = m_activeVoices.erase(it);
      continue;
    }
    ++it;
  }

  for (auto it = m_activeEmitters.begin(); it != m_activeEmitters.end();) {
    Emitter* emitter = it->get();
    if (&emitter->getAudioGroup() == grp) {
      emitter->_destroy();
      it = m_activeEmitters.erase(it);
      continue;
    }
    ++it;
  }

  for (auto it = m_activeSequencers.begin(); it != m_activeSequencers.end();) {
    Sequencer* seq = it->get();
    if (&seq->getAudioGroup() == grp) {
      seq->_destroy();
      it = m_activeSequencers.erase(it);
      continue;
    }
    ++it;
  }

  /* teardown SFX index for contained objects */
  for (const auto& pair : grp->getProj().sfxGroups()) {
    const SFXGroupIndex& sfxGroup = *pair.second;
    for (const auto& sfxEntry : sfxGroup.m_sfxEntries) {
      m_sfxLookup.erase(sfxEntry.first);
    }
  }

  m_audioGroups.erase(search);
}

/** Create new Studio within engine */
ObjToken<Studio> Engine::addStudio(bool mainOut) { return _allocateStudio(mainOut); }

/** Start soundFX playing from loaded audio groups */
ObjToken<Voice> Engine::fxStart(SFXId sfxId, float vol, float pan, ObjToken<Studio> smx) {
  auto search = m_sfxLookup.find(sfxId);
  if (search == m_sfxLookup.end())
    return {};

  AudioGroup* grp = std::get<0>(search->second);
  const SFXGroupIndex::SFXEntry* entry = std::get<2>(search->second);
  if (!grp)
    return {};

  std::list<ObjToken<Voice>>::iterator ret =
      _allocateVoice(*grp, std::get<1>(search->second), NativeSampleRate, true, false, smx);

  if (!(*ret)->loadPageObject(entry->objId, 1000.f, entry->defKey, entry->defVel, 0)) {
    _destroyVoice(ret);
    return {};
  }

  (*ret)->setVolume(vol);
  float evalPan = pan != 0.f ? pan : ((entry->panning - 64.f) / 63.f);
  evalPan = std::clamp(-1.f, evalPan, 1.f);
  (*ret)->setPan(evalPan);
  return *ret;
}

/** Start soundFX playing from explicit group data (for editor use) */
ObjToken<Voice> Engine::fxStart(const AudioGroup* group, GroupId groupId, SFXId sfxId, float vol, float pan,
                                ObjToken<Studio> smx) {
  const SFXGroupIndex* sfxIdx = group->getProj().getSFXGroupIndex(groupId);
  if (sfxIdx) {
    auto search = sfxIdx->m_sfxEntries.find(sfxId);
    if (search != sfxIdx->m_sfxEntries.cend()) {
      std::list<ObjToken<Voice>>::iterator ret = _allocateVoice(*group, groupId, NativeSampleRate, true, false, smx);

      auto& entry = search->second;
      if (!(*ret)->loadPageObject(entry.objId, 1000.f, entry.defKey, entry.defVel, 0)) {
        _destroyVoice(ret);
        return {};
      }

      (*ret)->setVolume(vol);
      float evalPan = pan != 0.f ? pan : ((entry.panning - 64.f) / 63.f);
      evalPan = std::clamp(-1.f, evalPan, 1.f);
      (*ret)->setPan(evalPan);
      return *ret;
    }
  }

  return {};
}

/** Start SoundMacro node playing directly (for editor use) */
ObjToken<Voice> Engine::macroStart(const AudioGroup* group, SoundMacroId id, uint8_t key, uint8_t vel, uint8_t mod,
                                   ObjToken<Studio> smx) {
  if (!group)
    return {};

  std::list<ObjToken<Voice>>::iterator ret = _allocateVoice(*group, {}, NativeSampleRate, true, false, smx);

  if (!(*ret)->loadMacroObject(id, 0, 1000.f, key, vel, mod)) {
    _destroyVoice(ret);
    return {};
  }

  (*ret)->setVolume(1.f);
  (*ret)->setPan(0.f);
  return *ret;
}

/** Start SoundMacro object playing directly (for editor use) */
ObjToken<Voice> Engine::macroStart(const AudioGroup* group, const SoundMacro* macro, uint8_t key, uint8_t vel,
                                   uint8_t mod, ObjToken<Studio> smx) {
  if (!group)
    return {};

  std::list<ObjToken<Voice>>::iterator ret = _allocateVoice(*group, {}, NativeSampleRate, true, false, smx);

  if (!(*ret)->loadMacroObject(macro, 0, 1000.f, key, vel, mod)) {
    _destroyVoice(ret);
    return {};
  }

  (*ret)->setVolume(1.f);
  (*ret)->setPan(0.f);
  return *ret;
}

/** Start PageObject node playing directly (for editor use) */
ObjToken<Voice> Engine::pageObjectStart(const AudioGroup* group, ObjectId id, uint8_t key, uint8_t vel, uint8_t mod,
                                        ObjToken<Studio> smx) {
  if (!group)
    return {};

  std::list<ObjToken<Voice>>::iterator ret = _allocateVoice(*group, {}, NativeSampleRate, true, false, smx);

  if (!(*ret)->loadPageObject(id, 1000.f, key, vel, mod)) {
    _destroyVoice(ret);
    return {};
  }

  return *ret;
}

/** Start soundFX playing from loaded audio groups, attach to positional emitter */
ObjToken<Emitter> Engine::addEmitter(const float* pos, const float* dir, float maxDist, float falloff, SFXId sfxId,
                                     float minVol, float maxVol, bool doppler, ObjToken<Studio> smx) {
  auto search = m_sfxLookup.find(sfxId);
  if (search == m_sfxLookup.end())
    return {};

  AudioGroup* grp = std::get<0>(search->second);
  const SFXGroupIndex::SFXEntry* entry = std::get<2>(search->second);
  if (!grp)
    return {};

  std::list<ObjToken<Voice>>::iterator vox =
      _allocateVoice(*grp, std::get<1>(search->second), NativeSampleRate, true, true, smx);

  if (!(*vox)->loadPageObject(entry->objId, 1000.f, entry->defKey, entry->defVel, 0)) {
    _destroyVoice(vox);
    return {};
  }

  auto emitIt = m_activeEmitters.emplace(m_activeEmitters.end(),
                                         MakeObj<Emitter>(*this, *grp, *vox, maxDist, minVol, falloff, doppler));
  Emitter& ret = *(*emitIt);

  ret.getVoice()->setPan(entry->panning);
  ret.setVectors(pos, dir);
  ret.setMaxVol(maxVol);

  return *emitIt;
}

/** Build listener and add to engine's listener list */
ObjToken<Listener> Engine::addListener(const float* pos, const float* dir, const float* heading, const float* up,
                                       float frontDiff, float backDiff, float soundSpeed, float volume) {
  auto listenerIt =
      m_activeListeners.emplace(m_activeListeners.end(), MakeObj<Listener>(volume, frontDiff, backDiff, soundSpeed));
  Listener& ret = *(*listenerIt);
  ret.setVectors(pos, dir, heading, up);
  return *listenerIt;
}

/** Remove listener from engine's listener list */
void Engine::removeListener(Listener* listener) {
  for (auto it = m_activeListeners.begin(); it != m_activeListeners.end(); ++it) {
    if (it->get() == listener) {
      m_activeListeners.erase(it);
      return;
    }
  }
}

/** Start song playing from loaded audio groups */
ObjToken<Sequencer> Engine::seqPlay(GroupId groupId, SongId songId, const unsigned char* arrData, bool loop,
                                    ObjToken<Studio> smx) {
  std::pair<AudioGroup*, const SongGroupIndex*> songGrp = _findSongGroup(groupId);
  if (songGrp.second) {
    std::list<ObjToken<Sequencer>>::iterator ret = _allocateSequencer(*songGrp.first, groupId, songId, smx);
    if (!*ret)
      return {};

    if (arrData)
      (*ret)->playSong(arrData, loop);
    return *ret;
  }

  std::pair<AudioGroup*, const SFXGroupIndex*> sfxGrp = _findSFXGroup(groupId);
  if (sfxGrp.second) {
    std::list<ObjToken<Sequencer>>::iterator ret = _allocateSequencer(*sfxGrp.first, groupId, songId, smx);
    if (!*ret)
      return {};
    return *ret;
  }

  return {};
}

ObjToken<Sequencer> Engine::seqPlay(const AudioGroup* group, GroupId groupId, SongId songId,
                                    const unsigned char* arrData, bool loop, ObjToken<Studio> smx) {
  const SongGroupIndex* sgIdx = group->getProj().getSongGroupIndex(groupId);
  if (sgIdx) {
    std::list<ObjToken<Sequencer>>::iterator ret = _allocateSequencer(*group, groupId, songId, smx);
    if (!*ret)
      return {};

    if (arrData)
      (*ret)->playSong(arrData, loop);
    return *ret;
  }

  const SFXGroupIndex* sfxIdx = group->getProj().getSFXGroupIndex(groupId);
  if (sfxIdx) {
    std::list<ObjToken<Sequencer>>::iterator ret = _allocateSequencer(*group, groupId, songId, smx);
    if (!*ret)
      return {};
    return *ret;
  }

  return {};
}

/** Set total volume of engine */
void Engine::setVolume(float vol) { m_masterVolume = vol; }

/** Find voice from VoiceId */
ObjToken<Voice> Engine::findVoice(int vid) {
  for (ObjToken<Voice>& vox : m_activeVoices) {
    ObjToken<Voice> ret = vox->_findVoice(vid, vox);
    if (ret)
      return ret;
  }

  for (ObjToken<Sequencer>& seq : m_activeSequencers) {
    ObjToken<Voice> ret = seq->findVoice(vid);
    if (ret)
      return ret;
  }

  return {};
}

/** Stop all voices in `kg`, stops immediately (no KeyOff) when `flag` set */
void Engine::killKeygroup(uint8_t kg, bool now) {
  for (auto it = m_activeVoices.begin(); it != m_activeVoices.end();) {
    Voice* vox = it->get();
    if (vox->m_keygroup == kg) {
      if (now) {
        it = _destroyVoice(it);
        continue;
      }
      vox->keyOff();
    }
    ++it;
  }

  for (ObjToken<Sequencer>& seq : m_activeSequencers)
    seq->killKeygroup(kg, now);
}

/** Send all voices using `macroId` the message `val` */
void Engine::sendMacroMessage(ObjectId macroId, int32_t val) {
  for (auto it = m_activeVoices.begin(); it != m_activeVoices.end(); ++it) {
    Voice* vox = it->get();
    if (vox->getObjectId() == macroId)
      vox->message(val);
  }

  for (ObjToken<Sequencer>& seq : m_activeSequencers)
    seq->sendMacroMessage(macroId, val);
}

size_t Engine::getNumTotalActiveVoices() const {
  size_t ret = 0;
  for (const auto& vox : m_activeVoices)
    ret += vox->getTotalVoices();
  return ret;
}
} // namespace amuse

#include "amuse/Engine.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Sequencer.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"
#include "amuse/AudioGroupData.hpp"
#include "amuse/AudioGroup.hpp"

namespace amuse
{

Engine::Engine(IBackendVoiceAllocator& backend)
: m_backend(backend)
{}

Voice* Engine::_allocateVoice(const AudioGroup& group, double sampleRate, bool dynamicPitch, bool emitter)
{
    auto it = m_activeVoices.emplace(m_activeVoices.end(), *this, group, m_nextVid++, emitter);
    m_activeVoices.back().m_backendVoice =
        m_backend.allocateVoice(m_activeVoices.back(), sampleRate, dynamicPitch);
    m_activeVoices.back().m_engineIt = it;
    return &m_activeVoices.back();
}

std::list<Voice>::iterator Engine::_destroyVoice(Voice* voice)
{
#ifndef NDEBUG
    assert(this == &voice->getEngine());
#endif
    voice->_destroy();
    return m_activeVoices.erase(voice->m_engineIt);
}

AudioGroup* Engine::_findGroupFromSfxId(int sfxId, const AudioGroupSampleDirectory::Entry*& entOut) const
{
    for (const auto& grp : m_audioGroups)
    {
        entOut = grp.second->getSfxEntry(sfxId);
        if (entOut)
            return grp.second.get();
    }
    return nullptr;
}

AudioGroup* Engine::_findGroupFromSongId(int songId) const
{
    for (const auto& grp : m_audioGroups)
        if (grp.second->songInGroup(songId))
            return grp.second.get();
    return nullptr;
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
bool Engine::addAudioGroup(int groupId, const AudioGroupData& data)
{
    std::unique_ptr<AudioGroup> grp = std::make_unique<AudioGroup>(groupId, data);
    if (!grp)
        return false;
    m_audioGroups.emplace(std::make_pair(groupId, std::move(grp)));
    return true;
}

/** Remove audio group from engine */
void Engine::removeAudioGroup(int groupId)
{
    for (auto it = m_activeVoices.begin() ; it != m_activeVoices.end() ;)
    {
        if (it->getAudioGroup().groupId() == groupId)
        {
            it = m_activeVoices.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeEmitters.begin() ; it != m_activeEmitters.end() ;)
    {
        if (it->getAudioGroup().groupId() == groupId)
        {
            it = m_activeEmitters.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeSequencers.begin() ; it != m_activeSequencers.end() ;)
    {
        if (it->getAudioGroup().groupId() == groupId)
        {
            it = m_activeSequencers.erase(it);
            continue;
        }
        ++it;
    }

    m_audioGroups.erase(groupId);
}

/** Start soundFX playing from loaded audio groups */
Voice* Engine::fxStart(int sfxId, float vol, float pan)
{
    const AudioGroupSampleDirectory::Entry* entry;
    AudioGroup* grp = _findGroupFromSfxId(sfxId, entry);
    if (!grp)
        return nullptr;

    Voice* ret = _allocateVoice(*grp, entry->m_sampleRate, true, false);
    ret->setVolume(vol);
    ret->setPanning(pan);
    return ret;
}

/** Start soundFX playing from loaded audio groups, attach to positional emitter */
Emitter* Engine::addEmitter(const Vector3f& pos, const Vector3f& dir, float maxDist,
                            float falloff, int sfxId, float minVol, float maxVol)
{
    const AudioGroupSampleDirectory::Entry* entry;
    AudioGroup* grp = _findGroupFromSfxId(sfxId, entry);
    if (!grp)
        return nullptr;

    Voice* vox = _allocateVoice(*grp, entry->m_sampleRate, true, true);
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
Sequencer* Engine::seqPlay(int songId, const unsigned char* arrData)
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

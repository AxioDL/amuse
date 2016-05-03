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

Voice* Engine::_allocateVoice(int groupId, double sampleRate, bool dynamicPitch)
{
    std::unique_ptr<Voice> vox = std::make_unique<Voice>(*this, groupId);
    vox->m_backendVoice = m_backend.allocateVoice(*vox, sampleRate, dynamicPitch);
    Voice* ret = vox.get();
    m_activeVoices.push_back(std::move(vox));
    return ret;
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
        if ((*it)->getGroupId() == groupId)
        {
            it = m_activeVoices.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeEmitters.begin() ; it != m_activeEmitters.end() ;)
    {
        if ((*it)->getGroupId() == groupId)
        {
            it = m_activeEmitters.erase(it);
            continue;
        }
        ++it;
    }

    for (auto it = m_activeSequencers.begin() ; it != m_activeSequencers.end() ;)
    {
        if ((*it)->getGroupId() == groupId)
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

    Voice* ret = _allocateVoice(grp->groupId(), entry->m_sampleRate, true);
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

    Voice* vox = _allocateVoice(grp->groupId(), entry->m_sampleRate, true);
    std::unique_ptr<Emitter> emtr = std::make_unique<Emitter>(*this, grp->groupId(), *vox);
    Emitter* ret = emtr.get();
    ret->setPos(pos);
    ret->setDir(dir);
    ret->setMaxDist(maxDist);
    ret->setFalloff(falloff);
    ret->setMinVol(minVol);
    ret->setMaxVol(maxVol);

    m_activeEmitters.push_back(std::move(emtr));
    return ret;
}

/** Start song playing from loaded audio groups */
Sequencer* Engine::seqPlay(int songId, const unsigned char* arrData)
{
}

}

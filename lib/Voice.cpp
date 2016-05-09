#include "amuse/Voice.hpp"
#include "amuse/Submix.hpp"
#include "amuse/IBackendVoice.hpp"

namespace amuse
{

void Voice::_destroy()
{
    Entity::_destroy();
    if (m_submix)
        m_submix->m_activeVoices.erase(this);
}

Voice::Voice(Engine& engine, const AudioGroup& group, int vid, bool emitter, Submix* smx)
: Entity(engine, group), m_vid(vid), m_emitter(emitter), m_submix(smx)
{
    if (m_submix)
        m_submix->m_activeVoices.insert(this);
}

Voice::Voice(Engine& engine, const AudioGroup& group, ObjectId oid, int vid, bool emitter, Submix* smx)
: Entity(engine, group, oid), m_vid(vid), m_emitter(emitter), m_submix(smx)
{
    if (m_submix)
        m_submix->m_activeVoices.insert(this);
}

size_t Voice::supplyAudio(size_t frames, int16_t* data)
{
}

Voice* Voice::startChildMacro(int8_t addNote, ObjectId macroId, int macroStep)
{
}

bool Voice::loadSoundMacro(ObjectId macroId, int macroStep, bool pushPc)
{
}

void Voice::keyOff()
{
}

void Voice::message(int32_t val)
{
}

void Voice::setVolume(float vol)
{
}

void Voice::setPanning(float pan)
{
}

}

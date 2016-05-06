#include "amuse/Voice.hpp"
#include "amuse/IBackendVoice.hpp"

namespace amuse
{

Voice::Voice(Engine& engine, const AudioGroup& group, int vid, bool emitter)
: Entity(engine, group), m_vid(vid), m_emitter(emitter)
{}

Voice::Voice(Engine& engine, const AudioGroup& group, ObjectId oid, int vid, bool emitter)
: Entity(engine, group, oid), m_vid(vid), m_emitter(emitter)
{}

size_t Voice::supplyAudio(size_t frames, int16_t* data)
{
}

Voice* Voice::startSiblingMacro(int8_t addNote, ObjectId macroId, int macroStep)
{
}

bool Voice::loadSoundMacro(ObjectId macroId, int macroStep)
{
}

}

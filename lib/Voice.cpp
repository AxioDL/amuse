#include "amuse/Voice.hpp"
#include "amuse/IBackendVoice.hpp"

namespace amuse
{

Voice::Voice(Engine& engine, int groupId, int vid, bool emitter)
: Entity(engine, groupId), m_vid(vid), m_emitter(emitter)
{
}

size_t Voice::supplyAudio(size_t frames, int16_t* data)
{
}

Voice* Voice::startSiblingMacro(int8_t addNote, int macroId, int macroStep)
{
}

bool Voice::loadSoundMacro(int macroId, int macroStep)
{
}

}

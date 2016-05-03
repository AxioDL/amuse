#include "amuse/Voice.hpp"
#include "amuse/IBackendVoice.hpp"

namespace amuse
{

Voice::Voice(Engine& engine, int groupId)
: Entity(engine, groupId)
{
}

size_t Voice::supplyAudio(size_t frames, int16_t* data)
{
}

}

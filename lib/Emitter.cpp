#include "amuse/Emitter.hpp"
#include "amuse/Voice.hpp"

namespace amuse
{

Emitter::Emitter(Engine& engine, const AudioGroup& group, Voice& vox)
: Entity(engine, group, vox.getObjectId()), m_vox(vox)
{
}

}

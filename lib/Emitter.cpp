#include "amuse/Emitter.hpp"

namespace amuse
{

Emitter::Emitter(Engine& engine, int groupId, Voice& vox)
: Entity(engine, groupId), m_vox(vox)
{
}

}

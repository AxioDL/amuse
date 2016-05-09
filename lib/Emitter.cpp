#include "amuse/Emitter.hpp"
#include "amuse/Voice.hpp"

namespace amuse
{

Emitter::Emitter(Engine& engine, const AudioGroup& group, Voice& vox)
: Entity(engine, group, vox.getObjectId()), m_vox(vox)
{
}

void Emitter::setPos(const Vector3f& pos)
{
}

void Emitter::setDir(const Vector3f& dir)
{
}

void Emitter::setMaxDist(float maxDist)
{
}

void Emitter::setMaxVol(float maxVol)
{
}

void Emitter::setMinVol(float minVol)
{
}

void Emitter::setFalloff(float falloff)
{
}

}

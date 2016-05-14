#include "amuse/Emitter.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Engine.hpp"

namespace amuse
{

Emitter::~Emitter() {}

Emitter::Emitter(Engine& engine, const AudioGroup& group, std::shared_ptr<Voice>&& vox)
: Entity(engine, group, vox->getObjectId()), m_vox(std::move(vox))
{
}

void Emitter::_destroy()
{
    Entity::_destroy();
    m_engine._destroyVoice(m_vox.get());
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

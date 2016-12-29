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
    m_vox->kill();
}

void Emitter::setPos(const float* pos) {}

void Emitter::setDir(const float* dir) {}

void Emitter::setMaxDist(float maxDist) {}

void Emitter::setMaxVol(float maxVol) {}

void Emitter::setMinVol(float minVol) {}

void Emitter::setFalloff(float falloff) {}
}

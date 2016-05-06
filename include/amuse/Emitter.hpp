#ifndef __AMUSE_EMITTER_HPP__
#define __AMUSE_EMITTER_HPP__

#include "Entity.hpp"

namespace amuse
{
class Voice;

using Vector3f = float[3];

class Emitter : public Entity
{
    Voice& m_vox;
public:
    Emitter(Engine& engine, const AudioGroup& group, Voice& vox);

    void setPos(const Vector3f& pos);
    void setDir(const Vector3f& dir);
    void setMaxDist(float maxDist);
    void setMaxVol(float maxVol);
    void setMinVol(float minVol);
    void setFalloff(float falloff);
};

}

#endif // __AMUSE_EMITTER_HPP__

#ifndef __AMUSE_EMITTER_HPP__
#define __AMUSE_EMITTER_HPP__

#include "Entity.hpp"
#include <memory>

namespace amuse
{
class Voice;

using Vector3f = float[3];

class Emitter : public Entity
{
    std::shared_ptr<Voice> m_vox;

    friend class Engine;
    void _destroy();
public:
    ~Emitter();
    Emitter(Engine& engine, const AudioGroup& group, std::shared_ptr<Voice>&& vox);

    void setPos(const Vector3f& pos);
    void setDir(const Vector3f& dir);
    void setMaxDist(float maxDist);
    void setMaxVol(float maxVol);
    void setMinVol(float minVol);
    void setFalloff(float falloff);

    std::shared_ptr<Voice>& getVoice() {return m_vox;}
};

}

#endif // __AMUSE_EMITTER_HPP__

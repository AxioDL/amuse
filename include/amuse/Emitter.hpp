#ifndef __AMUSE_EMITTER_HPP__
#define __AMUSE_EMITTER_HPP__

#include "Entity.hpp"
#include "Common.hpp"
#include <memory>
#include <cmath>
#include <cfloat>

namespace amuse
{
class Voice;
class Listener;

using Vector3f = float[3];

static float Dot(const Vector3f& a, const Vector3f& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

static float Length(const Vector3f& a)
{
    if (std::fabs(a[0]) <= FLT_EPSILON && std::fabs(a[1]) <= FLT_EPSILON && std::fabs(a[2]) <= FLT_EPSILON)
        return 0.f;
    return std::sqrt(Dot(a, a));
}

static float Normalize(Vector3f& out)
{
    float dist = Length(out);
    if (dist == 0.f)
        return 0.f;
    out[0] /= dist;
    out[1] /= dist;
    out[2] /= dist;
    return dist;
}

/** Voice wrapper with positional-3D level control */
class Emitter : public Entity
{
    std::shared_ptr<Voice> m_vox;
    Vector3f m_pos = {};
    Vector3f m_dir = {};
    float m_maxDist;
    float m_maxVol = 1.f;
    float m_minVol;
    float m_falloff;
    bool m_doppler;
    bool m_dirty = true;

    friend class Engine;
    void _destroy();
    float _attenuationCurve(float dist) const;
    void _update();

public:
    ~Emitter();
    Emitter(Engine& engine, const AudioGroup& group, const std::shared_ptr<Voice>& vox,
            float maxDist, float minVol, float falloff, bool doppler);

    void setVectors(const float* pos, const float* dir);
    void setMaxVol(float maxVol) { m_maxVol = clamp(0.f, maxVol, 1.f); m_dirty = true; }

    const std::shared_ptr<Voice>& getVoice() const { return m_vox; }
};
}

#endif // __AMUSE_EMITTER_HPP__

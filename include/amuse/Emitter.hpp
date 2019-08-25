#pragma once

#include <cfloat>
#include <cmath>

#include "amuse/Common.hpp"
#include "amuse/Entity.hpp"
#include "amuse/Voice.hpp"

namespace amuse {
class Listener;

using Vector3f = float[3];

inline float Dot(const Vector3f& a, const Vector3f& b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

inline float Length(const Vector3f& a) {
  if (std::fabs(a[0]) <= FLT_EPSILON && std::fabs(a[1]) <= FLT_EPSILON && std::fabs(a[2]) <= FLT_EPSILON)
    return 0.f;
  return std::sqrt(Dot(a, a));
}

inline float Normalize(Vector3f& out) {
  float dist = Length(out);
  if (dist == 0.f)
    return 0.f;
  out[0] /= dist;
  out[1] /= dist;
  out[2] /= dist;
  return dist;
}

/** Voice wrapper with positional-3D level control */
class Emitter : public Entity {
  friend class Engine;

  ObjToken<Voice> m_vox;
  Vector3f m_pos = {};
  Vector3f m_dir = {};
  float m_maxDist;
  float m_maxVol = 1.f;
  float m_minVol;
  float m_falloff;
  bool m_doppler;
  bool m_dirty = true;
  Voice::VolumeCache m_attCache;

  void _destroy();
  float _attenuationCurve(float dist) const;
  void _update();

public:
  ~Emitter() override;
  Emitter(Engine& engine, const AudioGroup& group, ObjToken<Voice> vox, float maxDist, float minVol, float falloff,
          bool doppler);

  void setVectors(const float* pos, const float* dir);
  void setMaxVol(float maxVol) {
    m_maxVol = clamp(0.f, maxVol, 1.f);
    m_dirty = true;
  }

  ObjToken<Voice> getVoice() const { return m_vox; }
};
} // namespace amuse

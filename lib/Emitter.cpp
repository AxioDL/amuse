#include "amuse/Emitter.hpp"

#include "amuse/Engine.hpp"
#include "amuse/Listener.hpp"
#include "amuse/Voice.hpp"

namespace amuse {
static constexpr Vector3f Delta(const Vector3f& a, const Vector3f& b) {
  return {a[0] - b[0], a[1] - b[1], a[2] - b[2]};
}

Emitter::~Emitter() = default;

Emitter::Emitter(Engine& engine, const AudioGroup& group, ObjToken<Voice> vox, float maxDist, float minVol,
                 float falloff, bool doppler)
: Entity(engine, group, vox->getGroupId(), vox->getObjectId())
, m_vox(vox)
, m_maxDist(maxDist)
, m_minVol(std::clamp(minVol, 0.f, 1.f))
, m_falloff(std::clamp(falloff, -1.f, 1.f))
, m_doppler(doppler) {}

void Emitter::_destroy() {
  Entity::_destroy();
  m_vox->kill();
}

float Emitter::_attenuationCurve(float dist) const {
  if (dist > m_maxDist)
    return 0.f;
  float t = dist / m_maxDist;
  if (m_falloff >= 0.f) {
    return 1.f - (m_falloff * t * t + (1.f - m_falloff) * t);
  } else {
    float omt = 1.f - t;
    return 1.f - ((1.f + m_falloff) * t - (1.f - omt * omt) * m_falloff);
  }
}

void Emitter::_update() {
  if (!m_dirty) {
    /* Ensure that all listeners are also not dirty */
    bool dirty = false;
    for (auto& listener : m_engine.m_activeListeners) {
      if (listener->m_dirty) {
        dirty = true;
        break;
      }
    }
    if (!dirty)
      return;
  }

  std::array<float, 8> coefs{};
  double avgDopplerRatio = 0.0;

  for (auto& listener : m_engine.m_activeListeners) {
    const Vector3f listenerToEmitter = Delta(m_pos, listener->m_pos);
    const float dist = Length(listenerToEmitter);
    const float panDist = Dot(listenerToEmitter, listener->m_right);
    const float frontPan = std::clamp(panDist / listener->m_frontDiff, -1.f, 1.f);
    const float backPan = std::clamp(panDist / listener->m_backDiff, -1.f, 1.f);
    const float spanDist = -Dot(listenerToEmitter, listener->m_heading);
    const float span =
        std::clamp(spanDist > 0.f ? spanDist / listener->m_backDiff : spanDist / listener->m_frontDiff, -1.f, 1.f);

    /* Calculate attenuation */
    float att = _attenuationCurve(dist);
    att = (m_maxVol - m_minVol) * att + m_minVol;
    att = m_attCache.getVolume(att, false);
    if (att > FLT_EPSILON) {
      /* Apply pan law */
      const std::array<float, 8> thisCoefs = m_vox->_panLaw(frontPan, backPan, span);

      /* Take maximum coefficient across listeners */
      for (size_t i = 0; i < coefs.size(); ++i) {
        coefs[i] = std::max(coefs[i], thisCoefs[i] * att * listener->m_volume);
      }
    }

    /* Calculate doppler */
    if (m_doppler) {
      /* Positive values indicate emitter and listener closing in */
      const Vector3f dirDelta = Delta(m_dir, listener->m_dir);
      const Vector3f posDelta = Normalize(Delta(listener->m_pos, m_pos));
      const float deltaSpeed = Dot(dirDelta, posDelta);
      if (listener->m_soundSpeed != 0.f) {
        avgDopplerRatio += 1.0 + deltaSpeed / listener->m_soundSpeed;
      } else {
        avgDopplerRatio += 1.0;
      }
    }
  }

  if (m_engine.m_activeListeners.size() != 0) {
    m_vox->setChannelCoefs(coefs);
    if (m_doppler) {
      m_vox->m_dopplerRatio = avgDopplerRatio / float(m_engine.m_activeListeners.size());
      m_vox->m_pitchDirty = true;
    }
  }

  m_dirty = false;
}

void Emitter::setVectors(const float* pos, const float* dir) {
  for (size_t i = 0; i < m_pos.size(); ++i) {
    m_pos[i] = pos[i];
    m_dir[i] = dir[i];
  }
  m_dirty = true;
}

} // namespace amuse

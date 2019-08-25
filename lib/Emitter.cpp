#include "amuse/Emitter.hpp"

#include "amuse/Engine.hpp"
#include "amuse/Listener.hpp"
#include "amuse/Voice.hpp"

namespace amuse {

static void Delta(Vector3f& out, const Vector3f& a, const Vector3f& b) {
  out[0] = a[0] - b[0];
  out[1] = a[1] - b[1];
  out[2] = a[2] - b[2];
}

Emitter::~Emitter() {}

Emitter::Emitter(Engine& engine, const AudioGroup& group, ObjToken<Voice> vox, float maxDist, float minVol,
                 float falloff, bool doppler)
: Entity(engine, group, vox->getGroupId(), vox->getObjectId())
, m_vox(vox)
, m_maxDist(maxDist)
, m_minVol(clamp(0.f, minVol, 1.f))
, m_falloff(clamp(-1.f, falloff, 1.f))
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

  float coefs[8] = {};
  double avgDopplerRatio = 0.0;

  for (auto& listener : m_engine.m_activeListeners) {
    Vector3f listenerToEmitter;
    Delta(listenerToEmitter, m_pos, listener->m_pos);
    float dist = Length(listenerToEmitter);
    float panDist = Dot(listenerToEmitter, listener->m_right);
    float frontPan = clamp(-1.f, panDist / listener->m_frontDiff, 1.f);
    float backPan = clamp(-1.f, panDist / listener->m_backDiff, 1.f);
    float spanDist = -Dot(listenerToEmitter, listener->m_heading);
    float span = clamp(-1.f, spanDist > 0.f ? spanDist / listener->m_backDiff : spanDist / listener->m_frontDiff, 1.f);

    /* Calculate attenuation */
    float att = _attenuationCurve(dist);
    att = (m_maxVol - m_minVol) * att + m_minVol;
    att = m_attCache.getVolume(att, false);
    if (att > FLT_EPSILON) {
      /* Apply pan law */
      float thisCoefs[8] = {};
      m_vox->_panLaw(thisCoefs, frontPan, backPan, span);

      /* Take maximum coefficient across listeners */
      for (int i = 0; i < 8; ++i)
        coefs[i] = std::max(coefs[i], thisCoefs[i] * att * listener->m_volume);
    }

    /* Calculate doppler */
    if (m_doppler) {
      /* Positive values indicate emitter and listener closing in */
      Vector3f dirDelta;
      Delta(dirDelta, m_dir, listener->m_dir);
      Vector3f posDelta;
      Delta(posDelta, listener->m_pos, m_pos);
      Normalize(posDelta);
      float deltaSpeed = Dot(dirDelta, posDelta);
      if (listener->m_soundSpeed != 0.f)
        avgDopplerRatio += 1.0 + deltaSpeed / listener->m_soundSpeed;
      else
        avgDopplerRatio += 1.0;
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
  for (int i = 0; i < 3; ++i) {
    m_pos[i] = pos[i];
    m_dir[i] = dir[i];
  }
  m_dirty = true;
}

} // namespace amuse

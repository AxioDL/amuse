#include "amuse/Emitter.hpp"
#include "amuse/Listener.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Engine.hpp"

namespace amuse
{

static void Delta(Vector3f& out, const Vector3f& a, const Vector3f& b)
{
    out[0] = a[0] - b[0];
    out[1] = a[1] - b[1];
    out[2] = a[2] - b[2];
}

Emitter::~Emitter() {}

Emitter::Emitter(Engine& engine, const AudioGroup& group, std::shared_ptr<Voice>&& vox,
                 float maxDist, float minVol, float falloff, bool doppler)
: Entity(engine, group, vox->getObjectId()), m_vox(std::move(vox)), m_maxDist(maxDist),
  m_minVol(clamp(0.f, minVol, 1.f)), m_falloff(clamp(-1.f, falloff, 1.f)), m_doppler(doppler)
{
}

void Emitter::_destroy()
{
    Entity::_destroy();
    m_vox->kill();
}

float Emitter::_attenuationCurve(float dist) const
{
    if (dist > m_maxDist)
        return 0.f;
    float t = dist / m_maxDist;
    if (m_falloff < 0.f)
    {
        float tmp = t * 10.f + 1.f;
        tmp = 1.f / (tmp * tmp);
        return (1.f + m_falloff) * (-t + 1.f) + -m_falloff * tmp;
    }
    else if (m_falloff > 0.f)
    {
        float tmp = (t - 1.f) * 10.f - 1.f;
        tmp = -1.f / (tmp * tmp) + 1.f;
        return (1.f - m_falloff) * (-t + 1.f) + m_falloff * tmp;
    }
    else
    {
        return -t + 1.f;
    }
}

void Emitter::_update()
{
    float coefs[8] = {};
    double avgDopplerRatio = 0.0;

    for (auto& listener : m_engine.m_activeListeners)
    {
        Vector3f listenerToEmitter;
        Delta(listenerToEmitter, m_pos, listener->m_pos);
        float dist = Length(listenerToEmitter);
        float panDist = Dot(listenerToEmitter, listener->m_right);
        float frontPan = clamp(-1.f, panDist / listener->m_frontDiff, 1.f);
        float backPan = clamp(-1.f, panDist / listener->m_backDiff, 1.f);
        float spanDist = -Dot(listenerToEmitter, listener->m_heading);
        float span = clamp(-1.f, spanDist > 0.f ? spanDist / listener->m_backDiff :
                                 spanDist / listener->m_frontDiff, 1.f);

        /* Calculate attenuation */
        float att = _attenuationCurve(dist);
        att = (1.f - att) * m_minVol + att * m_maxVol;

        /* Apply pan law */
        float thisCoefs[8] = {};
        m_vox->_panLaw(thisCoefs, frontPan, backPan, span);

        /* Take maximum coefficient across listeners */
        for (int i=0 ; i<8 ; ++i)
            coefs[i] = std::max(coefs[i], thisCoefs[i] * att * listener->m_volume);

        /* Calculate doppler */
        if (m_doppler)
        {
            /* Positive values indicate emitter and listener closing in */
            Vector3f dirDelta;
            Delta(dirDelta, listener->m_dir, m_dir);
            float sign = -Dot(listener->m_dir, m_dir);
            if (listener->m_soundSpeed != 0.f)
                avgDopplerRatio += 1.0 + std::copysign(Length(dirDelta), sign) / listener->m_soundSpeed;
            else
                avgDopplerRatio += 1.0;
        }
    }

    if (m_engine.m_activeListeners.size() != 0)
    {
        m_vox->setChannelCoefs(coefs);
        if (m_doppler)
            m_vox->m_dopplerRatio = avgDopplerRatio / float(m_engine.m_activeListeners.size());
    }
}

void Emitter::setVectors(const float* pos, const float* dir)
{
    for (int i=0 ; i<3 ; ++i)
    {
        m_pos[i] = pos[i];
        m_dir[i] = dir[i];
    }
}

}

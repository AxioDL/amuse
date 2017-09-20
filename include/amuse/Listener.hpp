#ifndef __AMUSE_LISTENER_HPP__
#define __AMUSE_LISTENER_HPP__

#include "amuse/Emitter.hpp"

namespace amuse
{
class Listener
{
    friend class Emitter;
    friend class Engine;
    Vector3f m_pos = {};
    Vector3f m_dir = {};
    Vector3f m_heading = {};
    Vector3f m_up = {};
    Vector3f m_right = {};
    float m_volume;
    float m_frontDiff;
    float m_backDiff;
    float m_soundSpeed;
    bool m_dirty = true;
public:
    Listener(float volume, float frontDiff, float backDiff, float soundSpeed)
    : m_volume(clamp(0.f, volume, 1.f)), m_frontDiff(frontDiff), m_backDiff(backDiff), m_soundSpeed(soundSpeed) {}
    void setVectors(const float* pos, const float* dir, const float* heading, const float* up);
    void setVolume(float vol) { m_volume = clamp(0.f, vol, 1.f); m_dirty = true; }
};
}

#endif // __AMUSE_LISTENER_HPP__

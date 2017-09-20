#include "amuse/Listener.hpp"

namespace amuse
{

static void Cross(Vector3f& out, const Vector3f& a, const Vector3f& b)
{
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}

void Listener::setVectors(const float* pos, const float* dir, const float* heading, const float* up)
{
    for (int i=0 ; i<3 ; ++i)
    {
        m_pos[i] = pos[i];
        m_dir[i] = dir[i];
        m_heading[i] = heading[i];
        m_up[i] = up[i];
    }

    Normalize(m_heading);
    Normalize(m_up);
    Cross(m_right, m_heading, m_up);
    Normalize(m_right);

    m_dirty = true;
}

}

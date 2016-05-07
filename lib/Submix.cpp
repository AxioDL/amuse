#include "amuse/Submix.hpp"

namespace amuse
{

void Submix::_destroy()
{
    m_destroyed = true;
    if (m_submix)
        m_submix->m_activeSubmixes.erase(this);
}

Submix::Submix(Engine& engine, Submix* smx)
: m_root(engine), m_submix(smx)
{
    if (m_submix)
        m_submix->m_activeSubmixes.insert(this);
}

}

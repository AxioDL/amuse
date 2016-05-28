#include "amuse/AudioGroupData.hpp"

namespace amuse
{

IntrusiveAudioGroupData::~IntrusiveAudioGroupData()
{
    if (m_owns)
    {
        delete m_pool;
        delete m_proj;
        delete m_sdir;
        delete m_samp;
    }
}

IntrusiveAudioGroupData::IntrusiveAudioGroupData(IntrusiveAudioGroupData&& other)
: AudioGroupData(other.m_proj, other.m_pool, other.m_sdir, other.m_samp, other.m_fmt)
{
    m_owns = other.m_owns;
    other.m_owns = false;
}

IntrusiveAudioGroupData& IntrusiveAudioGroupData::operator=(IntrusiveAudioGroupData&& other)
{
    if (m_owns)
    {
        delete m_pool;
        delete m_proj;
        delete m_sdir;
        delete m_samp;
    }

    m_owns = other.m_owns;
    other.m_owns = false;

    m_proj = other.m_proj;
    m_pool = other.m_pool;
    m_sdir = other.m_sdir;
    m_samp = other.m_samp;

    return *this;
}

}

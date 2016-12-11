#include "amuse/AudioGroupData.hpp"

namespace amuse
{

IntrusiveAudioGroupData::~IntrusiveAudioGroupData()
{
    if (m_owns)
    {
        delete[] m_pool;
        delete[] m_proj;
        delete[] m_sdir;
        delete[] m_samp;
    }
}

IntrusiveAudioGroupData::IntrusiveAudioGroupData(IntrusiveAudioGroupData&& other) noexcept
: AudioGroupData(other.m_proj, other.m_projSz, other.m_pool, other.m_poolSz, other.m_sdir, other.m_sdirSz, other.m_samp,
                 other.m_sampSz, other.m_fmt, other.m_absOffs)
{
    m_owns = other.m_owns;
    other.m_owns = false;
}

IntrusiveAudioGroupData& IntrusiveAudioGroupData::operator=(IntrusiveAudioGroupData&& other) noexcept
{
    if (m_owns)
    {
        delete[] m_pool;
        delete[] m_proj;
        delete[] m_sdir;
        delete[] m_samp;
    }

    m_owns = other.m_owns;
    other.m_owns = false;

    m_proj = other.m_proj;
    m_pool = other.m_pool;
    m_sdir = other.m_sdir;
    m_samp = other.m_samp;
    m_fmt = other.m_fmt;
    m_absOffs = other.m_absOffs;

    return *this;
}
}

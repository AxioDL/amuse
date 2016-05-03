#include "amuse/AudioGroupData.hpp"

namespace amuse
{

IntrusiveAudioGroupData::~IntrusiveAudioGroupData()
{
    delete m_pool;
    delete m_proj;
    delete m_sdir;
    delete m_samp;
}

}

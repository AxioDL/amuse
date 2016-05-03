#include "amuse/AudioGroupSampleDirectory.hpp"
#include "amuse/Common.hpp"

namespace amuse
{

void AudioGroupSampleDirectory::Entry::swapBig()
{
    m_sfxId = SBig(m_sfxId);
    m_sampleOff = SBig(m_sampleOff);
    m_unk = SBig(m_unk);
    m_sampleRate = SBig(m_sampleRate);
    m_numSamples = SBig(m_numSamples);
    m_loopStartSample = SBig(m_loopStartSample);
    m_loopLengthSamples = SBig(m_loopLengthSamples);
    m_adpcmParmOffset = SBig(m_adpcmParmOffset);
}

void AudioGroupSampleDirectory::ADPCMParms::swapBig()
{
    m_bytesPerFrame = SBig(m_bytesPerFrame);
    m_hist1 = SBig(m_hist1);
    m_hist2 = SBig(m_hist2);
    for (int i=0 ; i<16 ; ++i)
        m_coefs[i] = SBig(m_coefs[i]);
}

}

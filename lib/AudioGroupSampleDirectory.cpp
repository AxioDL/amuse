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
    for (int i=0 ; i<8 ; ++i)
    {
        m_coefs[i][0] = SBig(m_coefs[i][0]);
        m_coefs[i][1] = SBig(m_coefs[i][1]);
    }
}

AudioGroupSampleDirectory::AudioGroupSampleDirectory(const unsigned char* data)
{
    const unsigned char* cur = data;
    while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
    {
        const AudioGroupSampleDirectory::Entry* ent =
            reinterpret_cast<const AudioGroupSampleDirectory::Entry*>(cur);

        std::pair<Entry, ADPCMParms>& store = m_entries[SBig(ent->m_sfxId)];
        store.first = *ent;
        store.first.swapBig();

        if (store.first.m_adpcmParmOffset)
        {
            const AudioGroupSampleDirectory::ADPCMParms* adpcm =
                reinterpret_cast<const AudioGroupSampleDirectory::ADPCMParms*>(data +
                    store.first.m_adpcmParmOffset);
            store.second = *adpcm;
            store.second.swapBig();
        }

        cur += 32;
    }
}

}

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
    m_hist2 = SBig(m_hist2);
    m_hist1 = SBig(m_hist1);
    for (int i=0 ; i<8 ; ++i)
    {
        m_coefs[i][0] = SBig(m_coefs[i][0]);
        m_coefs[i][1] = SBig(m_coefs[i][1]);
    }
}

AudioGroupSampleDirectory::AudioGroupSampleDirectory(const unsigned char* data, GCNDataTag)
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

struct MusyX1SdirEntry
{
    uint16_t m_sfxId;
    uint32_t m_sampleOff;
    uint32_t m_pitchSampleRate;
    uint32_t m_numSamples;
    uint32_t m_loopStartSample;
    uint32_t m_loopLengthSamples;

    void swapBig()
    {
        m_sfxId = SBig(m_sfxId);
        m_sampleOff = SBig(m_sampleOff);
        m_pitchSampleRate = SBig(m_pitchSampleRate);
        m_numSamples = SBig(m_numSamples);
        m_loopStartSample = SBig(m_loopStartSample);
        m_loopLengthSamples = SBig(m_loopLengthSamples);
    }

    void setIntoMusyX2(AudioGroupSampleDirectory::Entry& ent) const
    {
        ent.m_sfxId = m_sfxId;
        ent.m_sampleOff = m_sampleOff;
        ent.m_unk = 0;
        ent.m_pitch = m_pitchSampleRate >> 24;
        ent.m_sampleRate = m_pitchSampleRate & 0xffff;
        ent.m_numSamples = m_numSamples;
        ent.m_loopStartSample = m_loopStartSample;
        ent.m_loopLengthSamples = m_loopLengthSamples;
        ent.m_adpcmParmOffset = 0;
    }
};

AudioGroupSampleDirectory::AudioGroupSampleDirectory(const unsigned char* data, N64DataTag)
{
    const unsigned char* cur = data;
    while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
    {
        MusyX1SdirEntry ent = *reinterpret_cast<const MusyX1SdirEntry*>(cur);
        ent.swapBig();

        std::pair<Entry, ADPCMParms>& store = m_entries[ent.m_sfxId];
        ent.setIntoMusyX2(store.first);

        cur += 24;
    }
}

AudioGroupSampleDirectory::AudioGroupSampleDirectory(const unsigned char* data, PCDataTag)
{
    const unsigned char* cur = data;
    while (*reinterpret_cast<const uint32_t*>(cur) != 0xffffffff)
    {
        const MusyX1SdirEntry* ent = reinterpret_cast<const MusyX1SdirEntry*>(cur);

        std::pair<Entry, ADPCMParms>& store = m_entries[ent->m_sfxId];
        ent->setIntoMusyX2(store.first);

        cur += 24;
    }
}

}

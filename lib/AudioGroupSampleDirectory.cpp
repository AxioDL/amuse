#include "amuse/AudioGroupSampleDirectory.hpp"
#include "amuse/Common.hpp"
#include "amuse/AudioGroupData.hpp"
#include <cstring>
#include <athena/MemoryReader.hpp>

namespace amuse
{

static bool AtEnd32(athena::io::IStreamReader& r)
{
    uint32_t v = r.readUint32Big();
    r.seek(-4, athena::Current);
    return v == 0xffffffff;
}

void AudioGroupSampleDirectory::ADPCMParms::swapBigDSP()
{
    dsp.m_bytesPerFrame = SBig(dsp.m_bytesPerFrame);
    dsp.m_hist2 = SBig(dsp.m_hist2);
    dsp.m_hist1 = SBig(dsp.m_hist1);
    for (int i = 0; i < 8; ++i)
    {
        dsp.m_coefs[i][0] = SBig(dsp.m_coefs[i][0]);
        dsp.m_coefs[i][1] = SBig(dsp.m_coefs[i][1]);
    }
}

void AudioGroupSampleDirectory::ADPCMParms::swapBigVADPCM()
{
    int16_t* allCoefs = reinterpret_cast<int16_t*>(vadpcm.m_coefs[0][0]);
    for (int i = 0; i < 128; ++i)
        allCoefs[i] = SBig(allCoefs[i]);
}

AudioGroupSampleDirectory::AudioGroupSampleDirectory(athena::io::IStreamReader& r, GCNDataTag)
{
    while (!AtEnd32(r))
    {
        EntryDNA<athena::Big> ent;
        ent.read(r);
        std::pair<Entry, ADPCMParms>& store = m_entries[ent.m_sfxId];
        store.first = ent;
    }

    for (auto& p : m_entries)
    {
        if (p.second.first.m_adpcmParmOffset)
        {
            r.seek(p.second.first.m_adpcmParmOffset, athena::Begin);
            r.readUBytesToBuf(&p.second.second, sizeof(ADPCMParms::DSPParms));
            p.second.second.swapBigDSP();
        }
    }
}

AudioGroupSampleDirectory::AudioGroupSampleDirectory(athena::io::IStreamReader& r, const unsigned char* sampData,
                                                     bool absOffs, N64DataTag)
{
    if (absOffs)
    {
        while (!AtEnd32(r))
        {
            MusyX1AbsSdirEntry<athena::Big> ent;
            ent.read(r);
            std::pair<Entry, ADPCMParms>& store = m_entries[ent.m_sfxId];
            store.first = ent;
        }
    }
    else
    {
        while (!AtEnd32(r))
        {
            MusyX1SdirEntry<athena::Big> ent;
            ent.read(r);
            std::pair<Entry, ADPCMParms>& store = m_entries[ent.m_sfxId];
            store.first = ent;
        }
    }

    for (auto& p : m_entries)
    {
        memcpy(&p.second.second, sampData + p.second.first.m_sampleOff, sizeof(ADPCMParms::VADPCMParms));
        p.second.second.swapBigVADPCM();
    }
}

AudioGroupSampleDirectory::AudioGroupSampleDirectory(athena::io::IStreamReader& r, bool absOffs, PCDataTag)
{
    if (absOffs)
    {
        while (!AtEnd32(r))
        {
            MusyX1AbsSdirEntry<athena::Little> ent;
            ent.read(r);
            std::pair<Entry, ADPCMParms>& store = m_entries[ent.m_sfxId];
            store.first = ent;
        }
    }
    else
    {
        while (!AtEnd32(r))
        {
            MusyX1SdirEntry<athena::Little> ent;
            ent.read(r);
            std::pair<Entry, ADPCMParms>& store = m_entries[ent.m_sfxId];
            store.first = ent;
        }
    }
}

AudioGroupSampleDirectory AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(const AudioGroupData& data)
{
    athena::io::MemoryReader r(data.getSdir(), data.getSdirSize());
    switch (data.getDataFormat())
    {
    case DataFormat::GCN:
    default:
        return AudioGroupSampleDirectory(r, GCNDataTag{});
    case DataFormat::N64:
        return AudioGroupSampleDirectory(r, data.getSamp(), data.getAbsoluteProjOffsets(), N64DataTag{});
    case DataFormat::PC:
        return AudioGroupSampleDirectory(r, data.getAbsoluteProjOffsets(), PCDataTag{});
    }
}
}

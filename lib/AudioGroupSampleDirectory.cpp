#include "amuse/AudioGroupSampleDirectory.hpp"
#include "amuse/Common.hpp"
#include "amuse/AudioGroupData.hpp"
#include "amuse/DSPCodec.hpp"
#include "amuse/N64MusyXCodec.hpp"
#include "amuse/DirectoryEnumerator.hpp"
#include "athena/MemoryReader.hpp"
#include "athena/FileWriter.hpp"
#include "athena/FileReader.hpp"
#include <cstring>

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
        m_entries[ent.m_sfxId] = ent;
    }

    for (auto& p : m_entries)
    {
        if (p.second.m_adpcmParmOffset)
        {
            r.seek(p.second.m_adpcmParmOffset, athena::Begin);
            r.readUBytesToBuf(&p.second.m_ADPCMParms, sizeof(ADPCMParms::DSPParms));
            p.second.m_ADPCMParms.swapBigDSP();
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
            m_entries[ent.m_sfxId] = ent;
        }
    }
    else
    {
        while (!AtEnd32(r))
        {
            MusyX1SdirEntry<athena::Big> ent;
            ent.read(r);
            m_entries[ent.m_sfxId] = ent;
        }
    }

    for (auto& p : m_entries)
    {
        memcpy(&p.second.m_ADPCMParms, sampData + p.second.m_sampleOff, sizeof(ADPCMParms::VADPCMParms));
        p.second.m_ADPCMParms.swapBigVADPCM();
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
            Entry& store = m_entries[ent.m_sfxId];
            store = ent;
            store.m_numSamples |= atUint32(SampleFormat::PCM_PC) << 24;
        }
    }
    else
    {
        while (!AtEnd32(r))
        {
            MusyX1SdirEntry<athena::Little> ent;
            ent.read(r);
            Entry& store = m_entries[ent.m_sfxId];
            store = ent;
            store.m_numSamples |= atUint32(SampleFormat::PCM_PC) << 24;
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

static uint32_t DSPSampleToNibble(uint32_t sample)
{
    uint32_t ret = sample / 14 * 16;
    if (sample % 14)
        ret += (sample % 14) + 2;
    return ret;
}

static uint32_t DSPNibbleToSample(uint32_t nibble)
{
    uint32_t ret = nibble / 16 * 14;
    if (nibble % 16)
        ret += (nibble % 16) - 2;
    return ret;
}

void AudioGroupSampleDirectory::Entry::loadLooseData(SystemStringView basePath)
{
    SystemString wavPath = SystemString(basePath) + _S(".wav");
    SystemString dspPath = SystemString(basePath) + _S(".dsp");
    Sstat wavStat, dspStat;
    bool wavValid = !Stat(wavPath.c_str(), &wavStat) && S_ISREG(wavStat.st_mode);
    bool dspValid = !Stat(dspPath.c_str(), &dspStat) && S_ISREG(dspStat.st_mode);

    if (wavValid && dspValid)
    {
        if (wavStat.st_mtime > dspStat.st_mtime)
            dspValid = false;
        else
            wavValid = false;
    }

    if (dspValid && (!m_looseData || dspStat.st_mtime > m_looseModTime))
    {
        athena::io::FileReader r(dspPath);
        if (!r.hasError())
        {
            DSPADPCMHeader header;
            header.read(r);
            m_pitch = header.m_pitch;
            m_sampleRate = atUint16(header.x8_sample_rate);
            m_numSamples = header.x0_num_samples;
            if (header.xc_loop_flag)
            {
                m_loopStartSample = DSPNibbleToSample(header.x10_loop_start_nibble);
                m_loopLengthSamples = DSPNibbleToSample(header.x14_loop_end_nibble) - m_loopStartSample;
            }
            m_ADPCMParms.dsp.m_ps = uint8_t(header.x3e_ps);
            m_ADPCMParms.dsp.m_lps = uint8_t(header.x44_loop_ps);
            m_ADPCMParms.dsp.m_hist1 = header.x40_hist1;
            m_ADPCMParms.dsp.m_hist2 = header.x42_hist2;
            for (int i = 0; i < 8; ++i)
                for (int j = 0; j < 2; ++j)
                    m_ADPCMParms.dsp.m_coefs[i][j] = header.x1c_coef[i][j];

            uint32_t dataLen = (header.x4_num_nibbles + 1) / 2;
            m_looseData.reset(new uint8_t[dataLen]);
            r.readUBytesToBuf(m_looseData.get(), dataLen);

            m_looseModTime = dspStat.st_mtime;
            return;
        }
    }

    if (wavValid && (!m_looseData || wavStat.st_mtime > m_looseModTime))
    {
        athena::io::FileReader r(wavPath);
        if (!r.hasError())
        {
            atUint32 riffMagic = r.readUint32Little();
            if (riffMagic != SBIG('RIFF'))
                return;
            atUint32 wavChuckSize = r.readUint32Little();
            atUint32 wavMagic = r.readUint32Little();
            if (wavMagic != SBIG('WAVE'))
                return;

            while (r.position() < wavChuckSize + 8)
            {
                atUint32 chunkMagic = r.readUint32Little();
                atUint32 chunkSize = r.readUint32Little();
                atUint64 startPos = r.position();
                if (chunkMagic == SBIG('fmt '))
                {
                    WAVFormatChunk fmt;
                    fmt.read(r);
                    m_sampleRate = atUint16(fmt.sampleRate);
                }
                else if (chunkMagic == SBIG('smpl'))
                {
                    WAVSampleChunk smpl;
                    smpl.read(r);
                    m_pitch = atUint8(smpl.midiNote);

                    if (smpl.numSampleLoops)
                    {
                        WAVSampleLoop loop;
                        loop.read(r);
                        m_loopStartSample = loop.start;
                        m_loopLengthSamples = loop.end - loop.start + 1;
                    }
                }
                else if (chunkMagic == SBIG('data'))
                {
                    m_numSamples = ((chunkSize / 2) & 0xffffff) | (atUint32(SampleFormat::PCM_PC) << 24);
                    m_looseData.reset(new uint8_t[chunkSize]);
                    r.readUBytesToBuf(m_looseData.get(), chunkSize);
                }
                r.seek(startPos + chunkSize, athena::Begin);
            }

            m_looseModTime = wavStat.st_mtime;
            return;
        }
    }
}

AudioGroupSampleDirectory AudioGroupSampleDirectory::CreateAudioGroupSampleDirectory(SystemStringView groupPath)
{
    AudioGroupSampleDirectory ret;

    DirectoryEnumerator de(groupPath, DirectoryEnumerator::Mode::FilesSorted);
    for (const DirectoryEnumerator::Entry& ent : de)
    {
        if (ent.m_name.size() < 4)
            continue;
        SystemString baseName;
        if (!CompareCaseInsensitive(ent.m_name.data() + ent.m_name.size() - 4, _S(".dsp")) ||
            !CompareCaseInsensitive(ent.m_name.data() + ent.m_name.size() - 4, _S(".wav")))
            baseName = SystemString(ent.m_name.begin(), ent.m_name.begin() + ent.m_name.size() - 4);
        else
            continue;

        ObjectId sampleId = SampleId::CurNameDB->generateId(NameDB::Type::Sample);
#ifdef _WIN32
        SampleId::CurNameDB->registerPair(athena::utility::wideToUtf8(baseName), sampleId);
#else
        SampleId::CurNameDB->registerPair(baseName, sampleId);
#endif

        Entry& entry = ret.m_entries[sampleId];
        SystemString basePath = SystemString(ent.m_path.begin(), ent.m_path.begin() + ent.m_path.size() - 4);
        entry.loadLooseData(basePath);
    }

    return ret;
}

void AudioGroupSampleDirectory::_extractWAV(SampleId id, const Entry& ent,
                                            amuse::SystemStringView destDir, const unsigned char* samp)
{
    amuse::SystemString path(destDir);
    path += _S("/");
#ifdef _WIN32
    path += athena::utility::utf8ToWide(SampleId::CurNameDB->resolveNameFromId(id));
#else
    path += SampleId::CurNameDB->resolveNameFromId(id);
#endif
    path += _S(".wav");
    athena::io::FileWriter w(path);

    SampleFormat fmt = SampleFormat(ent.m_numSamples >> 24);
    uint32_t numSamples = ent.m_numSamples & 0xffffff;
    if (ent.m_loopLengthSamples)
    {
        WAVHeaderLoop header;
        header.fmtChunk.sampleRate = ent.m_sampleRate;
        header.fmtChunk.byteRate = ent.m_sampleRate * 2u;
        header.smplChunk.smplPeriod = 1000000000u / ent.m_sampleRate;
        header.smplChunk.midiNote = ent.m_pitch;
        header.smplChunk.numSampleLoops = 1;
        header.sampleLoop.start = ent.m_loopStartSample;
        header.sampleLoop.end = ent.m_loopStartSample + ent.m_loopLengthSamples - 1;
        header.dataChunkSize = numSamples * 2u;
        header.wavChuckSize = 36 + 68 + header.dataChunkSize;
        header.write(w);
    }
    else
    {
        WAVHeader header;
        header.fmtChunk.sampleRate = ent.m_sampleRate;
        header.fmtChunk.byteRate = ent.m_sampleRate * 2u;
        header.smplChunk.smplPeriod = 1000000000u / ent.m_sampleRate;
        header.smplChunk.midiNote = ent.m_pitch;
        header.dataChunkSize = numSamples * 2u;
        header.wavChuckSize = 36 + 44 + header.dataChunkSize;
        header.write(w);
    }

    atUint64 dataLen;
    if (fmt == SampleFormat::DSP || fmt == SampleFormat::DSP_DRUM)
    {
        uint32_t remSamples = numSamples;
        uint32_t numFrames = (remSamples + 13) / 14;
        const unsigned char* cur = samp + ent.m_sampleOff;
        int16_t prev1 = ent.m_ADPCMParms.dsp.m_hist1;
        int16_t prev2 = ent.m_ADPCMParms.dsp.m_hist2;
        for (uint32_t i = 0; i < numFrames; ++i)
        {
            int16_t decomp[14] = {};
            unsigned thisSamples = std::min(remSamples, 14u);
            DSPDecompressFrame(decomp, cur, ent.m_ADPCMParms.dsp.m_coefs, &prev1, &prev2, thisSamples);
            remSamples -= thisSamples;
            cur += 8;
            w.writeBytes(decomp, thisSamples * 2);
        }

        dataLen = (DSPSampleToNibble(numSamples) + 1) / 2;
    }
    else if (fmt == SampleFormat::N64)
    {
        uint32_t remSamples = numSamples;
        uint32_t numFrames = (remSamples + 31) / 32;
        const unsigned char* cur = samp + ent.m_sampleOff + sizeof(ADPCMParms::VADPCMParms);
        for (uint32_t i = 0; i < numFrames; ++i)
        {
            int16_t decomp[32] = {};
            unsigned thisSamples = std::min(remSamples, 32u);
            N64MusyXDecompressFrame(decomp, cur, ent.m_ADPCMParms.vadpcm.m_coefs, thisSamples);
            remSamples -= thisSamples;
            cur += 16;
            w.writeBytes(decomp, thisSamples * 2);
        }

        dataLen = sizeof(ADPCMParms::VADPCMParms) + (numSamples + 31) / 32 * 16;
    }
    else if (fmt == SampleFormat::PCM)
    {
        dataLen = numSamples * 2;
        const int16_t* cur = reinterpret_cast<const int16_t*>(samp + ent.m_sampleOff);
        for (int i = 0; i < numSamples; ++i)
            w.writeInt16Big(cur[i]);
    }
    else // PCM_PC
    {
        dataLen = numSamples * 2;
        w.writeBytes(samp + ent.m_sampleOff, dataLen);
    }

    std::unique_ptr<uint8_t[]>& ld = const_cast<std::unique_ptr<uint8_t[]>&>(ent.m_looseData);
    if (!ld)
    {
        Sstat theStat;
        Stat(path.c_str(), &theStat);

        const_cast<time_t&>(ent.m_looseModTime) = theStat.st_mtime;
        ld.reset(new uint8_t[dataLen]);
        memcpy(ld.get(), samp + ent.m_sampleOff, dataLen);
    }
}

void AudioGroupSampleDirectory::extractWAV(SampleId id, amuse::SystemStringView destDir, const unsigned char* samp) const
{
    auto search = m_entries.find(id);
    if (search == m_entries.cend())
        return;
    _extractWAV(id, search->second, destDir, samp);
}

void AudioGroupSampleDirectory::extractAllWAV(amuse::SystemStringView destDir, const unsigned char* samp) const
{
    for (const auto& ent : m_entries)
        _extractWAV(ent.first, ent.second, destDir, samp);
}

void AudioGroupSampleDirectory::_extractCompressed(SampleId id, const Entry& ent,
                                                   amuse::SystemStringView destDir, const unsigned char* samp)
{
    SampleFormat fmt = SampleFormat(ent.m_numSamples >> 24);
    if (fmt == SampleFormat::PCM || fmt == SampleFormat::PCM_PC)
    {
        _extractWAV(id, ent, destDir, samp);
        return;
    }

    amuse::SystemString path(destDir);
    path += _S("/");
#ifdef _WIN32
    path += athena::utility::utf8ToWide(SampleId::CurNameDB->resolveNameFromId(id));
#else
    path += SampleId::CurNameDB->resolveNameFromId(id);
#endif

    uint32_t numSamples = ent.m_numSamples & 0xffffff;
    atUint64 dataLen;
    if (fmt == SampleFormat::DSP || fmt == SampleFormat::DSP_DRUM)
    {
        DSPADPCMHeader header;
        header.x0_num_samples = numSamples;
        header.x4_num_nibbles = DSPSampleToNibble(numSamples);
        header.x8_sample_rate = ent.m_sampleRate;
        header.xc_loop_flag = atUint16(ent.m_loopLengthSamples != 0);
        if (header.xc_loop_flag)
        {
            header.x10_loop_start_nibble = DSPSampleToNibble(ent.m_loopStartSample);
            header.x14_loop_end_nibble = DSPSampleToNibble(ent.m_loopStartSample + ent.m_loopLengthSamples);
        }
        for (int i = 0; i < 8; ++i)
            for (int j = 0; j < 2; ++j)
                header.x1c_coef[i][j] = ent.m_ADPCMParms.dsp.m_coefs[i][j];
        header.x3e_ps = ent.m_ADPCMParms.dsp.m_ps;
        header.x40_hist1 = ent.m_ADPCMParms.dsp.m_hist1;
        header.x42_hist2 = ent.m_ADPCMParms.dsp.m_hist2;
        header.x44_loop_ps = ent.m_ADPCMParms.dsp.m_lps;
        header.m_pitch = ent.m_pitch;

        path += _S(".dsp");
        athena::io::FileWriter w(path);
        header.write(w);
        dataLen = (header.x4_num_nibbles + 1) / 2;
        w.writeUBytes(samp + ent.m_sampleOff, dataLen);
    }
    else if (fmt == SampleFormat::N64)
    {
        path += _S(".vadpcm");
        athena::io::FileWriter w(path);
        dataLen = sizeof(ADPCMParms::VADPCMParms) + (numSamples + 31) / 32 * 16;
        w.writeUBytes(samp + ent.m_sampleOff, dataLen);
    }
    else
    {
        return;
    }

    std::unique_ptr<uint8_t[]>& ld = const_cast<std::unique_ptr<uint8_t[]>&>(ent.m_looseData);
    if (!ld)
    {
        Sstat theStat;
        Stat(path.c_str(), &theStat);

        const_cast<time_t&>(ent.m_looseModTime) = theStat.st_mtime;
        ld.reset(new uint8_t[dataLen]);
        memcpy(ld.get(), samp + ent.m_sampleOff, dataLen);
    }
}

void AudioGroupSampleDirectory::extractCompressed(SampleId id, amuse::SystemStringView destDir,
                                                  const unsigned char* samp) const
{
    auto search = m_entries.find(id);
    if (search == m_entries.cend())
        return;
    _extractCompressed(id, search->second, destDir, samp);
}

void AudioGroupSampleDirectory::extractAllCompressed(amuse::SystemStringView destDir,
                                                     const unsigned char* samp) const
{
    for (const auto& ent : m_entries)
        _extractCompressed(ent.first, ent.second, destDir, samp);
}
}

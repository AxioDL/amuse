#ifndef __AMUSE_AUDIOGROUPSAMPLEDIR_HPP__
#define __AMUSE_AUDIOGROUPSAMPLEDIR_HPP__

#include <unordered_map>
#include <cstdint>
#include "Common.hpp"

namespace amuse
{
class AudioGroupData;
class AudioGroupDatabase;

struct DSPADPCMHeader : BigDNA
{
    AT_DECL_DNA
    Value<atUint32> x0_num_samples;
    Value<atUint32> x4_num_nibbles;
    Value<atUint32> x8_sample_rate;
    Value<atUint16> xc_loop_flag = 0;
    Value<atUint16> xe_format = 0; /* 0 for ADPCM */
    Value<atUint32> x10_loop_start_nibble = 0;
    Value<atUint32> x14_loop_end_nibble = 0;
    Value<atUint32> x18_ca = 0;
    Value<atInt16> x1c_coef[8][2];
    Value<atInt16> x3c_gain = 0;
    Value<atInt16> x3e_ps = 0;
    Value<atInt16> x40_hist1 = 0;
    Value<atInt16> x42_hist2 = 0;
    Value<atInt16> x44_loop_ps = 0;
    Value<atInt16> x46_loop_hist1 = 0;
    Value<atInt16> x48_loop_hist2 = 0;
    Value<atUint8> m_pitch = 0; // Stash this in the padding
    Seek<21, athena::Current> pad;
};

struct WAVFormatChunk : LittleDNA
{
    AT_DECL_DNA
    Value<atUint16> sampleFmt = 1;
    Value<atUint16> numChannels = 1;
    Value<atUint32> sampleRate;
    Value<atUint32> byteRate; // sampleRate * numChannels * bitsPerSample/8
    Value<atUint16> blockAlign = 2; // numChannels * bitsPerSample/8
    Value<atUint16> bitsPerSample = 16;
};

struct WAVSampleChunk : LittleDNA
{
    AT_DECL_DNA
    Value<atUint32> smplManufacturer = 0;
    Value<atUint32> smplProduct = 0;
    Value<atUint32> smplPeriod; // 1 / sampleRate in nanoseconds
    Value<atUint32> midiNote; // native MIDI note of sample
    Value<atUint32> midiPitchFrac = 0;
    Value<atUint32> smpteFormat = 0;
    Value<atUint32> smpteOffset = 0;
    Value<atUint32> numSampleLoops = 0;
    Value<atUint32> additionalDataSize = 0;
};

struct WAVSampleLoop : LittleDNA
{
    AT_DECL_DNA
    Value<atUint32> cuePointId = 0;
    Value<atUint32> loopType = 0; // 0: forward loop
    Value<atUint32> start;
    Value<atUint32> end;
    Value<atUint32> fraction = 0;
    Value<atUint32> playCount = 0;
};

struct WAVHeader : LittleDNA
{
    AT_DECL_DNA
    Value<atUint32> riffMagic = SBIG('RIFF');
    Value<atUint32> wavChuckSize; // everything - 8
    Value<atUint32> wavMagic = SBIG('WAVE');

    Value<atUint32> fmtMagic = SBIG('fmt ');
    Value<atUint32> fmtChunkSize = 16;
    WAVFormatChunk fmtChunk;

    Value<atUint32> smplMagic = SBIG('smpl');
    Value<atUint32> smplChunkSize = 36; // 36 + numSampleLoops*24
    WAVSampleChunk smplChunk;

    Value<atUint32> dataMagic = SBIG('data');
    Value<atUint32> dataChunkSize; // numSamples * numChannels * bitsPerSample/8
};

struct WAVHeaderLoop : LittleDNA
{
    AT_DECL_DNA
    Value<atUint32> riffMagic = SBIG('RIFF');
    Value<atUint32> wavChuckSize; // everything - 8
    Value<atUint32> wavMagic = SBIG('WAVE');

    Value<atUint32> fmtMagic = SBIG('fmt ');
    Value<atUint32> fmtChunkSize = 16;
    WAVFormatChunk fmtChunk;

    Value<atUint32> smplMagic = SBIG('smpl');
    Value<atUint32> smplChunkSize = 60; // 36 + numSampleLoops*24
    WAVSampleChunk smplChunk;
    WAVSampleLoop sampleLoop;

    Value<atUint32> dataMagic = SBIG('data');
    Value<atUint32> dataChunkSize; // numSamples * numChannels * bitsPerSample/8
};

enum class SampleFormat : uint8_t
{
    DSP,      /**< GCN DSP-ucode ADPCM (very common for GameCube games) */
    DSP_DRUM, /**< GCN DSP-ucode ADPCM (seems to be set into drum samples for expanding their amplitude appropriately) */
    PCM,      /**< Big-endian PCM found in MusyX2 demo GM instruments */
    N64,      /**< 2-stage VADPCM coding with SAMP-embedded codebooks */
    PCM_PC    /**< Little-endian PCM found in PC Rogue Squadron (actually enum 0 which conflicts with DSP-ADPCM) */
};

enum class SampleFileState
{
    NoData,
    MemoryOnlyWAV,
    MemoryOnlyCompressed,
    WAVRecent,
    CompressedRecent,
    CompressedNoWAV,
    WAVNoCompressed
};

/** Indexes individual samples in SAMP chunk */
class AudioGroupSampleDirectory
{
    friend class AudioGroup;

public:
    union ADPCMParms {
        struct DSPParms
        {
            uint16_t m_bytesPerFrame;
            uint8_t m_ps;
            uint8_t m_lps;
            int16_t m_hist2;
            int16_t m_hist1;
            int16_t m_coefs[8][2];
        } dsp;
        struct VADPCMParms
        {
            int16_t m_coefs[8][2][8];
        } vadpcm;
        void swapBigDSP();
        void swapBigVADPCM();
    };

    template <athena::Endian DNAEn>
    struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
    EntryDNA : BigDNA
    {
        AT_DECL_DNA
        SampleIdDNA<DNAEn> m_sfxId;
        Seek<2, athena::Current> pad;
        Value<atUint32, DNAEn> m_sampleOff;
        Value<atUint32, DNAEn> m_unk;
        Value<atUint8, DNAEn> m_pitch;
        Seek<1, athena::Current> pad2;
        Value<atUint16, DNAEn> m_sampleRate;
        Value<atUint32, DNAEn> m_numSamples; // Top 8 bits is SampleFormat
        Value<atUint32, DNAEn> m_loopStartSample;
        Value<atUint32, DNAEn> m_loopLengthSamples;
        Value<atUint32, DNAEn> m_adpcmParmOffset;

        void _setLoopStartSample(atUint32 sample)
        {
            m_loopLengthSamples += m_loopStartSample - sample;
            m_loopStartSample = sample;
        }
        void setLoopEndSample(atUint32 sample)
        {
            m_loopLengthSamples = sample + 1 - m_loopStartSample;
        }
    };
    template <athena::Endian DNAEn>
    struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
    MusyX1SdirEntry : BigDNA
    {
        AT_DECL_DNA
        SampleIdDNA<DNAEn> m_sfxId;
        Seek<2, athena::Current> pad;
        Value<atUint32, DNAEn> m_sampleOff;
        Value<atUint32, DNAEn> m_pitchSampleRate;
        Value<atUint32, DNAEn> m_numSamples;
        Value<atUint32, DNAEn> m_loopStartSample;
        Value<atUint32, DNAEn> m_loopLengthSamples;
    };
    template <athena::Endian DNAEn>
    struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
    MusyX1AbsSdirEntry : BigDNA
    {
        AT_DECL_DNA
        SampleIdDNA<DNAEn> m_sfxId;
        Seek<2, athena::Current> pad;
        Value<uint32_t, DNAEn> m_sampleOff;
        Value<uint32_t, DNAEn> m_unk;
        Value<uint32_t, DNAEn> m_pitchSampleRate;
        Value<uint32_t, DNAEn> m_numSamples;
        Value<uint32_t, DNAEn> m_loopStartSample;
        Value<uint32_t, DNAEn> m_loopLengthSamples;
    };
    struct EntryData
    {
        atUint32 m_sampleOff = 0;
        atUint32 m_unk = 0;
        atUint8 m_pitch = 0;
        atUint16 m_sampleRate = 0;
        atUint32 m_numSamples = 0; // Top 8 bits is SampleFormat
        atUint32 m_loopStartSample = 0;
        atUint32 m_loopLengthSamples = 0;
        atUint32 m_adpcmParmOffset = 0;

        /* Stored out-of-band in a platform-dependent way */
        ADPCMParms m_ADPCMParms;

        /* In-memory storage of an individual sample. Editors use this structure
         * to override the loaded sample with a file-backed version without repacking
         * the sample data into a SAMP block. */
        time_t m_looseModTime = 0;
        std::unique_ptr<uint8_t[]> m_looseData;

        /* Use middle C when pitch is (impossibly low) default */
        atUint8 getPitch() const { return m_pitch == 0 ? atUint8(60) : m_pitch; }
        atUint32 getNumSamples() const { return m_numSamples & 0xffffff; }
        SampleFormat getSampleFormat() const { return SampleFormat(m_numSamples >> 24); }
        bool isFormatDSP() const
        {
            SampleFormat fmt = getSampleFormat();
            return fmt == SampleFormat::DSP || fmt == SampleFormat::DSP_DRUM;
        }

        void _setLoopStartSample(atUint32 sample)
        {
            m_loopLengthSamples += m_loopStartSample - sample;
            m_loopStartSample = sample;
        }
        void setLoopStartSample(atUint32 sample);
        atUint32 getLoopStartSample() const
        {
            return m_loopStartSample;
        }
        void setLoopEndSample(atUint32 sample)
        {
            m_loopLengthSamples = sample + 1 - m_loopStartSample;
        }
        atUint32 getLoopEndSample() const
        {
            return m_loopStartSample + m_loopLengthSamples - 1;
        }

        EntryData() = default;

        template <athena::Endian DNAE>
        EntryData(const EntryDNA<DNAE>& in)
            : m_sampleOff(in.m_sampleOff), m_unk(in.m_unk), m_pitch(in.m_pitch),
              m_sampleRate(in.m_sampleRate), m_numSamples(in.m_numSamples),
              m_loopStartSample(in.m_loopStartSample), m_loopLengthSamples(in.m_loopLengthSamples),
              m_adpcmParmOffset(in.m_adpcmParmOffset) {}

        template <athena::Endian DNAE>
        EntryData(const MusyX1SdirEntry<DNAE>& in)
            : m_sampleOff(in.m_sampleOff), m_unk(0), m_pitch(in.m_pitchSampleRate >> 24),
              m_sampleRate(in.m_pitchSampleRate & 0xffff), m_numSamples(in.m_numSamples),
              m_loopStartSample(in.m_loopStartSample), m_loopLengthSamples(in.m_loopLengthSamples),
              m_adpcmParmOffset(0) {}

        template <athena::Endian DNAE>
        EntryData(const MusyX1AbsSdirEntry<DNAE>& in)
            : m_sampleOff(in.m_sampleOff), m_unk(in.m_unk), m_pitch(in.m_pitchSampleRate >> 24),
              m_sampleRate(in.m_pitchSampleRate & 0xffff), m_numSamples(in.m_numSamples),
              m_loopStartSample(in.m_loopStartSample), m_loopLengthSamples(in.m_loopLengthSamples),
              m_adpcmParmOffset(0) {}

        template <athena::Endian DNAEn>
        EntryDNA<DNAEn> toDNA(SFXId id) const
        {
            EntryDNA<DNAEn> ret;
            ret.m_sfxId.id = id;
            ret.m_sampleOff = m_sampleOff;
            ret.m_unk = m_unk;
            ret.m_pitch = m_pitch;
            ret.m_sampleRate = m_sampleRate;
            ret.m_numSamples = m_numSamples;
            ret.m_loopStartSample = m_loopStartSample;
            ret.m_loopLengthSamples = m_loopLengthSamples;
            ret.m_adpcmParmOffset = m_adpcmParmOffset;
            return ret;
        }

        void loadLooseDSP(SystemStringView dspPath);
        void loadLooseWAV(SystemStringView wavPath);

        void patchMetadataDSP(SystemStringView dspPath);
        void patchMetadataWAV(SystemStringView wavPath);
    };
    /* This double-wrapper allows Voices to keep a strong reference on
     * a single instance of loaded loose data without being unexpectedly
     * clobbered */
    struct Entry
    {
        ObjToken<EntryData> m_data;

        Entry()
            : m_data(MakeObj<EntryData>()) {}

        template <athena::Endian DNAE>
        Entry(const EntryDNA<DNAE>& in)
            : m_data(MakeObj<EntryData>(in)) {}

        template <athena::Endian DNAE>
        Entry(const MusyX1SdirEntry<DNAE>& in)
            : m_data(MakeObj<EntryData>(in)) {}

        template <athena::Endian DNAE>
        Entry(const MusyX1AbsSdirEntry<DNAE>& in)
            : m_data(MakeObj<EntryData>(in)) {}

        template <athena::Endian DNAEn>
        EntryDNA<DNAEn> toDNA(SFXId id) const
        {
            return m_data->toDNA<DNAEn>(id);
        }

        void loadLooseData(SystemStringView basePath);
        SampleFileState getFileState(SystemStringView basePath, SystemString* pathOut = nullptr) const;
        void patchSampleMetadata(SystemStringView basePath) const;
    };

private:
    std::unordered_map<SampleId, ObjToken<Entry>> m_entries;
    static void _extractWAV(SampleId id, const EntryData& ent, amuse::SystemStringView destDir,
                            const unsigned char* samp);
    static void _extractCompressed(SampleId id, const EntryData& ent, amuse::SystemStringView destDir,
                                   const unsigned char* samp, bool compressWAV = false);

public:
    AudioGroupSampleDirectory() = default;
    AudioGroupSampleDirectory(athena::io::IStreamReader& r, GCNDataTag);
    AudioGroupSampleDirectory(athena::io::IStreamReader& r, const unsigned char* sampData, bool absOffs, N64DataTag);
    AudioGroupSampleDirectory(athena::io::IStreamReader& r, bool absOffs, PCDataTag);
    static AudioGroupSampleDirectory CreateAudioGroupSampleDirectory(const AudioGroupData& data);
    static AudioGroupSampleDirectory CreateAudioGroupSampleDirectory(SystemStringView groupPath);

    const std::unordered_map<SampleId, ObjToken<Entry>>& sampleEntries() const { return m_entries; }
    std::unordered_map<SampleId, ObjToken<Entry>>& sampleEntries() { return m_entries; }

    void extractWAV(SampleId id, amuse::SystemStringView destDir, const unsigned char* samp) const;
    void extractAllWAV(amuse::SystemStringView destDir, const unsigned char* samp) const;
    void extractCompressed(SampleId id, amuse::SystemStringView destDir, const unsigned char* samp) const;
    void extractAllCompressed(amuse::SystemStringView destDir, const unsigned char* samp) const;

    void reloadSampleData(SystemStringView groupPath);

    bool toGCNData(SystemStringView groupPath, const AudioGroupDatabase& group) const;

    AudioGroupSampleDirectory(const AudioGroupSampleDirectory&) = delete;
    AudioGroupSampleDirectory& operator=(const AudioGroupSampleDirectory&) = delete;
    AudioGroupSampleDirectory(AudioGroupSampleDirectory&&) = default;
    AudioGroupSampleDirectory& operator=(AudioGroupSampleDirectory&&) = default;
};

using SampleEntry = AudioGroupSampleDirectory::Entry;
using SampleEntryData = AudioGroupSampleDirectory::EntryData;
}

#endif // __AMUSE_AUDIOGROUPSAMPLEDIR_HPP__

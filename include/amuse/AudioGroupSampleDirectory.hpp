#ifndef __AMUSE_AUDIOGROUPSAMPLEDIR_HPP__
#define __AMUSE_AUDIOGROUPSAMPLEDIR_HPP__

#include <unordered_map>
#include <cstdint>
#include "Common.hpp"

namespace amuse
{
class AudioGroupData;

/** Indexes individual samples in SAMP chunk */
class AudioGroupSampleDirectory
{
    friend class AudioGroup;

public:
    enum class SampleFormat : atUint8
    {
        DSP,
        DSP2,
        PCM,
        N64
    };

    template <athena::Endian DNAEn>
    struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
    EntryDNA : BigDNA
    {
        AT_DECL_DNA
        SFXIdDNA<DNAEn> m_sfxId;
        Value<atUint32, DNAEn> m_sampleOff;
        Value<atUint32, DNAEn> m_unk;
        Value<atUint8, DNAEn> m_pitch;
        Seek<1, athena::Current> pad;
        Value<atUint16, DNAEn> m_sampleRate;
        Value<atUint32, DNAEn> m_numSamples; // Top 8 bits is SampleFormat
        Value<atUint32, DNAEn> m_loopStartSample;
        Value<atUint32, DNAEn> m_loopLengthSamples;
        Value<atUint32, DNAEn> m_adpcmParmOffset;
    };
    template <athena::Endian DNAEn>
    struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
    MusyX1SdirEntry : BigDNA
    {
        AT_DECL_DNA
        SFXIdDNA<DNAEn> m_sfxId;
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
        SFXIdDNA<DNAEn> m_sfxId;
        Value<uint32_t, DNAEn> m_sampleOff;
        Value<uint32_t, DNAEn> m_unk;
        Value<uint32_t, DNAEn> m_pitchSampleRate;
        Value<uint32_t, DNAEn> m_numSamples;
        Value<uint32_t, DNAEn> m_loopStartSample;
        Value<uint32_t, DNAEn> m_loopLengthSamples;
    };
    struct Entry
    {
        atUint32 m_sampleOff;
        atUint32 m_unk;
        atUint8 m_pitch;
        atUint16 m_sampleRate;
        atUint32 m_numSamples; // Top 8 bits is SampleFormat
        atUint32 m_loopStartSample;
        atUint32 m_loopLengthSamples;
        atUint32 m_adpcmParmOffset;

        Entry() = default;

        template <athena::Endian DNAE>
        Entry(const EntryDNA<DNAE>& in)
            : m_sampleOff(in.m_sampleOff), m_unk(in.m_unk), m_pitch(in.m_pitch),
              m_sampleRate(in.m_sampleRate), m_numSamples(in.m_numSamples),
              m_loopStartSample(in.m_loopStartSample), m_loopLengthSamples(in.m_loopLengthSamples),
              m_adpcmParmOffset(in.m_adpcmParmOffset) {}

        template <athena::Endian DNAE>
        Entry(const MusyX1SdirEntry<DNAE>& in)
            : m_sampleOff(in.m_sampleOff), m_unk(0), m_pitch(in.m_pitchSampleRate >> 24),
              m_sampleRate(in.m_pitchSampleRate & 0xffff), m_numSamples(in.m_numSamples),
              m_loopStartSample(in.m_loopStartSample), m_loopLengthSamples(in.m_loopLengthSamples),
              m_adpcmParmOffset(0) {}

        template <athena::Endian DNAE>
        Entry(const MusyX1AbsSdirEntry<DNAE>& in)
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
    };

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

private:
    std::unordered_map<SFXId, std::pair<Entry, ADPCMParms>> m_entries;

public:
    AudioGroupSampleDirectory(athena::io::IStreamReader& r, GCNDataTag);
    AudioGroupSampleDirectory(athena::io::IStreamReader& r, const unsigned char* sampData, bool absOffs, N64DataTag);
    AudioGroupSampleDirectory(athena::io::IStreamReader& r, bool absOffs, PCDataTag);
    static AudioGroupSampleDirectory CreateAudioGroupSampleDirectory(const AudioGroupData& data);

    const std::unordered_map<SFXId, std::pair<Entry, ADPCMParms>>& sampleEntries() const { return m_entries; }
};
}

#endif // __AMUSE_AUDIOGROUPSAMPLEDIR_HPP__

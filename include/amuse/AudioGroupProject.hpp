#ifndef __AMUSE_AUDIOGROUPPROJECT_HPP__
#define __AMUSE_AUDIOGROUPPROJECT_HPP__

#include "Entity.hpp"
#include "Common.hpp"
#include <vector>
#include <array>
#include <unordered_map>
#include <memory>

namespace amuse
{
class AudioGroupData;

enum class GroupType : atUint16
{
    Song,
    SFX
};

/** Header at top of project file */
template <athena::Endian DNAEn>
struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
GroupHeader : BigDNA
{
    AT_DECL_DNA
    Value<atUint32, DNAEn> groupEndOff;
    GroupIdDNA<DNAEn> groupId;
    Value<GroupType, DNAEn> type;
    Value<atUint32, DNAEn> soundMacroIdsOff;
    Value<atUint32, DNAEn> samplIdsOff;
    Value<atUint32, DNAEn> tableIdsOff;
    Value<atUint32, DNAEn> keymapIdsOff;
    Value<atUint32, DNAEn> layerIdsOff;
    Value<atUint32, DNAEn> pageTableOff;
    Value<atUint32, DNAEn> drumTableOff;
    Value<atUint32, DNAEn> midiSetupsOff;
};

/** Common index members of SongGroups and SFXGroups */
struct AudioGroupIndex {};

/** Root index of SongGroup */
struct SongGroupIndex : AudioGroupIndex
{
    /** Maps GM program numbers to sound entities */
    template <athena::Endian DNAEn>
    struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
    PageEntryDNA : BigDNA
    {
        AT_DECL_DNA_YAML
        PageObjectIdDNA<DNAEn> objId;
        Value<atUint8> priority;
        Value<atUint8> maxVoices;
        Value<atUint8> programNo;
        Seek<1, athena::Current> pad;
    };
    template <athena::Endian DNAEn>
    struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
    MusyX1PageEntryDNA : BigDNA
    {
        AT_DECL_DNA
        PageObjectIdDNA<DNAEn> objId;
        Value<atUint8> priority;
        Value<atUint8> maxVoices;
        Value<atUint8> unk;
        Value<atUint8> programNo;
        Seek<2, athena::Current> pad;
    };
    struct PageEntry : BigDNA
    {
        AT_DECL_DNA_YAML
        PageObjectIdDNA<athena::Big> objId;
        Value<atUint8> priority;
        Value<atUint8> maxVoices;

        PageEntry() = default;

        template <athena::Endian DNAE>
        PageEntry(const PageEntryDNA<DNAE>& in)
        : objId(in.objId.id), priority(in.priority), maxVoices(in.maxVoices) {}

        template <athena::Endian DNAE>
        PageEntry(const MusyX1PageEntryDNA<DNAE>& in)
        : objId(in.objId.id), priority(in.priority), maxVoices(in.maxVoices) {}

        template <athena::Endian DNAEn>
        PageEntryDNA<DNAEn> toDNA(uint8_t programNo) const
        {
            PageEntryDNA<DNAEn> ret;
            ret.objId = objId;
            ret.priority = priority;
            ret.maxVoices = maxVoices;
            ret.programNo = programNo;
            return ret;
        }
    };
    std::unordered_map<uint8_t, PageEntry> m_normPages;
    std::unordered_map<uint8_t, PageEntry> m_drumPages;

    /** Maps SongID to 16 MIDI channel numbers to GM program numbers and settings */
    struct MusyX1MIDISetup : BigDNA
    {
        AT_DECL_DNA_YAML
        Value<atUint8> programNo;
        Value<atUint8> volume;
        Value<atUint8> panning;
        Value<atUint8> reverb;
        Value<atUint8> chorus;
        Seek<3, athena::Current> pad;
    };
    struct MIDISetup : BigDNA
    {
        AT_DECL_DNA_YAML
        Value<atUint8> programNo;
        Value<atUint8> volume;
        Value<atUint8> panning;
        Value<atUint8> reverb;
        Value<atUint8> chorus;
        MIDISetup() = default;
        MIDISetup(const MusyX1MIDISetup& setup)
        : programNo(setup.programNo), volume(setup.volume), panning(setup.panning),
          reverb(setup.reverb), chorus(setup.chorus) {}
    };
    std::unordered_map<SongId, std::array<MIDISetup, 16>> m_midiSetups;
};

/** Root index of SFXGroup */
struct SFXGroupIndex : AudioGroupIndex
{
    /** Maps game-side SFX define IDs to sound entities */
    template <athena::Endian DNAEn>
    struct AT_SPECIALIZE_PARMS(athena::Endian::Big, athena::Endian::Little)
    SFXEntryDNA : BigDNA
    {
        AT_DECL_DNA
        SFXIdDNA<DNAEn> sfxId;
        SoundMacroIdDNA<DNAEn> macro;
        Value<atUint8> priority;
        Value<atUint8> maxVoices;
        Value<atUint8> defVel;
        Value<atUint8> panning;
        Value<atUint8> defKey;
        Seek<1, athena::Current> pad;
    };
    struct SFXEntry : BigDNA
    {
        AT_DECL_DNA_YAML
        SoundMacroIdDNA<athena::Big> macro;
        Value<atUint8> priority;
        Value<atUint8> maxVoices;
        Value<atUint8> defVel;
        Value<atUint8> panning;
        Value<atUint8> defKey;

        SFXEntry() = default;

        template <athena::Endian DNAE>
        SFXEntry(const SFXEntryDNA<DNAE>& in)
        : macro(in.macro.id), priority(in.priority), maxVoices(in.maxVoices),
          defVel(in.defVel), panning(in.panning), defKey(in.defKey) {}

        template <athena::Endian DNAEn>
        SFXEntryDNA<DNAEn> toDNA(SFXId id) const
        {
            SFXEntryDNA<DNAEn> ret;
            ret.sfxId.id = id;
            ret.macro = macro;
            ret.priority = priority;
            ret.maxVoices = maxVoices;
            ret.defVel = defVel;
            ret.panning = panning;
            ret.defKey = defKey;
            return ret;
        }
    };
    std::unordered_map<SFXId, SFXEntry> m_sfxEntries;
};

/** Collection of SongGroup and SFXGroup indexes */
class AudioGroupProject
{
    std::unordered_map<GroupId, ObjToken<SongGroupIndex>> m_songGroups;
    std::unordered_map<GroupId, ObjToken<SFXGroupIndex>> m_sfxGroups;

    AudioGroupProject(athena::io::IStreamReader& r, GCNDataTag);
    template <athena::Endian DNAE>
    static AudioGroupProject _AudioGroupProject(athena::io::IStreamReader& r, bool absOffs);

    static void BootstrapObjectIDs(athena::io::IStreamReader& r, GCNDataTag);
    template <athena::Endian DNAE>
    static void BootstrapObjectIDs(athena::io::IStreamReader& r, bool absOffs);
public:
    AudioGroupProject() = default;
    static AudioGroupProject CreateAudioGroupProject(const AudioGroupData& data);
    static AudioGroupProject CreateAudioGroupProject(SystemStringView groupPath);
    static void BootstrapObjectIDs(const AudioGroupData& data);

    const SongGroupIndex* getSongGroupIndex(int groupId) const;
    const SFXGroupIndex* getSFXGroupIndex(int groupId) const;

    const std::unordered_map<GroupId, ObjToken<SongGroupIndex>>& songGroups() const { return m_songGroups; }
    const std::unordered_map<GroupId, ObjToken<SFXGroupIndex>>& sfxGroups() const { return m_sfxGroups; }
    std::unordered_map<GroupId, ObjToken<SongGroupIndex>>& songGroups() { return m_songGroups; }
    std::unordered_map<GroupId, ObjToken<SFXGroupIndex>>& sfxGroups() { return m_sfxGroups; }

    bool toYAML(SystemStringView groupPath) const;

    AudioGroupProject(const AudioGroupProject&) = delete;
    AudioGroupProject& operator=(const AudioGroupProject&) = delete;
    AudioGroupProject(AudioGroupProject&&) = default;
    AudioGroupProject& operator=(AudioGroupProject&&) = default;
};
}

#endif // __AMUSE_AUDIOGROUPPROJECT_HPP__

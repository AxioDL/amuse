#pragma once

#include <string>
#include <unordered_set>

#include "amuse/AudioGroupPool.hpp"
#include "amuse/AudioGroupProject.hpp"
#include "amuse/AudioGroupSampleDirectory.hpp"
#include "amuse/Common.hpp"

namespace amuse {
class AudioGroupData;
class ProjectDatabase;

/** Runtime audio group index container */
class AudioGroup {
  friend class AudioGroupSampleDirectory;

protected:
  AudioGroupProject m_proj;
  AudioGroupPool m_pool;
  AudioGroupSampleDirectory m_sdir;
  const unsigned char* m_samp = nullptr;
  std::string m_groupPath; /* Typically only set by editor */
  bool m_valid;

public:
  std::string getSampleBasePath(SampleId sfxId) const;
  explicit operator bool() const { return m_valid; }
  AudioGroup() = default;
  explicit AudioGroup(const AudioGroupData& data) { assign(data); }
  explicit AudioGroup(std::string_view groupPath) { assign(groupPath); }
  explicit AudioGroup(const AudioGroup& data, std::string_view groupPath) { assign(data, groupPath); }

  void assign(const AudioGroupData& data);
  void assign(std::string_view groupPath);
  void assign(const AudioGroup& data, std::string_view groupPath);
  void setGroupPath(std::string_view groupPath) { m_groupPath = groupPath; }

  const SampleEntry* getSample(SampleId sfxId) const;
  std::pair<ObjToken<SampleEntryData>, const unsigned char*> getSampleData(SampleId sfxId,
                                                                           const SampleEntry* sample) const;
  SampleFileState getSampleFileState(SampleId sfxId, const SampleEntry* sample, std::string* pathOut = nullptr) const;
  void patchSampleMetadata(SampleId sfxId, const SampleEntry* sample) const;
  void makeWAVVersion(SampleId sfxId, const SampleEntry* sample) const;
  void makeCompressedVersion(SampleId sfxId, const SampleEntry* sample) const;
  const AudioGroupProject& getProj() const { return m_proj; }
  const AudioGroupPool& getPool() const { return m_pool; }
  const AudioGroupSampleDirectory& getSdir() const { return m_sdir; }
  AudioGroupProject& getProj() { return m_proj; }
  AudioGroupPool& getPool() { return m_pool; }
  AudioGroupSampleDirectory& getSdir() { return m_sdir; }
};

class AudioGroupDatabase final : public AudioGroup {
  void _recursiveRenameMacro(SoundMacroId id, std::string_view str, int& macroIdx,
                             std::unordered_set<SoundMacroId>& renamedIds);

public:
  AudioGroupDatabase() = default;
  explicit AudioGroupDatabase(const AudioGroupData& data) {
    assign(data);
  }
  explicit AudioGroupDatabase(std::string_view groupPath) {
    assign(groupPath);
  }
  explicit AudioGroupDatabase(const AudioGroupDatabase& data, std::string_view groupPath) {
    assign(data, groupPath);
  }

  void renameSample(SampleId id, std::string_view str);
  void deleteSample(SampleId id);
  void copySampleInto(const std::string& basePath, const std::string& newBasePath);

  void importCHeader(std::string_view header);
  std::string exportCHeader(std::string_view projectName, std::string_view groupName) const;
};

class ProjectDatabase {
  NameDB m_songDb;
  NameDB m_sfxDb;
  NameDB m_groupDb;
  NameDB m_soundMacroDb;
  NameDB m_sampleDb;
  NameDB m_tableDb;
  NameDB m_keymapDb;
  NameDB m_layersDb;

public:
  void setIdDatabases() const {
    SongId::CurNameDB = const_cast<NameDB*>(&m_songDb);
    SFXId::CurNameDB = const_cast<NameDB*>(&m_sfxDb);
    GroupId::CurNameDB = const_cast<NameDB*>(&m_groupDb);
    SoundMacroId::CurNameDB = const_cast<NameDB*>(&m_soundMacroDb);
    SampleId::CurNameDB = const_cast<NameDB*>(&m_sampleDb);
    TableId::CurNameDB = const_cast<NameDB*>(&m_tableDb);
    KeymapId::CurNameDB = const_cast<NameDB*>(&m_keymapDb);
    LayersId::CurNameDB = const_cast<NameDB*>(&m_layersDb);
  }
};
} // namespace amuse

#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "amuse/Common.hpp"

namespace amuse {

struct CaseInsensitiveCompare {
  bool operator()(std::string_view lhs, std::string_view rhs) const {
#if _WIN32
    if (_stricmp(lhs.data(), rhs.data()) < 0)
#else
    if (strcasecmp(lhs.data(), rhs.data()) < 0)
#endif
      return true;
    return false;
  }

#if _WIN32
  bool operator()(std::wstring_view lhs, std::wstring_view rhs) const {
    if (_wcsicmp(lhs.data(), rhs.data()) < 0)
      return true;
    return false;
  }
#endif
};

class DirectoryEnumerator {
public:
  enum class Mode { Native, DirsSorted, FilesSorted, DirsThenFilesSorted };
  struct Entry {
    SystemString m_path;
    SystemString m_name;
    size_t m_fileSz;
    bool m_isDir;

    Entry(const SystemString& path, const SystemChar* name, size_t sz, bool isDir)
    : m_path(path), m_name(name), m_fileSz(sz), m_isDir(isDir) {}
  };

private:
  std::vector<Entry> m_entries;

public:
  DirectoryEnumerator(SystemStringView path, Mode mode = Mode::DirsThenFilesSorted, bool sizeSort = false,
                      bool reverse = false, bool noHidden = false);

  operator bool() const { return m_entries.size() != 0; }
  size_t size() const { return m_entries.size(); }
  std::vector<Entry>::const_iterator begin() const { return m_entries.cbegin(); }
  std::vector<Entry>::const_iterator end() const { return m_entries.cend(); }
};
} // namespace amuse

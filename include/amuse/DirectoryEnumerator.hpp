#pragma once

#include <cstring>
#include <string>
#include <vector>

#include "amuse/Common.hpp"

namespace amuse {

/**
 * @brief Case-insensitive comparator for std::map sorting
 */
struct CaseInsensitiveCompare {
  // Allow heterogeneous lookup with maps that use this comparator.
  using is_transparent = void;

  bool operator()(std::string_view lhs, std::string_view rhs) const {
    return std::lexicographical_compare(lhs.begin(), lhs.end(), rhs.begin(), rhs.end(), [](char lhs, char rhs) {
      return std::tolower(static_cast<unsigned char>(lhs)) < std::tolower(static_cast<unsigned char>(rhs));
    });
  }
};

class DirectoryEnumerator {
public:
  enum class Mode { Native, DirsSorted, FilesSorted, DirsThenFilesSorted };
  struct Entry {
    std::string m_path;
    std::string m_name;
    size_t m_fileSz;
    bool m_isDir;

    Entry(std::string path, std::string name, size_t sz, bool isDir)
    : m_path(std::move(path)), m_name(std::move(name)), m_fileSz(sz), m_isDir(isDir) {}
  };

private:
  std::vector<Entry> m_entries;

public:
  DirectoryEnumerator(std::string_view path, Mode mode = Mode::DirsThenFilesSorted, bool sizeSort = false,
                      bool reverse = false, bool noHidden = false);

  explicit operator bool() const { return !m_entries.empty(); }
  [[nodiscard]] size_t size() const { return m_entries.size(); }
  [[nodiscard]] std::vector<Entry>::const_iterator begin() const { return m_entries.cbegin(); }
  [[nodiscard]] std::vector<Entry>::const_iterator end() const { return m_entries.cend(); }
};
} // namespace amuse

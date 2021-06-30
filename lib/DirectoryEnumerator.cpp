#ifdef _WIN32
#include <windows.h>
#include <cstdio>
#else
#include <dirent.h>
#endif

#include <ranges>
#include <sys/stat.h>

#if !defined(S_ISREG) && defined(S_IFMT) && defined(S_IFREG)
#define S_ISREG(m) (((m)&S_IFMT) == S_IFREG)
#endif

#if !defined(S_ISDIR) && defined(S_IFMT) && defined(S_IFDIR)
#define S_ISDIR(m) (((m)&S_IFMT) == S_IFDIR)
#endif

#include <map>

#include "amuse/DirectoryEnumerator.hpp"

namespace amuse {

DirectoryEnumerator::DirectoryEnumerator(std::string_view path, Mode mode, bool sizeSort, bool reverse, bool noHidden) {
  Sstat theStat;
  if (Stat(path.data(), &theStat) || !S_ISDIR(theStat.st_mode)) {
    return;
  }

#if _WIN32
  std::wstring wc = nowide::widen(path);
  wc += L"/*";
  WIN32_FIND_DATAW d;
  HANDLE dir = FindFirstFileW(wc.c_str(), &d);
  if (dir == INVALID_HANDLE_VALUE) {
    return;
  }
  switch (mode) {
  case Mode::Native:
    do {
      if (!wcscmp(d.cFileName, L".") || !wcscmp(d.cFileName, L"..")) {
        continue;
      }
      if (noHidden && (d.cFileName[0] == L'.' || (d.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)) {
        continue;
      }
      std::string fileName = nowide::narrow(d.cFileName);
      std::string fp(path);
      fp += '/';
      fp += fileName;
      Sstat st;
      if (Stat(fp.c_str(), &st)) {
        continue;
      }

      size_t sz = 0;
      bool isDir = false;
      if (S_ISDIR(st.st_mode)) {
        isDir = true;
      } else if (S_ISREG(st.st_mode)) {
        sz = st.st_size;
      } else {
        continue;
      }

      m_entries.emplace_back(fp, fileName, sz, isDir);
    } while (FindNextFileW(dir, &d));
    break;
  case Mode::DirsThenFilesSorted:
  case Mode::DirsSorted: {
    std::map<std::string, Entry, CaseInsensitiveCompare> sort;
    do {
      if (!wcscmp(d.cFileName, L".") || !wcscmp(d.cFileName, L"..")) {
        continue;
      }
      if (noHidden && (d.cFileName[0] == L'.' || (d.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)) {
        continue;
      }
      std::string fileName = nowide::narrow(d.cFileName);
      std::string fp(path);
      fp += '/';
      fp += fileName;
      Sstat st;
      if (Stat(fp.c_str(), &st) || !S_ISDIR(st.st_mode)) {
        continue;
      }
      sort.emplace(fileName, Entry{fp, fileName, 0, true});
    } while (FindNextFileW(dir, &d));

    m_entries.reserve(sort.size());
    if (reverse) {
      for (auto& it : std::ranges::reverse_view(sort)) {
        m_entries.emplace_back(std::move(it.second));
      }
    } else {
      for (auto& e : sort) {
        m_entries.emplace_back(std::move(e.second));
      }
    }

    if (mode == Mode::DirsSorted) {
      break;
    }
    FindClose(dir);
    dir = FindFirstFileW(wc.c_str(), &d);
  }
  case Mode::FilesSorted: {
    if (mode == Mode::FilesSorted) {
      m_entries.clear();
    }

    if (sizeSort) {
      std::multimap<size_t, Entry> sort;
      do {
        if (!wcscmp(d.cFileName, L".") || !wcscmp(d.cFileName, L"..")) {
          continue;
        }
        if (noHidden && (d.cFileName[0] == L'.' || (d.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)) {
          continue;
        }
        std::string fileName = nowide::narrow(d.cFileName);
        std::string fp(path);
        fp += '/';
        fp += fileName;
        Sstat st;
        if (Stat(fp.c_str(), &st) || !S_ISREG(st.st_mode)) {
          continue;
        }
        sort.emplace(st.st_size, Entry{fp, fileName, static_cast<size_t>(st.st_size), false});
      } while (FindNextFileW(dir, &d));

      m_entries.reserve(m_entries.size() + sort.size());
      if (reverse) {
        for (auto& it : std::ranges::reverse_view(sort)) {
          m_entries.emplace_back(std::move(it.second));
        }
      } else {
        for (auto& e : sort) {
          m_entries.emplace_back(std::move(e.second));
        }
      }
    } else {
      std::map<std::string, Entry, CaseInsensitiveCompare> sort;
      do {
        if (!wcscmp(d.cFileName, L".") || !wcscmp(d.cFileName, L"..")) {
          continue;
        }
        if (noHidden && (d.cFileName[0] == L'.' || (d.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) != 0)) {
          continue;
        }
        std::string fileName = nowide::narrow(d.cFileName);
        std::string fp(path);
        fp += '/';
        fp += fileName;
        Sstat st;
        if (Stat(fp.c_str(), &st) || !S_ISREG(st.st_mode)) {
          continue;
        }
        sort.emplace(fileName, Entry{fp, fileName, static_cast<size_t>(st.st_size), false});
      } while (FindNextFileW(dir, &d));

      m_entries.reserve(m_entries.size() + sort.size());
      if (reverse) {
        for (auto& e : std::ranges::reverse_view(sort)) {
          m_entries.emplace_back(std::move(e.second));
        }
      } else {
        for (auto& e : sort) {
          m_entries.emplace_back(std::move(e.second));
        }
      }
    }

    break;
  }
  }
  FindClose(dir);

#else

  DIR* dir = opendir(path.data());
  if (!dir)
    return;
  const dirent* d;
  switch (mode) {
  case Mode::Native:
    while ((d = readdir(dir))) {
      if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
        continue;
      if (noHidden && d->d_name[0] == '.')
        continue;
      std::string fp(path);
      fp += '/';
      fp += d->d_name;
      Sstat st;
      if (Stat(fp.c_str(), &st))
        continue;

      size_t sz = 0;
      bool isDir = false;
      if (S_ISDIR(st.st_mode))
        isDir = true;
      else if (S_ISREG(st.st_mode))
        sz = st.st_size;
      else
        continue;

      m_entries.emplace_back(fp, d->d_name, sz, isDir);
    }
    break;
  case Mode::DirsThenFilesSorted:
  case Mode::DirsSorted: {
    std::map<std::string, Entry, CaseInsensitiveCompare> sort;
    while ((d = readdir(dir))) {
      if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
        continue;
      if (noHidden && d->d_name[0] == '.')
        continue;
      std::string fp(path);
      fp += '/';
      fp += d->d_name;
      Sstat st;
      if (Stat(fp.c_str(), &st) || !S_ISDIR(st.st_mode))
        continue;
      sort.emplace(std::make_pair(d->d_name, Entry(fp, d->d_name, 0, true)));
    }

    m_entries.reserve(sort.size());
    if (reverse)
      for (auto it = sort.crbegin(); it != sort.crend(); ++it)
        m_entries.push_back(std::move(it->second));
    else
      for (auto& e : sort)
        m_entries.push_back(std::move(e.second));

    if (mode == Mode::DirsSorted)
      break;
    rewinddir(dir);
    [[fallthrough]];
  }
  case Mode::FilesSorted: {
    if (mode == Mode::FilesSorted)
      m_entries.clear();

    if (sizeSort) {
      std::multimap<size_t, Entry> sort;
      while ((d = readdir(dir))) {
        if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
          continue;
        if (noHidden && d->d_name[0] == '.')
          continue;
        std::string fp(path);
        fp += '/';
        fp += d->d_name;
        Sstat st;
        if (Stat(fp.c_str(), &st) || !S_ISREG(st.st_mode))
          continue;
        sort.emplace(std::make_pair(st.st_size, Entry(fp, d->d_name, st.st_size, false)));
      }

      m_entries.reserve(sort.size());
      if (reverse)
        for (auto it = sort.crbegin(); it != sort.crend(); ++it)
          m_entries.push_back(std::move(it->second));
      else
        for (auto& e : sort)
          m_entries.push_back(std::move(e.second));
    } else {
      std::map<std::string, Entry, CaseInsensitiveCompare> sort;
      while ((d = readdir(dir))) {
        if (!strcmp(d->d_name, ".") || !strcmp(d->d_name, ".."))
          continue;
        if (noHidden && d->d_name[0] == '.')
          continue;
        std::string fp(path);
        fp += '/';
        fp += d->d_name;
        Sstat st;
        if (Stat(fp.c_str(), &st) || !S_ISREG(st.st_mode))
          continue;
        sort.emplace(std::make_pair(d->d_name, Entry(fp, d->d_name, st.st_size, false)));
      }

      m_entries.reserve(sort.size());
      if (reverse)
        for (auto it = sort.crbegin(); it != sort.crend(); ++it)
          m_entries.push_back(std::move(it->second));
      else
        for (auto& e : sort)
          m_entries.push_back(std::move(e.second));
    }

    break;
  }
  }
  closedir(dir);

#endif
}
} // namespace amuse

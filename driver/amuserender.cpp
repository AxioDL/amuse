#include "amuse/amuse.hpp"
#include "amuse/BooBackend.hpp"
#include "athena/FileReader.hpp"
#include "boo/boo.hpp"
#include "boo/audiodev/IAudioVoiceEngine.hpp"
#include "logvisor/logvisor.hpp"
#include <optional>
#include <cstdio>
#include <cstring>
#include <signal.h>
#include <thread>
#include <map>
#include <set>
#include <vector>
#include <unordered_map>
#include <cstdarg>

static logvisor::Module Log("amuserender");

#if _WIN32
#include <DbgHelp.h>
#include <nowide/args.hpp>
#pragma comment(lib, "Dbghelp.lib")

#include <signal.h>

static void abortHandler(int signum) {
  unsigned int i;
  void* stack[100];
  unsigned short frames;
  SYMBOL_INFO* symbol;
  HANDLE process;

  process = GetCurrentProcess();
  SymInitialize(process, NULL, TRUE);
  frames = CaptureStackBackTrace(0, 100, stack, NULL);
  symbol = (SYMBOL_INFO*)calloc(sizeof(SYMBOL_INFO) + 256 * sizeof(char), 1);
  symbol->MaxNameLen = 255;
  symbol->SizeOfStruct = sizeof(SYMBOL_INFO);

  for (i = 0; i < frames; i++) {
    SymFromAddr(process, (DWORD64)(stack[i]), 0, symbol);

    printf("%i: %s - 0x%0llX", frames - i - 1, symbol->Name, symbol->Address);

    DWORD dwDisplacement;
    IMAGEHLP_LINE64 line;
    SymSetOptions(SYMOPT_LOAD_LINES);

    line.SizeOfStruct = sizeof(IMAGEHLP_LINE64);
    if (SymGetLineFromAddr64(process, (DWORD64)(stack[i]), &dwDisplacement, &line)) {
      // SymGetLineFromAddr64 returned success
      printf(" LINE %d\n", line.LineNumber);
    } else {
      printf("\n");
    }
  }

  free(symbol);

  // If you caught one of the above signals, it is likely you just
  // want to quit your program right now.
  system("PAUSE");
  exit(signum);
}
#endif

/* SIGINT will gracefully break write loop */
static bool g_BreakLoop = false;
static void SIGINTHandler(int sig) { g_BreakLoop = true; }

int main(int argc, char** argv) {
  logvisor::RegisterConsoleLogger();

  std::vector<std::string> m_args;
  m_args.reserve(argc);
  double rate = NativeSampleRate;
  int chCount = 2;
  double volume = 1.0;
  for (int i = 1; i < argc; ++i) {
    if (!strncmp(argv[i], "-r", 2)) {
      if (argv[i][2])
        rate = strtod(&argv[i][2], nullptr);
      else if (argc > (i + 1)) {
        rate = strtod(argv[i + 1], nullptr);
        ++i;
      }
    } else if (!strncmp(argv[i], "-c", 2)) {
      if (argv[i][2])
        chCount = strtoul(&argv[i][2], nullptr, 0);
      else if (argc > (i + 1)) {
        chCount = strtoul(argv[i + 1], nullptr, 0);
        ++i;
      }
    } else if (!strncmp(argv[i], "-v", 2)) {
      if (argv[i][2])
        volume = strtod(&argv[i][2], nullptr);
      else if (argc > (i + 1)) {
        volume = strtod(argv[i + 1], nullptr);
        ++i;
      }
    } else
      m_args.push_back(argv[i]);
  }

  /* Load data */
  if (m_args.size() < 1) {
    Log.report(logvisor::Error,
               FMT_STRING("Usage: amuserender <group-file> [<songs-file>] [-r <sample-rate>] [-c <channel-count>] [-v <volume "
                   "0.0-1.0>]"));
    return 1;
  }

  amuse::ContainerRegistry::Type cType = amuse::ContainerRegistry::DetectContainerType(m_args[0].c_str());
  if (cType == amuse::ContainerRegistry::Type::Invalid) {
    Log.report(logvisor::Error, FMT_STRING("invalid/no data at path argument"));
    return 1;
  }
  Log.report(logvisor::Info, FMT_STRING("Found '{}' Audio Group data"), amuse::ContainerRegistry::TypeToName(cType));

  std::vector<std::pair<std::string, amuse::IntrusiveAudioGroupData>> data =
      amuse::ContainerRegistry::LoadContainer(m_args[0].c_str());
  if (data.empty()) {
    Log.report(logvisor::Error, FMT_STRING("invalid/no data at path argument"));
    return 1;
  }

  int m_groupId = -1;
  int m_setupId = -1;
  const std::string* m_groupName = nullptr;
  const std::string* m_songName = nullptr;
  amuse::ContainerRegistry::SongData* m_arrData = nullptr;
  bool m_sfxGroup = false;

  std::list<amuse::AudioGroupProject> m_projs;
  std::map<amuse::GroupId, std::pair<std::pair<std::string, amuse::IntrusiveAudioGroupData>*,
                           amuse::ObjToken<amuse::SongGroupIndex>>>
      allSongGroups;
  std::map<amuse::GroupId, std::pair<std::pair<std::string, amuse::IntrusiveAudioGroupData>*,
                           amuse::ObjToken<amuse::SFXGroupIndex>>>
      allSFXGroups;
  size_t totalGroups = 0;

  for (auto& grp : data) {
    /* Load project to assemble group list */
    m_projs.push_back(amuse::AudioGroupProject::CreateAudioGroupProject(grp.second));
    amuse::AudioGroupProject& proj = m_projs.back();
    totalGroups += proj.sfxGroups().size() + proj.songGroups().size();

    for (auto it = proj.songGroups().begin(); it != proj.songGroups().end(); ++it)
      allSongGroups[it->first] = std::make_pair(&grp, it->second);

    for (auto it = proj.sfxGroups().begin(); it != proj.sfxGroups().end(); ++it)
      allSFXGroups[it->first] = std::make_pair(&grp, it->second);
  }

  /* Attempt loading song */
  std::vector<std::pair<std::string, amuse::ContainerRegistry::SongData>> songs;
  if (m_args.size() > 1)
    songs = amuse::ContainerRegistry::LoadSongs(m_args[1].c_str());
  else
    songs = amuse::ContainerRegistry::LoadSongs(m_args[0].c_str());

  if (songs.size()) {
    bool play = true;
    if (m_args.size() <= 1) {
      bool prompt = true;
      while (true) {
        if (prompt) {
          fmt::print(FMT_STRING("Render Song? (Y/N): "));
          prompt = false;
        }
        char userSel;
        if (scanf("%c", &userSel) <= 0 || userSel == '\n')
          continue;
        userSel = tolower(userSel);
        if (userSel == 'n')
          play = false;
        else if (userSel != 'y') {
          prompt = true;
          continue;
        }
        break;
      }
    }

    if (play) {
      /* Get song selection from user */
      if (songs.size() > 1) {
        /* Ask user to specify which song */
        fmt::print(FMT_STRING("Multiple Songs discovered:\n"));
        int idx = 0;
        for (const auto& pair : songs) {
          const amuse::ContainerRegistry::SongData& sngData = pair.second;
          int16_t grpId = sngData.m_groupId;
          int16_t setupId = sngData.m_setupId;
          if (sngData.m_groupId == -1 && sngData.m_setupId != -1) {
            for (const auto& pair : allSongGroups) {
              for (const auto& setup : pair.second.second->m_midiSetups) {
                if (setup.first == sngData.m_setupId) {
                  grpId = pair.first.id;
                  break;
                }
              }
              if (grpId != -1)
                break;
            }
          }
          fmt::print(FMT_STRING("    {} {} (Group {}, Setup {})\n"), idx++, pair.first, grpId, setupId);
        }

        int userSel = 0;
        fmt::print(FMT_STRING("Enter Song Number: "));
        if (scanf("%d", &userSel) <= 0) {
          Log.report(logvisor::Error, FMT_STRING("unable to parse prompt"));
          return 1;
        }

        if (userSel < songs.size()) {
          m_arrData = &songs[userSel].second;
          m_groupId = m_arrData->m_groupId;
          m_setupId = m_arrData->m_setupId;
          m_songName = &songs[userSel].first;
        } else {
          Log.report(logvisor::Error, FMT_STRING("unable to find Song {}"), userSel);
          return 1;
        }
      } else if (songs.size() == 1) {
        m_arrData = &songs[0].second;
        m_groupId = m_arrData->m_groupId;
        m_setupId = m_arrData->m_setupId;
        m_songName = &songs[0].first;
      }
    }
  }

  /* Get group selection via setup search */
  if (m_groupId == -1 && m_setupId != -1) {
    for (const auto& pair : allSongGroups) {
      for (const auto& setup : pair.second.second->m_midiSetups) {
        if (setup.first == m_setupId) {
          m_groupId = pair.first.id;
          m_groupName = &pair.second.first->first;
          break;
        }
      }
      if (m_groupId != -1)
        break;
    }
  }

  /* Get group selection via user */
  if (m_groupId != -1) {
    auto songSearch = allSongGroups.find(m_groupId);
    auto sfxSearch = allSFXGroups.find(m_groupId);
    if (songSearch != allSongGroups.end()) {
      m_sfxGroup = false;
      m_groupName = &songSearch->second.first->first;
    } else if (sfxSearch != allSFXGroups.end()) {
      m_sfxGroup = true;
      m_groupName = &sfxSearch->second.first->first;
    } else {
      Log.report(logvisor::Error, FMT_STRING("unable to find Group {}"), m_groupId);
      return 1;
    }
  } else if (totalGroups > 1) {
    /* Ask user to specify which group in project */
    fmt::print(FMT_STRING("Multiple Audio Groups discovered:\n"));
    for (const auto& pair : allSFXGroups) {
      fmt::print(FMT_STRING("    {} {} (SFXGroup)  {} sfx-entries\n"), pair.first,
                 pair.second.first->first, pair.second.second->m_sfxEntries.size());
    }
    for (const auto& pair : allSongGroups) {
      fmt::print(FMT_STRING("    {} {} (SongGroup)  {} normal-pages, {} drum-pages, {} MIDI-setups\n"),
                 pair.first, pair.second.first->first, pair.second.second->m_normPages.size(),
                 pair.second.second->m_drumPages.size(), pair.second.second->m_midiSetups.size());
    }

    int userSel = 0;
    fmt::print(FMT_STRING("Enter Group Number: "));
    if (scanf("%d", &userSel) <= 0) {
      Log.report(logvisor::Error, FMT_STRING("unable to parse prompt"));
      return 1;
    }

    auto songSearch = allSongGroups.find(userSel);
    auto sfxSearch = allSFXGroups.find(userSel);
    if (songSearch != allSongGroups.end()) {
      m_groupId = userSel;
      m_groupName = &songSearch->second.first->first;
      m_sfxGroup = false;
    } else if (sfxSearch != allSFXGroups.end()) {
      m_groupId = userSel;
      m_groupName = &sfxSearch->second.first->first;
      m_sfxGroup = true;
    } else {
      Log.report(logvisor::Error, FMT_STRING("unable to find Group {}"), userSel);
      return 1;
    }
  } else if (totalGroups == 1) {
    /* Load one and only group */
    if (allSongGroups.size()) {
      const auto& pair = *allSongGroups.cbegin();
      m_groupId = pair.first.id;
      m_groupName = &pair.second.first->first;
      m_sfxGroup = false;
    } else {
      const auto& pair = *allSFXGroups.cbegin();
      m_groupId = pair.first.id;
      m_groupName = &pair.second.first->first;
      m_sfxGroup = true;
    }
  } else {
    Log.report(logvisor::Error, FMT_STRING("empty project"));
    return 1;
  }

  /* Make final group selection */
  amuse::IntrusiveAudioGroupData* selData = nullptr;
  amuse::ObjToken<amuse::SongGroupIndex> songIndex;
  amuse::ObjToken<amuse::SFXGroupIndex> sfxIndex;
  auto songSearch = allSongGroups.find(m_groupId);
  if (songSearch != allSongGroups.end()) {
    selData = &songSearch->second.first->second;
    songIndex = songSearch->second.second;
    std::set<amuse::SongId> sortSetups;
    for (auto& pair : songIndex->m_midiSetups)
      sortSetups.insert(pair.first);
    if (m_setupId == -1) {
      /* Ask user to specify which group in project */
      fmt::print(FMT_STRING("Multiple MIDI Setups:\n"));
      for (auto setup : sortSetups)
        fmt::print(FMT_STRING("    {}\n"), setup);
      int userSel = 0;
      fmt::print(FMT_STRING("Enter Setup Number: "));
      if (scanf("%d", &userSel) <= 0) {
        Log.report(logvisor::Error, FMT_STRING("unable to parse prompt"));
        return 1;
      }
      m_setupId = userSel;
    }
    if (sortSetups.find(m_setupId) == sortSetups.cend()) {
      Log.report(logvisor::Error, FMT_STRING("unable to find setup {}"), m_setupId);
      return 1;
    }
  } else {
    auto sfxSearch = allSFXGroups.find(m_groupId);
    if (sfxSearch != allSFXGroups.end()) {
      selData = &sfxSearch->second.first->second;
      sfxIndex = sfxSearch->second.second;
    }
  }

  if (!selData) {
    Log.report(logvisor::Error, FMT_STRING("unable to select audio group data"));
    return 1;
  }

  if (m_sfxGroup) {
    Log.report(logvisor::Error, FMT_STRING("amuserender is currently only able to render SongGroups"));
    return 1;
  }

  /* WAV out path */
  std::string pathOut = fmt::format(FMT_STRING("{}-{}.wav"), *m_groupName, *m_songName);
  Log.report(logvisor::Info, FMT_STRING("Writing to {}"), pathOut);

  /* Build voice engine */
  std::unique_ptr<boo::IAudioVoiceEngine> voxEngine = boo::NewWAVAudioVoiceEngine(pathOut.c_str(), rate, chCount);
  amuse::BooBackendVoiceAllocator booBackend(*voxEngine);
  amuse::Engine engine(booBackend, amuse::AmplitudeMode::PerSample);
  engine.setVolume(float(std::clamp(0.0, volume, 1.0)));

  /* Load group into engine */
  const amuse::AudioGroup* group = engine.addAudioGroup(*selData);
  if (!group) {
    Log.report(logvisor::Error, FMT_STRING("unable to add audio group"));
    return 1;
  }

  /* Enter playback loop */
  amuse::ObjToken<amuse::Sequencer> seq = engine.seqPlay(m_groupId, m_setupId, m_arrData->m_data.get(), false);
  size_t wroteFrames = 0;
  signal(SIGINT, SIGINTHandler);
  do {
    voxEngine->pumpAndMixVoices();
    wroteFrames += voxEngine->get5MsFrames();
    fmt::print(FMT_STRING("\rFrame {}"), wroteFrames);
    fflush(stdout);
  } while (!g_BreakLoop && (seq->state() == amuse::SequencerState::Playing || seq->getVoiceCount() != 0));

  fmt::print(FMT_STRING("\n"));
  return 0;
}

#if _WIN32
#include <shellapi.h>

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
  signal(SIGABRT, abortHandler);
  signal(SIGSEGV, abortHandler);
  signal(SIGILL, abortHandler);
  signal(SIGFPE, abortHandler);

  int argc = 0;
  char** argv = nullptr;
  nowide::args _(argc, argv);
  logvisor::CreateWin32Console();
  return main(argc, argv);
}
#endif

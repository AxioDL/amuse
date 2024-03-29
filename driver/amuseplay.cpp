#include "amuse/amuse.hpp"
#include "amuse/BooBackend.hpp"
#include "boo/boo.hpp"
#include "logvisor/logvisor.hpp"

#include <map>

#if _WIN32
#include <nowide/args.hpp>
#endif

#define EMITTER_TEST 0

static logvisor::Module Log("amuseplay");

struct AppCallback;

struct EventCallback : boo::IWindowCallback {
  AppCallback& m_app;
  bool m_tracking = false;

public:
  void charKeyDown(unsigned long charCode, boo::EModifierKey mods, bool isRepeat) override;
  void charKeyUp(unsigned long charCode, boo::EModifierKey mods) override;
  void specialKeyDown(boo::ESpecialKey key, boo::EModifierKey mods, bool isRepeat) override;
  void specialKeyUp(boo::ESpecialKey key, boo::EModifierKey mods) override;
  void resized(const boo::SWindowRect&, bool) override {}

  void mouseDown(const boo::SWindowCoord&, boo::EMouseButton, boo::EModifierKey) override;
  void mouseUp(const boo::SWindowCoord&, boo::EMouseButton, boo::EModifierKey) override;
  void mouseMove(const boo::SWindowCoord& coord) override;

  EventCallback(AppCallback& app) : m_app(app) {}
};

struct AppCallback : boo::IApplicationCallback {
  int m_argc;
  char** m_argv;

  /* Boo window and events */
  EventCallback m_eventRec;
  boo::DeferredWindowEvents<EventCallback> m_events;
  std::shared_ptr<boo::IWindow> m_win;

  /* Amuse engine */
  std::unique_ptr<amuse::BooBackendVoiceAllocator> m_booBackend;
  std::unique_ptr<amuse::Engine> m_engine;
  int m_groupId = -1;
  bool m_sfxGroup;

  /* Song playback selection */
  int m_setupId = -1;
  int m_chanId = 0;
  int8_t m_octave = 4;
  int8_t m_velocity = 64;
  amuse::ObjToken<amuse::Sequencer> m_seq;
  amuse::ContainerRegistry::SongData* m_arrData = nullptr;

  /* SFX playback selection */
  int m_sfxId = -1;
  amuse::ObjToken<amuse::Voice> m_vox;
  size_t m_lastVoxCount = 0;
  int8_t m_lastChanProg = -1;

  /* Control state */
  float m_volume = 0.8f;
  float m_modulation = 0.f;
  float m_pitchBend = 0.f;
  bool m_updateDisp = false;
  bool m_running = true;
  bool m_wantsNext = false;
  bool m_wantsPrev = false;
  bool m_breakout = false;
  int m_panicCount = 0;

#if EMITTER_TEST
  amuse::Vector3f m_pos = {};
  amuse::Vector3f m_dir = {0.f, 0.f, 0.f};
  amuse::Vector3f m_listenerDir = {0.f, 40.f, 0.f};
  std::shared_ptr<amuse::Emitter> m_emitter;
  std::shared_ptr<amuse::Listener> m_listener;
#endif

  void UpdateSongDisplay() {
    size_t voxCount = 0;
    int program = 0;
    if (m_seq) {
      voxCount = m_seq->getVoiceCount();
      program = m_seq->getChanProgram(m_chanId);
    }
    fmt::print(FMT_STRING(
        "\r                                                                                "
        "\r  {} Setup {}, Chan {}, Prog {}, Octave: {}, Vel: {}, VOL: {}%\r"),
        voxCount, m_setupId, m_chanId, program, m_octave, m_velocity, int(std::rint(m_volume * 100)));
    fflush(stdout);
  }

  void SelectSong(int setupId) {
    m_setupId = setupId;
    if (m_seq) {
      m_seq->stopSong(0.f, true);
      m_seq->kill();
    }
    m_seq = m_engine->seqPlay(m_groupId, setupId, nullptr);
    m_engine->setVolume(m_volume);

    if (m_arrData)
      m_seq->playSong(m_arrData->m_data.get(), false);

    UpdateSongDisplay();
  }

  void SongLoop(const amuse::SongGroupIndex& index) {
    fmt::print(FMT_STRING(
        "░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░\n"
        "░░░   ████ ████  ┃  ████ ████ ████   ┃   ████ ████  ░░░\n"
        "░░░   ████ ████  ┃  ████ ████ ████   ┃   ████ ████  ░░░\n"
        "░░░   ▌W▐█ ▌E▐█  ┃  ▌T▐█ ▌Y▐█ ▌U▐█   ┃   ▌O▐█ ▌P▐█  ░░░\n"
        "░░░    │    │    ┃    │    │    │    ┃    │    │    ░░░\n"
        "░░░ A  │ S  │ D  ┃ F  │ G  │ H  │ J  ┃ K  │ L  │ ;  ░░░\n"
        "░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░\n"
        "<left/right>: cycle MIDI setup, <up/down>: volume, <space>: PANIC\n"
        "<tab>: sustain pedal, <window-Y>: pitch wheel, <window-X>: mod wheel\n"
        "<Z/X>: octave, <C/V>: velocity, <B/N>: channel, <,/.>: program, <Q>: quit\n"));

    std::map<amuse::SongId, std::array<amuse::SongGroupIndex::MIDISetup, 16>> sortEntries(index.m_midiSetups.cbegin(),
                                                                                          index.m_midiSetups.cend());
    auto setupIt = sortEntries.cbegin();
    if (setupIt != sortEntries.cend()) {
      if (m_setupId == -1)
        SelectSong(setupIt->first.id);
      else {
        while (setupIt != sortEntries.cend() && setupIt->first != m_setupId)
          ++setupIt;
        if (setupIt == sortEntries.cend())
          setupIt = sortEntries.cbegin();
        SelectSong(setupIt->first.id);
      }
    }

    while (m_running) {
      m_events.dispatchEvents();

      if (m_wantsNext) {
        m_wantsNext = false;
        auto nextIt = setupIt;
        ++nextIt;
        if (nextIt != sortEntries.cend()) {
          ++setupIt;
          SelectSong(setupIt->first.id);
          m_updateDisp = false;
        }
      }

      if (m_wantsPrev) {
        m_wantsPrev = false;
        if (setupIt != sortEntries.cbegin()) {
          --setupIt;
          SelectSong(setupIt->first.id);
          m_updateDisp = false;
        }
      }

      if (m_seq && m_panicCount) {
        if (m_panicCount > 1) {
          m_seq->allOff(true);
          m_panicCount = 0;
        } else
          m_seq->allOff(false);
        m_updateDisp = true;
      }

      if (m_updateDisp) {
        m_updateDisp = false;
        UpdateSongDisplay();
      }

      m_win->waitForRetrace();

      size_t voxCount;
      int8_t progId;
      if (m_seq) {
        voxCount = m_seq->getVoiceCount();
        progId = m_seq->getChanProgram(m_chanId);
      } else {
        voxCount = 0;
        progId = -1;
      }

      if (m_lastVoxCount != voxCount || m_lastChanProg != progId) {
        m_lastVoxCount = voxCount;
        m_lastChanProg = progId;
        UpdateSongDisplay();
      }

      if (m_breakout) {
        m_breakout = false;
        m_seq->allOff(true);
        m_seq.reset();
        break;
      }
    }
  }

  void UpdateSFXDisplay() {
    bool playing = m_vox && m_vox->state() == amuse::VoiceState::Playing;
#if EMITTER_TEST
    printf(
        "\r                                                                                "
        "\r  %c SFX %d, VOL: %d%% POS: (%f,%f)\r",
        playing ? '>' : ' ', m_sfxId, int(std::rint(m_volume * 100)), m_pos[0], m_pos[1]);
#else
    fmt::print(FMT_STRING(
        "\r                                                                                "
        "\r  {:c} SFX {}, VOL: {}%\r"),
        playing ? '>' : ' ', m_sfxId, int(std::rint(m_volume * 100)));
#endif
    fflush(stdout);
  }

  void SelectSFX(int sfxId) {
    m_sfxId = sfxId;

    bool playing = m_vox && m_vox->state() == amuse::VoiceState::Playing;
    if (playing) {
#if EMITTER_TEST
      if (m_emitter)
        m_emitter->getVoice()->keyOff();
      m_emitter = m_engine->addEmitter(m_pos, m_dir, 100.f, 0.f, m_sfxId, 0.f, 1.f, true);
      m_vox = m_emitter->getVoice();
#else
      m_vox->keyOff();
      m_vox = m_engine->fxStart(m_sfxId, m_volume, 0.f);
#endif
    }

    UpdateSFXDisplay();
  }

  void SFXLoop(const amuse::SFXGroupIndex& index) {
    fmt::print(FMT_STRING("<space>: keyon/keyoff, <left/right>: cycle SFX, <up/down>: volume, <Q>: quit\n"));

    m_seq = m_engine->seqPlay(m_groupId, 0, nullptr);

    std::map<amuse::SFXId, amuse::SFXGroupIndex::SFXEntry> sortEntries(index.m_sfxEntries.cbegin(),
                                                                       index.m_sfxEntries.cend());
    auto sfxIt = sortEntries.cbegin();
    if (sfxIt != sortEntries.cend())
      SelectSFX(sfxIt->first.id);

#if EMITTER_TEST
    float emitterTheta = 0.f;
    float zeroVec[3] = {};
    float heading[3] = {0.f, 1.f, 0.f};
    float up[3] = {0.f, 0.f, 1.f};
    m_listener = m_engine->addListener(zeroVec, m_listenerDir, heading, up, 5.f, 5.f, 1000.f, 1.f);
#endif

    while (m_running) {
#if EMITTER_TEST
      // float dist = std::sin(emitterTheta * 0.25f);
      m_pos[0] = std::cos(emitterTheta) * 5.f;
      m_pos[1] = std::sin(emitterTheta) * 5.f;
      if (m_emitter)
        m_emitter->setVectors(m_pos, m_dir);
      emitterTheta += 1.f / 60.f;
      m_updateDisp = true;
#endif

      m_events.dispatchEvents();

      if (m_wantsNext) {
        m_wantsNext = false;
        auto nextIt = sfxIt;
        ++nextIt;
        if (nextIt != sortEntries.cend()) {
          ++sfxIt;
          SelectSFX(sfxIt->first.id);
          m_updateDisp = false;
        }
      }

      if (m_wantsPrev) {
        m_wantsPrev = false;
        if (sfxIt != sortEntries.cbegin()) {
          --sfxIt;
          SelectSFX(sfxIt->first.id);
          m_updateDisp = false;
        }
      }

      if (m_updateDisp) {
        m_updateDisp = false;
        UpdateSFXDisplay();
      }

      m_win->waitForRetrace();

      if (m_vox && m_vox->state() == amuse::VoiceState::Dead) {
        m_vox.reset();
#if EMITTER_TEST
        m_emitter.reset();
#endif
        UpdateSFXDisplay();
      }

      if (m_breakout) {
        m_breakout = false;
        m_vox.reset();
#if EMITTER_TEST
        m_emitter.reset();
#endif
        m_seq->allOff(true);
        m_seq.reset();
        break;
      }
    }

#if EMITTER_TEST
    m_engine->removeListener(m_listener.get());
    m_listener.reset();
#endif
  }

  void charKeyDownRepeat(unsigned long charCode) {
    charCode = tolower(charCode);

    if (m_seq && m_chanId != -1) {
      switch (charCode) {
      case 'z':
        m_octave = std::clamp(-1, m_octave - 1, 8);
        m_updateDisp = true;
        break;
      case 'x':
        m_octave = std::clamp(-1, m_octave + 1, 8);
        m_updateDisp = true;
        break;
      case 'c':
        m_velocity = std::clamp(0, m_velocity - 1, 127);
        m_updateDisp = true;
        break;
      case 'v':
        m_velocity = std::clamp(0, m_velocity + 1, 127);
        m_updateDisp = true;
        break;
      case 'b':
        m_chanId = std::clamp(0, m_chanId - 1, 15);
        m_updateDisp = true;
        break;
      case 'n':
        m_chanId = std::clamp(0, m_chanId + 1, 15);
        m_updateDisp = true;
        break;
      case ',':
        m_seq->prevChanProgram(m_chanId);
        m_updateDisp = true;
        break;
      case '.':
        m_seq->nextChanProgram(m_chanId);
        m_updateDisp = true;
        break;
      default:
        break;
      }
    }
  }

  void charKeyDown(unsigned long charCode) {
    charCode = tolower(charCode);
    if (charCode == 'q') {
      m_running = false;
      return;
    }

    if (m_sfxGroup) {
      switch (charCode) {
      case ' ':
        if (m_seq)
          m_seq->allOff(true);
        if (m_vox && m_vox->state() == amuse::VoiceState::Playing)
          m_vox->keyOff();
        else if (m_sfxId != -1) {
#if EMITTER_TEST
          m_emitter = m_engine->addEmitter(m_pos, m_dir, 100.f, 0.f, m_sfxId, 0.f, 1.f, true);
          m_vox = m_emitter->getVoice();
#else
          m_vox = m_engine->fxStart(m_sfxId, m_volume, 0.f);
#endif
        }
        m_updateDisp = true;
        break;
      default:
        break;
      }
    } else if (m_seq && m_chanId != -1) {
      bool setPanic = false;

      switch (charCode) {
      case ' ':
        ++m_panicCount;
        setPanic = true;
        break;
      case 'z':
        m_octave = std::clamp(-1, m_octave - 1, 8);
        m_updateDisp = true;
        break;
      case 'x':
        m_octave = std::clamp(-1, m_octave + 1, 8);
        m_updateDisp = true;
        break;
      case 'c':
        m_velocity = std::clamp(0, m_velocity - 1, 127);
        m_updateDisp = true;
        break;
      case 'v':
        m_velocity = std::clamp(0, m_velocity + 1, 127);
        m_updateDisp = true;
        break;
      case 'b':
        m_chanId = std::clamp(0, m_chanId - 1, 15);
        m_updateDisp = true;
        break;
      case 'n':
        m_chanId = std::clamp(0, m_chanId + 1, 15);
        m_updateDisp = true;
        break;
      case ',':
        m_seq->prevChanProgram(m_chanId);
        m_updateDisp = true;
        break;
      case '.':
        m_seq->nextChanProgram(m_chanId);
        m_updateDisp = true;
        break;
      case '\t':
        m_seq->setCtrlValue(m_chanId, 64, 127);
        break;
      case 'a':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12, m_velocity);
        break;
      case 'w':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 1, m_velocity);
        break;
      case 's':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 2, m_velocity);
        break;
      case 'e':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 3, m_velocity);
        break;
      case 'd':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 4, m_velocity);
        break;
      case 'f':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 5, m_velocity);
        break;
      case 't':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 6, m_velocity);
        break;
      case 'g':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 7, m_velocity);
        break;
      case 'y':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 8, m_velocity);
        break;
      case 'h':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 9, m_velocity);
        break;
      case 'u':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 10, m_velocity);
        break;
      case 'j':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 11, m_velocity);
        break;
      case 'k':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 12, m_velocity);
        break;
      case 'o':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 13, m_velocity);
        break;
      case 'l':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 14, m_velocity);
        break;
      case 'p':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 15, m_velocity);
        break;
      case ';':
      case ':':
        m_seq->keyOn(m_chanId, (m_octave + 1) * 12 + 16, m_velocity);
        break;
      default:
        break;
      }

      if (!setPanic)
        m_panicCount = 0;
    }
  }

  void charKeyUp(unsigned long charCode) {
    charCode = tolower(charCode);

    if (!m_sfxGroup && m_chanId != -1) {
      switch (charCode) {
      case '\t':
        m_seq->setCtrlValue(m_chanId, 64, 0);
        break;
      case 'a':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12, m_velocity);
        break;
      case 'w':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 1, m_velocity);
        break;
      case 's':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 2, m_velocity);
        break;
      case 'e':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 3, m_velocity);
        break;
      case 'd':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 4, m_velocity);
        break;
      case 'f':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 5, m_velocity);
        break;
      case 't':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 6, m_velocity);
        break;
      case 'g':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 7, m_velocity);
        break;
      case 'y':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 8, m_velocity);
        break;
      case 'h':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 9, m_velocity);
        break;
      case 'u':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 10, m_velocity);
        break;
      case 'j':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 11, m_velocity);
        break;
      case 'k':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 12, m_velocity);
        break;
      case 'o':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 13, m_velocity);
        break;
      case 'l':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 14, m_velocity);
        break;
      case 'p':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 15, m_velocity);
        break;
      case ';':
      case ':':
        m_seq->keyOff(m_chanId, (m_octave + 1) * 12 + 16, m_velocity);
        break;
      default:
        break;
      }
    }
  }

  int appMain(boo::IApplication* app) override {
    /* Event window */
    m_win = app->newWindow("amuseplay");
    m_win->setCallback(&m_events);
    m_win->setWindowFrame(100, 100, 100, 100);
    m_win->setStyle(~boo::EWindowStyle::Resize);
    m_win->showWindow();
    boo::ObjToken<boo::ITextureR> tex;
    m_win->getMainContextDataFactory()->commitTransaction([&](boo::IGraphicsDataFactory::Context& ctx) {
      tex = ctx.newRenderTexture(100, 100, boo::TextureClampMode::Repeat, 1, 0);
      return true;
    } BooTrace);
    boo::IGraphicsCommandQueue* q = m_win->getCommandQueue();
    q->setRenderTarget(tex);
    q->clearTarget();
    q->resolveDisplay(tex);
    q->execute();

    /* Load data */
    if (m_argc < 2) {
      Log.report(logvisor::Error, FMT_STRING("needs group path argument"));
      return 1;
    }

    amuse::ContainerRegistry::Type cType = amuse::ContainerRegistry::DetectContainerType(m_argv[1]);
    if (cType == amuse::ContainerRegistry::Type::Invalid) {
      Log.report(logvisor::Error, FMT_STRING("invalid/no data at path argument"));
      return 1;
    }
    Log.report(logvisor::Info, FMT_STRING("Found '%s' Audio Group data"), amuse::ContainerRegistry::TypeToName(cType));

    std::vector<std::pair<std::string, amuse::IntrusiveAudioGroupData>> data =
        amuse::ContainerRegistry::LoadContainer(m_argv[1]);
    if (data.empty()) {
      Log.report(logvisor::Error, FMT_STRING("invalid/no data at path argument"));
      return 1;
    }

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

    while (m_running) {
      m_groupId = -1;
      m_setupId = -1;

      /* Attempt loading song */
      std::vector<std::pair<std::string, amuse::ContainerRegistry::SongData>> songs;
      if (m_argc > 2)
        songs = amuse::ContainerRegistry::LoadSongs(m_argv[2]);
      else
        songs = amuse::ContainerRegistry::LoadSongs(m_argv[1]);

      if (songs.size()) {
        bool play = true;
        if (m_argc <= 2) {
          bool prompt = true;
          while (true) {
            if (prompt) {
              fmt::print(FMT_STRING("Play Song? (Y/N): "));
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
            } else {
              Log.report(logvisor::Error, FMT_STRING("unable to find Song {}"), userSel);
              return 1;
            }
          } else if (songs.size() == 1) {
            m_arrData = &songs[0].second;
            m_groupId = m_arrData->m_groupId;
            m_setupId = m_arrData->m_setupId;
          }
        }
      }

      /* Get group selection via setup search */
      if (m_groupId == -1 && m_setupId != -1) {
        for (const auto& pair : allSongGroups) {
          for (const auto& setup : pair.second.second->m_midiSetups) {
            if (setup.first == m_setupId) {
              m_groupId = pair.first.id;
              break;
            }
          }
          if (m_groupId != -1)
            break;
        }
      }

      /* Get group selection via user */
      if (m_groupId != -1) {
        if (allSongGroups.find(m_groupId) != allSongGroups.end())
          m_sfxGroup = false;
        else if (allSFXGroups.find(m_groupId) != allSFXGroups.end())
          m_sfxGroup = true;
        else {
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

        if (allSongGroups.find(userSel) != allSongGroups.end()) {
          m_groupId = userSel;
          m_sfxGroup = false;
        } else if (allSFXGroups.find(userSel) != allSFXGroups.end()) {
          m_groupId = userSel;
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
          m_sfxGroup = false;
        } else {
          const auto& pair = *allSFXGroups.cbegin();
          m_groupId = pair.first.id;
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

      /* Build voice engine */
      std::unique_ptr<boo::IAudioVoiceEngine> voxEngine = boo::NewAudioVoiceEngine();
      m_booBackend.reset(new amuse::BooBackendVoiceAllocator(*voxEngine));
      m_engine.reset(new amuse::Engine(*m_booBackend, amuse::AmplitudeMode::PerSample));

      /* Load group into engine */
      const amuse::AudioGroup* group = m_engine->addAudioGroup(*selData);
      if (!group) {
        Log.report(logvisor::Error, FMT_STRING("unable to add audio group"));
        return 1;
      }

      /* Enter playback loop */
      if (m_sfxGroup)
        SFXLoop(*sfxIndex);
      else
        SongLoop(*songIndex);

      m_vox.reset();
      m_seq.reset();
      m_engine.reset();
      m_booBackend.reset();
      fmt::print(FMT_STRING("\n\n"));
    }

    return 0;
  }

  void appQuitting(boo::IApplication*) override { m_running = false; }

  AppCallback(int argc, char** argv)
  : m_argc(argc), m_argv(argv), m_eventRec(*this), m_events(m_eventRec) {}
};

void EventCallback::charKeyDown(unsigned long charCode, boo::EModifierKey mods, bool isRepeat) {
  if (isRepeat)
    m_app.charKeyDownRepeat(charCode);
  else
    m_app.charKeyDown(charCode);
}

void EventCallback::charKeyUp(unsigned long charCode, boo::EModifierKey mods) { m_app.charKeyUp(charCode); }

void EventCallback::specialKeyDown(boo::ESpecialKey key, boo::EModifierKey mods, bool isRepeat) {
  switch (key) {
  case boo::ESpecialKey::Left:
    m_app.m_wantsPrev = true;
    break;
  case boo::ESpecialKey::Right:
    m_app.m_wantsNext = true;
    break;
  case boo::ESpecialKey::Up:
    if (m_app.m_volume < 1.f)
      m_app.m_volume = std::clamp(0.f, m_app.m_volume + 0.05f, 1.f);
    m_app.m_engine->setVolume(m_app.m_volume);
    m_app.m_updateDisp = true;
    break;
  case boo::ESpecialKey::Down:
    if (m_app.m_volume > 0.f)
      m_app.m_volume = std::clamp(0.f, m_app.m_volume - 0.05f, 1.f);
    m_app.m_engine->setVolume(m_app.m_volume);
    m_app.m_updateDisp = true;
    break;
  case boo::ESpecialKey::Esc:
    m_app.m_breakout = true;
    break;
  default:
    break;
  }
}

void EventCallback::specialKeyUp(boo::ESpecialKey key, boo::EModifierKey mods) {}

void EventCallback::mouseDown(const boo::SWindowCoord& coord, boo::EMouseButton, boo::EModifierKey) {
  m_tracking = true;
  m_app.m_pitchBend = std::clamp(-1.f, coord.norm[1] * 2.f - 1.f, 1.f);
  if (m_app.m_vox)
    m_app.m_vox->setPitchWheel(m_app.m_pitchBend);
  if (m_app.m_seq && m_app.m_chanId != -1)
    m_app.m_seq->setPitchWheel(m_app.m_chanId, m_app.m_pitchBend);
}

void EventCallback::mouseUp(const boo::SWindowCoord&, boo::EMouseButton, boo::EModifierKey) {
  m_tracking = false;
  m_app.m_pitchBend = 0.f;
  if (m_app.m_vox)
    m_app.m_vox->setPitchWheel(m_app.m_pitchBend);
  if (m_app.m_seq && m_app.m_chanId != -1)
    m_app.m_seq->setPitchWheel(m_app.m_chanId, m_app.m_pitchBend);
}

void EventCallback::mouseMove(const boo::SWindowCoord& coord) {
  if (m_tracking) {
    m_app.m_modulation = std::clamp(0.f, coord.norm[0], 1.f);
    if (m_app.m_vox)
      m_app.m_vox->setCtrlValue(1, m_app.m_modulation * 127.f);
    if (m_app.m_seq && m_app.m_chanId != -1)
      m_app.m_seq->setCtrlValue(m_app.m_chanId, 1, m_app.m_modulation * 127.f);

    m_app.m_pitchBend = std::clamp(-1.f, coord.norm[1] * 2.f - 1.f, 1.f);
    if (m_app.m_vox)
      m_app.m_vox->setPitchWheel(m_app.m_pitchBend);
    if (m_app.m_seq && m_app.m_chanId != -1)
      m_app.m_seq->setPitchWheel(m_app.m_chanId, m_app.m_pitchBend);
  }
}

int main(int argc, char** argv) {
  logvisor::RegisterConsoleLogger();
  logvisor::RegisterStandardExceptions();
  AppCallback app(argc, argv);
  int ret = boo::ApplicationRun(boo::IApplication::EPlatformType::Auto, app, "amuseplay",
                                "Amuse Player", argc, argv, {}, 1, 1, false);
  return ret;
}

#if _WIN32
#include <shellapi.h>

int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR lpCmdLine, int) {
  int argc = 0;
  char** argv = nullptr;
  nowide::args _(argc, argv);
  logvisor::CreateWin32Console();
  return main(argc, argv);
}
#endif

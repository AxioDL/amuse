#include "amuse/amuse.hpp"
#include "amuse/BooBackend.hpp"
#include "athena/FileReader.hpp"
#include "boo/boo.hpp"
#include "boo/audiodev/IAudioVoiceEngine.hpp"
#include "logvisor/logvisor.hpp"
#include "optional.hpp"
#include <stdio.h>
#include <string.h>
#include <thread>
#include <map>

static logvisor::Module Log("amuseplay");

static amuse::IntrusiveAudioGroupData LoadFromArgs(int argc, const boo::SystemChar** argv,
                                                   std::string& descOut, bool& good)
{
    if (argc > 1)
    {
        /* First attempt single-file case */
        std::experimental::optional<athena::io::FileReader> r;
        r.emplace(argv[1], 32 * 1024, false);
        if (!r->hasError())
        {
            char testBuf[6];
            if (r->readBytesToBuf(testBuf, 6) == 6)
            {
                if (!memcmp(testBuf, "Audio/", 6))
                {
                    /* Metroid Prime 1 container */
                    r->seek(0, athena::SeekOrigin::Begin);
                    r->readString();
                    descOut = "Metroid Prime (" + r->readString() + ")";

                    std::unique_ptr<uint8_t[]> pool = r->readUBytes(r->readUint32Big());
                    std::unique_ptr<uint8_t[]> proj = r->readUBytes(r->readUint32Big());
                    std::unique_ptr<uint8_t[]> samp = r->readUBytes(r->readUint32Big());
                    std::unique_ptr<uint8_t[]> sdir = r->readUBytes(r->readUint32Big());

                    good = true;
                    return {proj.release(), pool.release(), sdir.release(), samp.release()};
                }
                else if (amuse::SBig(*reinterpret_cast<uint32_t*>(testBuf)) == 0x1)
                {
                    /* Metroid Prime 2 container */
                    r->seek(0, athena::SeekOrigin::Begin);
                    r->readUint32Big();
                    descOut = "Metroid Prime 2 (" + r->readString() + ")";

                    r->readUint16Big();
                    uint32_t poolSz = r->readUint32Big();
                    uint32_t projSz = r->readUint32Big();
                    uint32_t sdirSz = r->readUint32Big();
                    uint32_t sampSz = r->readUint32Big();

                    std::unique_ptr<uint8_t[]> pool = r->readUBytes(poolSz);
                    std::unique_ptr<uint8_t[]> proj = r->readUBytes(projSz);
                    std::unique_ptr<uint8_t[]> sdir = r->readUBytes(sdirSz);
                    std::unique_ptr<uint8_t[]> samp = r->readUBytes(sampSz);

                    good = true;
                    return {proj.release(), pool.release(), sdir.release(), samp.release()};
                }
            }
        }
        else
        {
            /* Now attempt multi-file case */
            char path[1024];

            /* Project */
            snprintf(path, 1024, "%s.pro", argv[1]);
            r.emplace(path, 32 * 1024, false);
            if (r->hasError())
            {
                snprintf(path, 1024, "%s.proj", argv[1]);
                r.emplace(path, 32 * 1024, false);
                if (r->hasError())
                    return {nullptr, nullptr, nullptr, nullptr};
            }
            std::unique_ptr<uint8_t[]> proj = r->readUBytes(r->length());

            /* Pool */
            snprintf(path, 1024, "%s.poo", argv[1]);
            r.emplace(path, 32 * 1024, false);
            if (r->hasError())
            {
                snprintf(path, 1024, "%s.pool", argv[1]);
                r.emplace(path, 32 * 1024, false);
                if (r->hasError())
                    return {nullptr, nullptr, nullptr, nullptr};
            }
            std::unique_ptr<uint8_t[]> pool = r->readUBytes(r->length());

            /* Sdir */
            snprintf(path, 1024, "%s.sdi", argv[1]);
            r.emplace(path, 32 * 1024, false);
            if (r->hasError())
            {
                snprintf(path, 1024, "%s.sdir", argv[1]);
                r.emplace(path, 32 * 1024, false);
                if (r->hasError())
                    return {nullptr, nullptr, nullptr, nullptr};
            }
            std::unique_ptr<uint8_t[]> sdir = r->readUBytes(r->length());

            /* Samp */
            snprintf(path, 1024, "%s.sam", argv[1]);
            r.emplace(path, 32 * 1024, false);
            if (r->hasError())
            {
                snprintf(path, 1024, "%s.samp", argv[1]);
                r.emplace(path, 32 * 1024, false);
                if (r->hasError())
                    return {nullptr, nullptr, nullptr, nullptr};
            }
            std::unique_ptr<uint8_t[]> samp = r->readUBytes(r->length());

            descOut = "4 RAW Chunk Files";
            good = true;
            return {proj.release(), pool.release(), sdir.release(), samp.release()};
        }
    }

    return {nullptr, nullptr, nullptr, nullptr};
}

struct AppCallback;

struct EventCallback : boo::IWindowCallback
{
    AppCallback& m_app;
    bool m_tracking = false;
public:
    void charKeyDown(unsigned long charCode, boo::EModifierKey mods, bool isRepeat);
    void charKeyUp(unsigned long charCode, boo::EModifierKey mods);
    void specialKeyDown(boo::ESpecialKey key, boo::EModifierKey mods, bool isRepeat);
    void specialKeyUp(boo::ESpecialKey key, boo::EModifierKey mods);
    void resized(const boo::SWindowRect&, const boo::SWindowRect&) {}

    void mouseDown(const boo::SWindowCoord&, boo::EMouseButton, boo::EModifierKey);
    void mouseUp(const boo::SWindowCoord&, boo::EMouseButton, boo::EModifierKey);
    void mouseMove(const boo::SWindowCoord& coord);

    EventCallback(AppCallback& app) : m_app(app) {}
};

struct AppCallback : boo::IApplicationCallback
{
    int m_argc;
    const boo::SystemChar** m_argv;

    /* Boo window and events */
    EventCallback m_eventRec;
    boo::DeferredWindowEvents<EventCallback> m_events;
    boo::IWindow* m_win = nullptr;

    /* Amuse engine */
    std::experimental::optional<amuse::Engine> m_engine;
    int m_groupId;
    bool m_sfxGroup;

    /* Song playback selection */
    int m_setupId = -1;
    int m_chanId = 0;
    int8_t m_octave = 4;
    int8_t m_velocity = 64;
    std::shared_ptr<amuse::Sequencer> m_seq;

    /* SFX playback selection */
    int m_sfxId = -1;
    std::shared_ptr<amuse::Voice> m_vox;
    size_t m_lastVoxCount = 0;

    /* Control state */
    float m_volume = 0.5f;
    float m_modulation = 0.f;
    float m_pitchBend = 0.f;
    bool m_updateDisp = false;
    bool m_running = true;
    bool m_wantsNext = false;
    bool m_wantsPrev = false;
    int m_panicCount = 0;

    void UpdateSongDisplay()
    {
        size_t voxCount = 0;
        int program = 0;
        if (m_seq)
        {
            voxCount = m_seq->getVoiceCount();
            program = m_seq->getChanProgram(m_chanId);
        }
        printf("\r                                                                                "
               "\r  %" PRISize " Setup %d, Chan %d, Prog %d, Octave: %d, Vel: %d, VOL: %d%%\r", voxCount,
               m_setupId, m_chanId, program, m_octave, m_velocity, int(std::rint(m_volume * 100)));
        fflush(stdout);
    }

    void SelectSong(int setupId)
    {
        m_setupId = setupId;
        if (m_seq)
            m_seq->allOff();
        m_seq = m_engine->seqPlay(m_groupId, setupId, nullptr);
        m_seq->setVolume(m_volume);
        UpdateSongDisplay();
    }

    void SongLoop(const amuse::AudioGroup& group,
                  const amuse::SongGroupIndex& index)
    {
        printf("░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░\n"
               "░░░   ████ ████  ┃  ████ ████ ████   ┃   ████ ████  ░░░\n"
               "░░░   ████ ████  ┃  ████ ████ ████   ┃   ████ ████  ░░░\n"
               "░░░   ▌W▐█ ▌E▐█  ┃  ▌T▐█ ▌Y▐█ ▌U▐█   ┃   ▌O▐█ ▌P▐█  ░░░\n"
               "░░░    │    │    ┃    │    │    │    ┃    │    │    ░░░\n"
               "░░░ A  │ S  │ D  ┃ F  │ G  │ H  │ J  ┃ K  │ L  │ ;  ░░░\n"
               "░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░░\n"
               "<left/right>: cycle MIDI setup / channel, <up/down>: volume, <space>: PANIC\n"
               "<tab>: sustain pedal, <window-Y>: pitch wheel, <window-X>: mod wheel\n"
               "<Z/X>: octave, <C/V>: velocity, <B/N>: channel, <,/.>: program, <Q>: quit\n");

        std::map<int, const std::array<amuse::SongGroupIndex::MIDISetup, 16>*> sortEntries
            (index.m_midiSetups.cbegin(), index.m_midiSetups.cend());
        auto setupIt = sortEntries.cbegin();
        if (setupIt != sortEntries.cend())
            SelectSong(setupIt->first);

        while (m_running)
        {
            m_events.dispatchEvents();

            if (m_wantsNext)
            {
                m_wantsNext = false;
                auto nextIt = setupIt;
                ++nextIt;
                if (nextIt != sortEntries.cend())
                {
                    ++setupIt;
                    SelectSong(setupIt->first);
                    m_updateDisp = false;
                }
            }

            if (m_wantsPrev)
            {
                m_wantsPrev = false;
                if (setupIt != sortEntries.cbegin())
                {
                    --setupIt;
                    SelectSong(setupIt->first);
                    m_updateDisp = false;
                }
            }

            if (m_seq && m_panicCount)
            {
                if (m_panicCount > 1)
                {
                    m_seq->allOff(true);
                    m_panicCount = 0;
                }
                else
                    m_seq->allOff(false);
                m_updateDisp = true;
            }

            if (m_updateDisp)
            {
                m_updateDisp = false;
                UpdateSongDisplay();
            }

            m_engine->pumpEngine();

            size_t voxCount;
            if (m_seq)
                voxCount = m_seq->getVoiceCount();
            else
                voxCount = 0;

            if (m_lastVoxCount != voxCount)
            {
                m_lastVoxCount = voxCount;
                UpdateSongDisplay();
            }

            m_win->waitForRetrace();
        }
    }

    void UpdateSFXDisplay()
    {
        bool playing = m_vox && m_vox->state() == amuse::VoiceState::Playing;
        printf("\r                                                                                "
               "\r  %c SFX %d, VOL: %d%%\r", playing ? '>' : ' ',
               m_sfxId, int(std::rint(m_volume * 100)));
        fflush(stdout);
    }

    void SelectSFX(int sfxId)
    {
        m_sfxId = sfxId;

        bool playing = m_vox && m_vox->state() == amuse::VoiceState::Playing;
        if (playing)
        {
            m_vox->keyOff();
            m_vox = m_engine->fxStart(m_sfxId, m_volume, 0.f);
        }

        UpdateSFXDisplay();
    }

    void SFXLoop(const amuse::AudioGroup& group,
                 const amuse::SFXGroupIndex& index)
    {
        printf("<space>: keyon/keyoff, <left/right>: cycle SFX, <up/down>: volume, <Q>: quit\n");

        std::map<uint16_t, const amuse::SFXGroupIndex::SFXEntry*> sortEntries
            (index.m_sfxEntries.cbegin(), index.m_sfxEntries.cend());
        auto sfxIt = sortEntries.cbegin();
        if (sfxIt != sortEntries.cend())
            SelectSFX(sfxIt->first);

        while (m_running)
        {
            m_events.dispatchEvents();

            if (m_wantsNext)
            {
                m_wantsNext = false;
                auto nextIt = sfxIt;
                ++nextIt;
                if (nextIt != sortEntries.cend())
                {
                    ++sfxIt;
                    SelectSFX(sfxIt->first);
                    m_updateDisp = false;
                }
            }

            if (m_wantsPrev)
            {
                m_wantsPrev = false;
                if (sfxIt != sortEntries.cbegin())
                {
                    --sfxIt;
                    SelectSFX(sfxIt->first);
                    m_updateDisp = false;
                }
            }

            if (m_updateDisp)
            {
                m_updateDisp = false;
                UpdateSFXDisplay();
            }

            m_engine->pumpEngine();

            if (m_vox && m_vox->state() == amuse::VoiceState::Dead)
            {
                m_vox.reset();
                UpdateSFXDisplay();
            }

            m_win->waitForRetrace();
        }
    }

    void charKeyDownRepeat(unsigned long charCode)
    {
        charCode = tolower(charCode);

        if (m_seq && m_chanId != -1)
        {
            switch (charCode)
            {
            case 'z':
                m_octave = amuse::clamp(-1, m_octave - 1, 8);
                m_updateDisp = true;
                break;
            case 'x':
                m_octave = amuse::clamp(-1, m_octave + 1, 8);
                m_updateDisp = true;
                break;
            case 'c':
                m_velocity = amuse::clamp(0, m_velocity - 1, 127);
                m_updateDisp = true;
                break;
            case 'v':
                m_velocity = amuse::clamp(0, m_velocity + 1, 127);
                m_updateDisp = true;
                break;
            case 'b':
                m_chanId = amuse::clamp(0, m_chanId - 1, 15);
                m_updateDisp = true;
                break;
            case 'n':
                m_chanId = amuse::clamp(0, m_chanId + 1, 15);
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
            }
        }
    }

    void charKeyDown(unsigned long charCode)
    {
        charCode = tolower(charCode);
        if (charCode == 'q')
        {
            m_running = false;
            return;
        }

        if (m_sfxGroup)
        {
            switch (charCode)
            {
            case ' ':
                if (m_vox && m_vox->state() == amuse::VoiceState::Playing)
                    m_vox->keyOff();
                else if (m_sfxId != -1)
                    m_vox = m_engine->fxStart(m_sfxId, m_volume, 0.f);
                m_updateDisp = true;
            default: break;
            }
        }
        else if (m_seq && m_chanId != -1)
        {
            bool setPanic = false;

            switch (charCode)
            {
            case ' ':
                ++m_panicCount;
                setPanic = true;
                break;
            case 'z':
                m_octave = amuse::clamp(-1, m_octave - 1, 8);
                m_updateDisp = true;
                break;
            case 'x':
                m_octave = amuse::clamp(-1, m_octave + 1, 8);
                m_updateDisp = true;
                break;
            case 'c':
                m_velocity = amuse::clamp(0, m_velocity - 1, 127);
                m_updateDisp = true;
                break;
            case 'v':
                m_velocity = amuse::clamp(0, m_velocity + 1, 127);
                m_updateDisp = true;
                break;
            case 'b':
                m_chanId = amuse::clamp(0, m_chanId - 1, 15);
                m_updateDisp = true;
                break;
            case 'n':
                m_chanId = amuse::clamp(0, m_chanId + 1, 15);
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
            default: break;
            }

            if (!setPanic)
                m_panicCount = 0;
        }
    }

    void charKeyUp(unsigned long charCode)
    {
        charCode = tolower(charCode);

        if (!m_sfxGroup && m_chanId != -1)
        {
            switch (charCode)
            {
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
            default: break;
            }
        }
    }

    int appMain(boo::IApplication* app)
    {
        /* Event window */
        m_win = app->newWindow("amuseplay", 1);
        m_win->setCallback(&m_events);
        m_win->setWindowFrame(100, 100, 100, 100);
        m_win->setStyle(~boo::EWindowStyle::Resize);
        m_win->showWindow();
        boo::ITextureR* tex = nullptr;
        boo::GraphicsDataToken gfxToken =
        m_win->getMainContextDataFactory()->commitTransaction(
        [&](boo::IGraphicsDataFactory::Context& ctx) -> bool
        {
            tex = ctx.newRenderTexture(100, 100, false, false);
            return true;
        });
        boo::IGraphicsCommandQueue* q = m_win->getCommandQueue();
        q->setRenderTarget(tex);
        q->clearTarget();
        q->resolveDisplay(tex);
        q->execute();

        /* Load data */
        std::string desc;
        bool good = false;
        amuse::IntrusiveAudioGroupData data = LoadFromArgs(m_argc, m_argv, desc, good);
        if (!good)
            Log.report(logvisor::Fatal, "incomplete data in args");
        Log.report(logvisor::Info, "Found '%s' Audio Group data", desc.c_str());

        /* Load project to assemble group list */
        amuse::AudioGroupProject proj(data.getProj());

        /* Get group selection from user */
        size_t totalGroups = proj.sfxGroups().size() + proj.songGroups().size();
        if (totalGroups > 1)
        {
            /* Ask user to specify which group in project */
            printf("Multiple Audio Groups discovered:\n");
            for (const auto& pair : proj.songGroups())
            {
                printf("    %d (SongGroup)  %" PRISize " normal-pages, %" PRISize " drum-pages\n",
                       pair.first, pair.second.m_normPages.size(), pair.second.m_drumPages.size());
            }
            for (const auto& pair : proj.sfxGroups())
            {
                printf("    %d (SFXGroup)   %" PRISize " sfx-entries\n",
                       pair.first, pair.second.m_sfxEntries.size());
            }

            int userSel = 0;
            printf("Enter Group Number: ");
            if (scanf("%d", &userSel) <= 0)
                Log.report(logvisor::Fatal, "unable to parse prompt");

            if (proj.getSongGroupIndex(userSel))
            {
                m_groupId = userSel;
                m_sfxGroup = false;
            }
            else if (proj.getSFXGroupIndex(userSel))
            {
                m_groupId = userSel;
                m_sfxGroup = true;
            }
            else
                Log.report(logvisor::Fatal, "unable to find Group %d", userSel);
        }
        else if (totalGroups == 1)
        {
            /* Load one and only group */
            if (proj.songGroups().size())
            {
                const auto& pair = *proj.songGroups().cbegin();
                m_groupId = pair.first;
                m_sfxGroup = false;
            }
            else
            {
                const auto& pair = *proj.sfxGroups().cbegin();
                m_groupId = pair.first;
                m_sfxGroup = true;
            }
        }
        else
            Log.report(logvisor::Fatal, "empty project");

        /* Build voice engine */
        std::unique_ptr<boo::IAudioVoiceEngine> voxEngine = boo::NewAudioVoiceEngine();
        amuse::BooBackendVoiceAllocator booBackend(*voxEngine);
        m_engine.emplace(booBackend);

        /* Load group into engine */
        const amuse::AudioGroup* group = m_engine->addAudioGroup(data);
        if (!group)
            Log.report(logvisor::Fatal, "unable to add audio group");

        /* Enter playback loop */
        if (m_sfxGroup)
            SFXLoop(*group, *proj.getSFXGroupIndex(m_groupId));
        else
            SongLoop(*group, *proj.getSongGroupIndex(m_groupId));

        return 0;
    }

    void appQuitting(boo::IApplication*)
    {
        m_running = false;
    }

    AppCallback(int argc, const boo::SystemChar** argv)
    : m_argc(argc), m_argv(argv), m_eventRec(*this), m_events(m_eventRec) {}
};

void EventCallback::charKeyDown(unsigned long charCode, boo::EModifierKey mods, bool isRepeat)
{
    if (isRepeat)
        m_app.charKeyDownRepeat(charCode);
    else
        m_app.charKeyDown(charCode);
}

void EventCallback::charKeyUp(unsigned long charCode, boo::EModifierKey mods)
{
    m_app.charKeyUp(charCode);
}

void EventCallback::specialKeyDown(boo::ESpecialKey key, boo::EModifierKey mods, bool isRepeat)
{
    switch (key)
    {
    case boo::ESpecialKey::Left:
        m_app.m_wantsPrev = true;
        break;
    case boo::ESpecialKey::Right:
        m_app.m_wantsNext = true;
        break;
    case boo::ESpecialKey::Up:
        if (m_app.m_volume < 1.f)
            m_app.m_volume = amuse::clamp(0.f, m_app.m_volume + 0.05f, 1.f);
        if (m_app.m_vox)
            m_app.m_vox->setVolume(m_app.m_volume);
        if (m_app.m_seq)
            m_app.m_seq->setVolume(m_app.m_volume);
        m_app.m_updateDisp = true;
        break;
    case boo::ESpecialKey::Down:
        if (m_app.m_volume > 0.f)
            m_app.m_volume = amuse::clamp(0.f, m_app.m_volume - 0.05f, 1.f);
        if (m_app.m_vox)
            m_app.m_vox->setVolume(m_app.m_volume);
        if (m_app.m_seq)
            m_app.m_seq->setVolume(m_app.m_volume);
        m_app.m_updateDisp = true;
        break;
    default: break;
    }
}

void EventCallback::specialKeyUp(boo::ESpecialKey key, boo::EModifierKey mods)
{
}

void EventCallback::mouseDown(const boo::SWindowCoord& coord, boo::EMouseButton, boo::EModifierKey)
{
    m_tracking = true;
    m_app.m_pitchBend = amuse::clamp(-1.f, coord.norm[1] * 2.f - 1.f, 1.f);
    if (m_app.m_vox)
        m_app.m_vox->setPitchWheel(m_app.m_pitchBend);
    if (m_app.m_seq && m_app.m_chanId != -1)
        m_app.m_seq->setPitchWheel(m_app.m_chanId, m_app.m_pitchBend);
}

void EventCallback::mouseUp(const boo::SWindowCoord&, boo::EMouseButton, boo::EModifierKey)
{
    m_tracking = false;
    m_app.m_pitchBend = 0.f;
    if (m_app.m_vox)
        m_app.m_vox->setPitchWheel(m_app.m_pitchBend);
    if (m_app.m_seq && m_app.m_chanId != -1)
        m_app.m_seq->setPitchWheel(m_app.m_chanId, m_app.m_pitchBend);
}

void EventCallback::mouseMove(const boo::SWindowCoord& coord)
{
    if (m_tracking)
    {
        m_app.m_modulation = amuse::clamp(0.f, coord.norm[0], 1.f);
        if (m_app.m_vox)
            m_app.m_vox->setCtrlValue(1, m_app.m_modulation * 127.f);
        if (m_app.m_seq && m_app.m_chanId != -1)
            m_app.m_seq->setCtrlValue(m_app.m_chanId, 1, m_app.m_modulation * 127.f);

        m_app.m_pitchBend = amuse::clamp(-1.f, coord.norm[1] * 2.f - 1.f, 1.f);
        if (m_app.m_vox)
            m_app.m_vox->setPitchWheel(m_app.m_pitchBend);
        if (m_app.m_seq && m_app.m_chanId != -1)
            m_app.m_seq->setPitchWheel(m_app.m_chanId, m_app.m_pitchBend);
    }
}

#if _WIN32
int wmain(int argc, const boo::SystemChar** argv)
#else
int main(int argc, const boo::SystemChar** argv)
#endif
{
    logvisor::RegisterConsoleLogger();
    AppCallback app(argc, argv);
    int ret = boo::ApplicationRun(boo::IApplication::EPlatformType::Auto,
                                  app, _S("amuseplay"), _S("Amuse Player"), argc, argv);
    printf("IM DYING!!\n");
    return ret;
}

#if _WIN32
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPWSTR lpCmdLine, int)
{
    int argc = 0;
    const boo::SystemChar** argv = (const wchar_t**)(CommandLineToArgvW(lpCmdLine, &argc));
    static boo::SystemChar selfPath[1024];
    GetModuleFileNameW(nullptr, selfPath, 1024);
    static const boo::SystemChar* booArgv[32] = {};
    booArgv[0] = selfPath;
    for (int i=0 ; i<argc ; ++i)
        booArgv[i+1] = argv[i];

    logvisor::CreateWin32Console();
    return wmain(argc+1, booArgv);
}
#endif

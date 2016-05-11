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

static logvisor::Module Log("amusetool");

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
public:
    void charKeyDown(unsigned long charCode, boo::EModifierKey mods, bool isRepeat);
    void charKeyUp(unsigned long charCode, boo::EModifierKey mods);
    void specialKeyDown(boo::ESpecialKey key, boo::EModifierKey mods, bool isRepeat);
    void specialKeyUp(boo::ESpecialKey key, boo::EModifierKey mods);
    void resized(const boo::SWindowRect&, const boo::SWindowRect&) {}

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
    int m_chanId = -1;

    /* SFX playback selection */
    int m_sfxId = -1;
    amuse::Voice* m_vox = nullptr;

    /* Control state */
    bool m_running = true;
    bool m_wantsNext = false;
    bool m_wantsPrev = false;

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
               "<left/right>: cycle MIDI setup / channel, <up/down>: volume\n"
               "<tab>: sustain pedal, <window-Y>: pitch wheel, <window-X>: mod wheel\n"
               "<Z/X>: octave, <C/V>: velocity\n"
               "<Q>: quit\n");

        std::map<int, const std::array<amuse::SongGroupIndex::MIDISetup, 16>*> sortEntries
            (index.m_midiSetups.cbegin(), index.m_midiSetups.cend());
        auto setupIt = sortEntries.cbegin();
        if (setupIt != sortEntries.cend())
        {
            m_setupId = setupIt->first;
            m_chanId = 0;
        }

        while (m_running)
        {
            m_events.dispatchEvents();

            if (m_wantsNext)
            {
                m_wantsNext = false;

            }

            m_engine->pumpEngine();
            m_win->waitForRetrace();
        }
    }

    void SFXLoop(const amuse::AudioGroup& group,
                 const amuse::SFXGroupIndex& index)
    {
        printf("<space>: keyon/keyon, <left/right>: cycle SFX, <up/down>: volume\n"
               "<Q>: quit\n");

        std::map<uint16_t, const amuse::SFXGroupIndex::SFXEntry*> sortEntries
            (index.m_sfxEntries.cbegin(), index.m_sfxEntries.cend());
        auto sfxIt = sortEntries.cbegin();
        if (sfxIt != sortEntries.cend())
        {
            m_sfxId = sfxIt->first;
            m_vox = m_engine->fxStart(m_sfxId, 1.f, 0.f);
        }

        while (m_running)
        {
            m_events.dispatchEvents();
            m_engine->pumpEngine();
            m_win->waitForRetrace();
        }
    }

    void charKeyDown(unsigned long charCode)
    {
        if (m_sfxGroup)
        {
            switch (charCode)
            {
            case ' ':
                if (m_vox && m_vox->state() == amuse::VoiceState::Playing)
                    m_vox->keyOff();
                else if (m_sfxId != -1)
                    m_vox = m_engine->fxStart(m_sfxId, 1.f, 0.f);
            default: break;
            }
        }
        else
        {
        }
    }

    void charKeyUp(unsigned long charCode)
    {
    }

    int appMain(boo::IApplication* app)
    {
        /* Event window */
        m_win = app->newWindow("amusetool", 1);
        m_win->setWindowFrame(100, 100, 100, 100);
        m_win->setCallback(&m_events);
        m_win->showWindow();

        /* Load data */
        std::string desc;
        bool good = false;
        amuse::IntrusiveAudioGroupData data = LoadFromArgs(m_argc, m_argv, desc, good);
        if (!good)
            Log.report(logvisor::Fatal, "incomplete data in args");
        printf("Found '%s' Audio Group data\n", desc.c_str());

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
                printf("    %d (SongGroup)  %" PRISize "  normal-pages, %" PRISize " drum-pages\n",
                       pair.first, pair.second.m_normPages.size(), pair.second.m_drumPages.size());
            }
            for (const auto& pair : proj.sfxGroups())
            {
                printf("    %d (SFXGroup)   %" PRISize "  sfx-entries\n",
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
        const amuse::AudioGroup* group = m_engine->addAudioGroup(m_groupId, data);
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
        return;
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
    default: break;
    }
}

void EventCallback::specialKeyUp(boo::ESpecialKey key, boo::EModifierKey mods)
{
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
                                  app, _S("amusetool"), _S("Amuse Tool"), argc, argv);
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

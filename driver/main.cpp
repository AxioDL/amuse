#include "amuse/amuse.hpp"
#include "amuse/BooBackend.hpp"
#include "boo/audiodev/IAudioVoiceEngine.hpp"
#include "logvisor/logvisor.hpp"
#include <thread>

static logvisor::Module Log("amusetool");

static amuse::IntrusiveAudioGroupData LoadFromArgs(int argc, char* argv[],
                                                   std::string& descOut, bool& good)
{
    return {nullptr, nullptr, nullptr, nullptr};
}

int main(int argc, char* argv[])
{
    logvisor::RegisterConsoleLogger();

    /* Load data */
    std::string desc;
    bool good = false;
    amuse::IntrusiveAudioGroupData data = LoadFromArgs(argc, argv, desc, good);
    if (!good)
        Log.report(logvisor::Fatal, "incomplete data in args");
    printf("Found '%s' Audio Group data\n", desc.c_str());

    /* Load project to assemble group list */
    amuse::AudioGroupProject proj(data.getProj());

    /* Get group selection from user */
    int groupId;
    bool sfxGroup;
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
            groupId = userSel;
            sfxGroup = false;
        }
        else if (proj.getSFXGroupIndex(userSel))
        {
            groupId = userSel;
            sfxGroup = true;
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
            groupId = pair.first;
            sfxGroup = false;
        }
        else
        {
            const auto& pair = *proj.sfxGroups().cbegin();
            groupId = pair.first;
            sfxGroup = true;
        }
    }
    else
        Log.report(logvisor::Fatal, "empty project");

    /* Build voice engine */
    std::unique_ptr<boo::IAudioVoiceEngine> voxEngine = boo::NewAudioVoiceEngine();
    amuse::BooBackendVoiceAllocator booBackend(*voxEngine);
    amuse::Engine engine(booBackend);

    engine.addAudioGroup(groupId, data);

    amuse::Voice* vox = engine.fxStart(1, 1.f, 0.f);

    for (int f=0 ; f<300 ; ++f)
    {
        engine.pumpEngine();
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    vox->keyOff();

    for (int f=0 ; f<120 ; ++f)
    {
        engine.pumpEngine();
        std::this_thread::sleep_for(std::chrono::milliseconds(15));
    }

    return 0;
}

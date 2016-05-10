#include "amuse/amuse.hpp"
#include "amuse/BooBackend.hpp"
#include "boo/audiodev/IAudioVoiceEngine.hpp"
#include "logvisor/logvisor.hpp"
#include <thread>

int main(int argc, char* argv[])
{
    logvisor::RegisterConsoleLogger();

    std::unique_ptr<boo::IAudioVoiceEngine> voxEngine = boo::NewAudioVoiceEngine();
    amuse::BooBackendVoiceAllocator booBackend(*voxEngine);
    amuse::Engine engine(booBackend);

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

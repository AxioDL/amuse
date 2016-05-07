#ifndef __AMUSE_SUBMIX_HPP__
#define __AMUSE_SUBMIX_HPP__

#include <memory>
#include "SoundMacroState.hpp"
#include "IBackendSubmix.hpp"
#include "IBackendVoice.hpp"
#include <unordered_set>

namespace amuse
{
class IBackendSubmix;

/** Intermediate mix of voices for applying auxiliary effects */
class Submix
{
    friend class Engine;
    friend class Voice;
    Engine& m_root;
    Submix* m_submix = nullptr; /**< Parent submix of this submix (or NULL if mixing to main output) */
    std::list<Submix>::iterator m_engineIt; /**< Iterator to self within Engine's list for quick deletion */
    std::unique_ptr<IBackendSubmix> m_backendSubmix; /**< Handle to client-implemented backend submix */
    std::unordered_set<Voice*> m_activeVoices; /**< Secondary index of Voices within Submix */
    std::unordered_set<Submix*> m_activeSubmixes; /**< Secondary index of Submixes within Submix */
    bool m_destroyed = false;
    void _destroy();
public:
    Submix(Engine& engine, Submix* smx);
    ~Submix()
    {
#ifndef NDEBUG
        /* Ensure proper destruction procedure followed */
        assert(m_destroyed);
#endif
    }

    /** Obtain pointer to Submix's parent Submix */
    Submix* getParentSubmix() {return m_submix;}

    /** Returns true when an effect callback is bound */
    bool canApplyEffect() const;

    /** in/out transformation entry for audio effect */
    void applyEffect(int16_t* audio, const ChannelMap& chanMap, double sampleRate) const;
    void applyEffect(int32_t* audio, const ChannelMap& chanMap, double sampleRate) const;
    void applyEffect(float* audio, const ChannelMap& chanMap, double sampleRate) const;

    Engine& getEngine() {return m_root;}
};

}

#endif // __AMUSE_SUBMIX_HPP__

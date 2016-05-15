#ifndef __AMUSE_SUBMIX_HPP__
#define __AMUSE_SUBMIX_HPP__

#include <memory>
#include "SoundMacroState.hpp"
#include "IBackendSubmix.hpp"
#include "IBackendVoice.hpp"
#include "EffectBase.hpp"
#include "EffectChorus.hpp"
#include "EffectDelay.hpp"
#include "EffectReverb.hpp"
#include <unordered_set>

namespace amuse
{
class IBackendSubmix;
class Sequencer;

/** Intermediate mix of voices for applying auxiliary effects */
class Submix
{
    friend class Engine;
    friend class Voice;
    friend class Sequencer;
    Engine& m_root;
    Submix* m_submix = nullptr; /**< Parent submix of this submix (or NULL if mixing to main output) */
    std::list<Submix>::iterator m_engineIt; /**< Iterator to self within Engine's list for quick deletion */
    std::unique_ptr<IBackendSubmix> m_backendSubmix; /**< Handle to client-implemented backend submix */
    std::unordered_set<Voice*> m_activeVoices; /**< Secondary index of Voices within Submix */
    std::unordered_set<Sequencer*> m_activeSequencers; /**< Secondary index of Sequencers within Submix */
    std::unordered_set<Submix*> m_activeSubmixes; /**< Secondary index of Submixes within Submix */
    std::vector<std::unique_ptr<EffectBaseTypeless>> m_effectStack; /**< Ordered list of effects to apply to submix */
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

    /** Add new effect to effect stack and assume ownership */
    template <class T, class ...Args>
    T& makeEffect(Args... args)
    {
        switch (m_backendSubmix->getSampleFormat())
        {
        case SubmixFormat::Int16:
        {
            using ImpType = typename T::template ImpType<int16_t>;
            m_effectStack.emplace_back(new ImpType(args..., m_backendSubmix->getSampleRate()));
            return static_cast<ImpType&>(*m_effectStack.back());
        }
        case SubmixFormat::Int32:
        {
            using ImpType = typename T::template ImpType<int32_t>;
            m_effectStack.emplace_back(new ImpType(args..., m_backendSubmix->getSampleRate()));
            return static_cast<ImpType&>(*m_effectStack.back());
        }
        case SubmixFormat::Float:
        {
            using ImpType = typename T::template ImpType<float>;
            m_effectStack.emplace_back(new ImpType(args..., m_backendSubmix->getSampleRate()));
            return static_cast<ImpType&>(*m_effectStack.back());
        }
        }
    }

    /** Add new chorus effect to effect stack and assume ownership */
    EffectChorus& makeChorus(uint32_t baseDelay, uint32_t variation, uint32_t period);

    /** Add new delay effect to effect stack and assume ownership */
    EffectDelay& makeDelay(uint32_t initDelay, uint32_t initFeedback, uint32_t initOutput);

    /** Add new standard-quality reverb effect to effect stack and assume ownership */
    EffectReverb& makeReverbStd(float coloration, float mix, float time,
                                float damping, float preDelay);

    /** Add new high-quality reverb effect to effect stack and assume ownership */
    EffectReverbHi& makeReverbHi(float coloration, float mix, float time,
                                 float damping, float preDelay, float crosstalk);

    /** Remove and deallocate all effects from effect stack */
    void clearEffects() {m_effectStack.clear();}

    /** Returns true when an effect callback is bound */
    bool canApplyEffect() const {return m_effectStack.size() != 0;}

    /** in/out transformation entry for audio effect */
    void applyEffect(int16_t* audio, size_t frameCount, const ChannelMap& chanMap) const;

    /** in/out transformation entry for audio effect */
    void applyEffect(int32_t* audio, size_t frameCount, const ChannelMap& chanMap) const;

    /** in/out transformation entry for audio effect */
    void applyEffect(float* audio, size_t frameCount, const ChannelMap& chanMap) const;

    Engine& getEngine() {return m_root;}
};

}

#endif // __AMUSE_SUBMIX_HPP__

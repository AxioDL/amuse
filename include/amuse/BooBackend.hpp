#ifndef __AMUSE_BOO_BACKEND_HPP__
#define __AMUSE_BOO_BACKEND_HPP__

#include <boo/audiodev/IAudioVoiceEngine.hpp>
#include <boo/audiodev/IAudioSubmix.hpp>
#include "IBackendVoice.hpp"
#include "IBackendSubmix.hpp"
#include "IBackendVoiceAllocator.hpp"

namespace amuse
{

/** Backend voice implementation for boo mixer */
class BooBackendVoice : public IBackendVoice
{
    friend class BooBackendVoiceAllocator;
    Voice& m_clientVox;
    struct VoiceCallback : boo::IAudioVoiceCallback
    {
        BooBackendVoice& m_parent;
        size_t supplyAudio(boo::IAudioVoice& voice, size_t frames, int16_t* data);
        VoiceCallback(BooBackendVoice& parent) : m_parent(parent) {}
    } m_cb;
    std::unique_ptr<boo::IAudioVoice> m_booVoice;
public:
    BooBackendVoice(boo::IAudioVoiceEngine& engine, Voice& clientVox,
                    double sampleRate, bool dynamicPitch);
    BooBackendVoice(boo::IAudioSubmix& submix, Voice& clientVox,
                    double sampleRate, bool dynamicPitch);
    void resetSampleRate(double sampleRate);
    void setMatrixCoefficients(const float coefs[8]);
    void setPitchRatio(double ratio);
    void start();
    void stop();
};

/** Backend submix implementation for boo mixer */
class BooBackendSubmix : public IBackendSubmix
{
    friend class BooBackendVoiceAllocator;
    Submix& m_clientSmx;
    struct SubmixCallback : boo::IAudioSubmixCallback
    {
        BooBackendSubmix& m_parent;
        bool canApplyEffect() const;
        void applyEffect(int16_t* audio, size_t frameCount,
                         const boo::ChannelMap& chanMap, double sampleRate) const;
        void applyEffect(int32_t* audio, size_t frameCount,
                         const boo::ChannelMap& chanMap, double sampleRate) const;
        void applyEffect(float* audio, size_t frameCount,
                         const boo::ChannelMap& chanMap, double sampleRate) const;
        SubmixCallback(BooBackendSubmix& parent) : m_parent(parent) {}
    } m_cb;
    std::unique_ptr<boo::IAudioSubmix> m_booSubmix;
public:
    BooBackendSubmix(boo::IAudioVoiceEngine& engine, Submix& clientSmx);
    BooBackendSubmix(boo::IAudioSubmix& parent, Submix& clientSmx);
    void setChannelGains(const float gains[8]);
    std::unique_ptr<IBackendVoice> allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch);
};

/** Backend voice allocator implementation for boo mixer */
class BooBackendVoiceAllocator : public IBackendVoiceAllocator
{
    boo::IAudioVoiceEngine& m_booEngine;
public:
    BooBackendVoiceAllocator(boo::IAudioVoiceEngine& booEngine);
    std::unique_ptr<IBackendVoice> allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch);
    std::unique_ptr<IBackendSubmix> allocateSubmix(Submix& clientSmx);
    AudioChannelSet getAvailableSet();
};

}

#endif // __AMUSE_BOO_BACKEND_HPP__

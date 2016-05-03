#ifndef __AMUSE_BOO_BACKEND_HPP__
#define __AMUSE_BOO_BACKEND_HPP__

#include <boo/audiodev/IAudioVoiceEngine.hpp>
#include "IBackendVoice.hpp"
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
    void setMatrixCoefficients(const float coefs[8]);
    void setPitchRatio(double ratio);
    void start();
    void stop();
};

/** Backend voice allocator implementation for boo mixer */
class BooBackendVoiceAllocator : public IBackendVoiceAllocator
{
    boo::IAudioVoiceEngine& m_booEngine;
public:
    BooBackendVoiceAllocator(boo::IAudioVoiceEngine& booEngine);
    std::unique_ptr<IBackendVoice> allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch);
};

}

#endif // __AMUSE_BOO_BACKEND_HPP__

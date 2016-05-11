#include "amuse/BooBackend.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Submix.hpp"

namespace amuse
{

size_t BooBackendVoice::VoiceCallback::supplyAudio(boo::IAudioVoice&,
                                                   size_t frames, int16_t* data)
{
    return m_parent.m_clientVox.supplyAudio(frames, data);
}

BooBackendVoice::BooBackendVoice(boo::IAudioVoiceEngine& engine, Voice& clientVox,
                                 double sampleRate, bool dynamicPitch)
: m_clientVox(clientVox), m_cb(*this),
  m_booVoice(engine.allocateNewMonoVoice(sampleRate, &m_cb, dynamicPitch))
{}

BooBackendVoice::BooBackendVoice(boo::IAudioSubmix& submix, Voice& clientVox,
                                 double sampleRate, bool dynamicPitch)
: m_clientVox(clientVox), m_cb(*this),
  m_booVoice(submix.allocateNewMonoVoice(sampleRate, &m_cb, dynamicPitch))
{}

void BooBackendVoice::resetSampleRate(double sampleRate)
{
    m_booVoice->resetSampleRate(sampleRate);
}

void BooBackendVoice::setMatrixCoefficients(const float coefs[8])
{
    m_booVoice->setMonoMatrixCoefficients(coefs);
}

void BooBackendVoice::setPitchRatio(double ratio)
{
    m_booVoice->setPitchRatio(ratio);
}

void BooBackendVoice::start()
{
    m_booVoice->start();
}

void BooBackendVoice::stop()
{
    m_booVoice->stop();
}

bool BooBackendSubmix::SubmixCallback::canApplyEffect() const
{
    return m_parent.m_clientSmx.canApplyEffect();
}

void BooBackendSubmix::SubmixCallback::applyEffect(int16_t* audio, size_t frameCount,
                                                   const boo::ChannelMap& chanMap, double sampleRate) const
{
    return m_parent.m_clientSmx.applyEffect(audio, reinterpret_cast<const ChannelMap&>(chanMap), sampleRate);
}

void BooBackendSubmix::SubmixCallback::applyEffect(int32_t* audio, size_t frameCount,
                                                   const boo::ChannelMap& chanMap, double sampleRate) const
{
    return m_parent.m_clientSmx.applyEffect(audio, reinterpret_cast<const ChannelMap&>(chanMap), sampleRate);
}

void BooBackendSubmix::SubmixCallback::applyEffect(float* audio, size_t frameCount,
                                                   const boo::ChannelMap& chanMap, double sampleRate) const
{
    return m_parent.m_clientSmx.applyEffect(audio, reinterpret_cast<const ChannelMap&>(chanMap), sampleRate);
}

BooBackendSubmix::BooBackendSubmix(boo::IAudioVoiceEngine& engine, Submix& clientSmx)
: m_clientSmx(clientSmx), m_cb(*this), m_booSubmix(engine.allocateNewSubmix(&m_cb))
{}

BooBackendSubmix::BooBackendSubmix(boo::IAudioSubmix& parent, Submix& clientSmx)
: m_clientSmx(clientSmx), m_cb(*this), m_booSubmix(parent.allocateNewSubmix(&m_cb))
{}

void BooBackendSubmix::setChannelGains(const float gains[8])
{
    m_booSubmix->setChannelGains(gains);
}

std::unique_ptr<IBackendVoice>
BooBackendSubmix::allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch)
{
    return std::make_unique<BooBackendVoice>(*m_booSubmix, clientVox, sampleRate, dynamicPitch);
}

BooBackendVoiceAllocator::BooBackendVoiceAllocator(boo::IAudioVoiceEngine& booEngine)
: m_booEngine(booEngine)
{}

std::unique_ptr<IBackendVoice>
BooBackendVoiceAllocator::allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch)
{
    return std::make_unique<BooBackendVoice>(m_booEngine, clientVox, sampleRate, dynamicPitch);
}

std::unique_ptr<IBackendSubmix> BooBackendVoiceAllocator::allocateSubmix(Submix& clientSmx)
{
    return std::make_unique<BooBackendSubmix>(m_booEngine, clientSmx);
}

AudioChannelSet BooBackendVoiceAllocator::getAvailableSet()
{
    return AudioChannelSet(m_booEngine.getAvailableSet());
}

}

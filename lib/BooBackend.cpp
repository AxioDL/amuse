#include "amuse/BooBackend.hpp"
#include "amuse/Voice.hpp"

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

BooBackendVoiceAllocator::BooBackendVoiceAllocator(boo::IAudioVoiceEngine& booEngine)
: m_booEngine(booEngine)
{}

std::unique_ptr<IBackendVoice>
BooBackendVoiceAllocator::allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch)
{
    return std::make_unique<BooBackendVoice>(m_booEngine, clientVox, sampleRate, dynamicPitch);
}

}

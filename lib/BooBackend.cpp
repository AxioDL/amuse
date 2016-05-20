#include "amuse/BooBackend.hpp"
#include "amuse/Voice.hpp"
#include "amuse/Submix.hpp"
#include "amuse/Engine.hpp"

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

void BooBackendVoice::setPitchRatio(double ratio, bool slew)
{
    m_booVoice->setPitchRatio(ratio, slew);
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
                                                   const boo::ChannelMap& chanMap, double) const
{
    return m_parent.m_clientSmx.applyEffect(audio, frameCount, reinterpret_cast<const ChannelMap&>(chanMap));
}

void BooBackendSubmix::SubmixCallback::applyEffect(int32_t* audio, size_t frameCount,
                                                   const boo::ChannelMap& chanMap, double) const
{
    return m_parent.m_clientSmx.applyEffect(audio, frameCount, reinterpret_cast<const ChannelMap&>(chanMap));
}

void BooBackendSubmix::SubmixCallback::applyEffect(float* audio, size_t frameCount,
                                                   const boo::ChannelMap& chanMap, double) const
{
    return m_parent.m_clientSmx.applyEffect(audio, frameCount, reinterpret_cast<const ChannelMap&>(chanMap));
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

double BooBackendSubmix::getSampleRate() const
{
    return m_booSubmix->getSampleRate();
}

SubmixFormat BooBackendSubmix::getSampleFormat() const
{
    return SubmixFormat(m_booSubmix->getSampleFormat());
}


std::string BooBackendMIDIReader::description()
{
    return m_midiIn->description();
}

void BooBackendMIDIReader::pumpReader()
{
    while (m_decoder.receiveBytes()) {}
}

void BooBackendMIDIReader::noteOff(uint8_t chan, uint8_t key, uint8_t velocity)
{
    for (std::shared_ptr<Sequencer>& seq : m_engine.getActiveSequencers())
        seq->keyOff(chan, key, velocity);
}

void BooBackendMIDIReader::noteOn(uint8_t chan, uint8_t key, uint8_t velocity)
{
    for (std::shared_ptr<Sequencer>& seq : m_engine.getActiveSequencers())
        seq->keyOn(chan, key, velocity);
}

void BooBackendMIDIReader::notePressure(uint8_t chan, uint8_t key, uint8_t pressure)
{
}

void BooBackendMIDIReader::controlChange(uint8_t chan, uint8_t control, uint8_t value)
{
    for (std::shared_ptr<Sequencer>& seq : m_engine.getActiveSequencers())
        seq->setCtrlValue(chan, control, value);
}

void BooBackendMIDIReader::programChange(uint8_t chan, uint8_t program)
{
    for (std::shared_ptr<Sequencer>& seq : m_engine.getActiveSequencers())
        seq->setChanProgram(chan, program);
}

void BooBackendMIDIReader::channelPressure(uint8_t chan, uint8_t pressure)
{
}

void BooBackendMIDIReader::pitchBend(uint8_t chan, int16_t pitch)
{
    for (std::shared_ptr<Sequencer>& seq : m_engine.getActiveSequencers())
        seq->setPitchWheel(chan, (pitch - 0x2000) / float(0x2000));
}


void BooBackendMIDIReader::allSoundOff(uint8_t chan)
{
    for (std::shared_ptr<Sequencer>& seq : m_engine.getActiveSequencers())
        seq->allOff(chan, true);
}

void BooBackendMIDIReader::resetAllControllers(uint8_t chan)
{
}

void BooBackendMIDIReader::localControl(uint8_t chan, bool on)
{
}

void BooBackendMIDIReader::allNotesOff(uint8_t chan)
{
    for (std::shared_ptr<Sequencer>& seq : m_engine.getActiveSequencers())
        seq->allOff(chan, false);
}

void BooBackendMIDIReader::omniMode(uint8_t chan, bool on)
{
}

void BooBackendMIDIReader::polyMode(uint8_t chan, bool on)
{
}


void BooBackendMIDIReader::sysex(const void* data, size_t len)
{
}

void BooBackendMIDIReader::timeCodeQuarterFrame(uint8_t message, uint8_t value)
{
}

void BooBackendMIDIReader::songPositionPointer(uint16_t pointer)
{
}

void BooBackendMIDIReader::songSelect(uint8_t song)
{
}

void BooBackendMIDIReader::tuneRequest()
{
}


void BooBackendMIDIReader::startSeq()
{
}

void BooBackendMIDIReader::continueSeq()
{
}

void BooBackendMIDIReader::stopSeq()
{
}


void BooBackendMIDIReader::reset()
{
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

std::vector<std::pair<std::string, std::string>> BooBackendVoiceAllocator::enumerateMIDIDevices()
{
    return m_booEngine.enumerateMIDIDevices();
}

std::unique_ptr<IMIDIReader> BooBackendVoiceAllocator::allocateMIDIReader(Engine& engine, const char* name)
{
    std::unique_ptr<boo::IMIDIIn> inPort;
    if (!name)
        inPort = m_booEngine.newVirtualMIDIIn();
    else
        inPort = m_booEngine.newRealMIDIIn(name);

    if (!inPort)
        return {};

    return std::make_unique<BooBackendMIDIReader>(engine, std::move(inPort));
}

void BooBackendVoiceAllocator::register5MsCallback(std::function<void(double)>&& callback)
{
    m_booEngine.register5MsCallback(std::move(callback));
}

AudioChannelSet BooBackendVoiceAllocator::getAvailableSet()
{
    return AudioChannelSet(m_booEngine.getAvailableSet());
}

void BooBackendVoiceAllocator::pumpAndMixVoices()
{
    m_booEngine.pumpAndMixVoices();
}

}

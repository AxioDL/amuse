#include "amuse/BooBackend.hpp"

#include "amuse/Engine.hpp"
#include "amuse/Submix.hpp"
#include "amuse/Voice.hpp"

namespace amuse {

void BooBackendVoice::VoiceCallback::preSupplyAudio(boo::IAudioVoice&, double dt) {
  m_parent.m_clientVox.preSupplyAudio(dt);
}

size_t BooBackendVoice::VoiceCallback::supplyAudio(boo::IAudioVoice&, size_t frames, int16_t* data) {
  return m_parent.m_clientVox.supplyAudio(frames, data);
}

void BooBackendVoice::VoiceCallback::routeAudio(size_t frames, size_t channels, double dt, int busId, int16_t* in,
                                                int16_t* out) {
  m_parent.m_clientVox.routeAudio(frames, dt, busId, in, out);
}

void BooBackendVoice::VoiceCallback::routeAudio(size_t frames, size_t channels, double dt, int busId, int32_t* in,
                                                int32_t* out) {
  m_parent.m_clientVox.routeAudio(frames, dt, busId, in, out);
}

void BooBackendVoice::VoiceCallback::routeAudio(size_t frames, size_t channels, double dt, int busId, float* in,
                                                float* out) {
  m_parent.m_clientVox.routeAudio(frames, dt, busId, in, out);
}

BooBackendVoice::BooBackendVoice(boo::IAudioVoiceEngine& engine, Voice& clientVox, double sampleRate, bool dynamicPitch)
: m_clientVox(clientVox), m_cb(*this), m_booVoice(engine.allocateNewMonoVoice(sampleRate, &m_cb, dynamicPitch)) {}

void BooBackendVoice::resetSampleRate(double sampleRate) { m_booVoice->resetSampleRate(sampleRate); }

void BooBackendVoice::resetChannelLevels() { m_booVoice->resetChannelLevels(); }

void BooBackendVoice::setChannelLevels(IBackendSubmix* submix, const std::array<float, 8>& coefs, bool slew) {
  auto& smx = *static_cast<BooBackendSubmix*>(submix);
  m_booVoice->setMonoChannelLevels(smx.m_booSubmix.get(), coefs.data(), slew);
}

void BooBackendVoice::setPitchRatio(double ratio, bool slew) { m_booVoice->setPitchRatio(ratio, slew); }

void BooBackendVoice::start() { m_booVoice->start(); }

void BooBackendVoice::stop() { m_booVoice->stop(); }

bool BooBackendSubmix::SubmixCallback::canApplyEffect() const { return m_parent.m_clientSmx.canApplyEffect(); }

void BooBackendSubmix::SubmixCallback::applyEffect(int16_t* audio, size_t frameCount, const boo::ChannelMap& chanMap,
                                                   double) const {
  return m_parent.m_clientSmx.applyEffect(audio, frameCount, reinterpret_cast<const ChannelMap&>(chanMap));
}

void BooBackendSubmix::SubmixCallback::applyEffect(int32_t* audio, size_t frameCount, const boo::ChannelMap& chanMap,
                                                   double) const {
  return m_parent.m_clientSmx.applyEffect(audio, frameCount, reinterpret_cast<const ChannelMap&>(chanMap));
}

void BooBackendSubmix::SubmixCallback::applyEffect(float* audio, size_t frameCount, const boo::ChannelMap& chanMap,
                                                   double) const {
  return m_parent.m_clientSmx.applyEffect(audio, frameCount, reinterpret_cast<const ChannelMap&>(chanMap));
}

void BooBackendSubmix::SubmixCallback::resetOutputSampleRate(double sampleRate) {
  m_parent.m_clientSmx.resetOutputSampleRate(sampleRate);
}

BooBackendSubmix::BooBackendSubmix(boo::IAudioVoiceEngine& engine, Submix& clientSmx, bool mainOut, int busId)
: m_clientSmx(clientSmx), m_cb(*this), m_booSubmix(engine.allocateNewSubmix(mainOut, &m_cb, busId)) {}

void BooBackendSubmix::setSendLevel(IBackendSubmix* submix, float level, bool slew) {
  BooBackendSubmix& smx = *reinterpret_cast<BooBackendSubmix*>(submix);
  m_booSubmix->setSendLevel(smx.m_booSubmix.get(), level, slew);
}

double BooBackendSubmix::getSampleRate() const { return m_booSubmix->getSampleRate(); }

SubmixFormat BooBackendSubmix::getSampleFormat() const { return SubmixFormat(m_booSubmix->getSampleFormat()); }

BooBackendMIDIReader::~BooBackendMIDIReader() {}

BooBackendMIDIReader::BooBackendMIDIReader(Engine& engine, bool useLock)
: m_engine(engine), m_decoder(*this), m_useLock(useLock) {
  BooBackendVoiceAllocator& voxAlloc = static_cast<BooBackendVoiceAllocator&>(engine.getBackend());
  auto devices = voxAlloc.m_booEngine.enumerateMIDIInputs();
  for (const auto& dev : devices) {
    auto midiIn =
        voxAlloc.m_booEngine.newRealMIDIIn(dev.first.c_str(), std::bind(&BooBackendMIDIReader::_MIDIReceive, this,
                                                                        std::placeholders::_1, std::placeholders::_2));
    if (midiIn)
      m_midiIns[dev.first] = std::move(midiIn);
  }
  if (voxAlloc.m_booEngine.supportsVirtualMIDIIn())
    m_virtualIn = voxAlloc.m_booEngine.newVirtualMIDIIn(
        std::bind(&BooBackendMIDIReader::_MIDIReceive, this, std::placeholders::_1, std::placeholders::_2));
}

void BooBackendMIDIReader::addMIDIIn(const char* name) {
  BooBackendVoiceAllocator& voxAlloc = static_cast<BooBackendVoiceAllocator&>(m_engine.getBackend());
  auto midiIn = voxAlloc.m_booEngine.newRealMIDIIn(
      name, std::bind(&BooBackendMIDIReader::_MIDIReceive, this, std::placeholders::_1, std::placeholders::_2));
  if (midiIn)
    m_midiIns[name] = std::move(midiIn);
}

void BooBackendMIDIReader::removeMIDIIn(const char* name) { m_midiIns.erase(name); }

bool BooBackendMIDIReader::hasMIDIIn(const char* name) const { return m_midiIns.find(name) != m_midiIns.cend(); }

void BooBackendMIDIReader::setVirtualIn(bool v) {
  if (v) {
    BooBackendVoiceAllocator& voxAlloc = static_cast<BooBackendVoiceAllocator&>(m_engine.getBackend());
    if (voxAlloc.m_booEngine.supportsVirtualMIDIIn())
      m_virtualIn = voxAlloc.m_booEngine.newVirtualMIDIIn(
          std::bind(&BooBackendMIDIReader::_MIDIReceive, this, std::placeholders::_1, std::placeholders::_2));
  } else {
    m_virtualIn.reset();
  }
}

bool BooBackendMIDIReader::hasVirtualIn() const { return m_virtualIn.operator bool(); }

void BooBackendMIDIReader::_MIDIReceive(std::vector<uint8_t>&& bytes, double time) {
  std::unique_lock<std::mutex> lk(m_midiMutex, std::defer_lock_t{});
  if (m_useLock)
    lk.lock();
  m_queue.emplace_back(time, std::move(bytes));
#if 0
    openlog("LogIt", (LOG_CONS|LOG_PERROR|LOG_PID), LOG_DAEMON);
    syslog(LOG_EMERG, "MIDI receive %f\n", time);
    closelog();
#endif
}

void BooBackendMIDIReader::pumpReader(double dt) {
  dt += 0.001; /* Add 1ms to ensure consumer keeps up with producer */

  std::unique_lock<std::mutex> lk(m_midiMutex, std::defer_lock_t{});
  if (m_useLock)
    lk.lock();
  if (m_queue.empty())
    return;

  /* Determine range of buffer updates within this period */
  auto periodEnd = m_queue.cbegin();
  double startPt = m_queue.front().first;
  for (; periodEnd != m_queue.cend(); ++periodEnd) {
    double delta = periodEnd->first - startPt;
    if (delta > dt)
      break;
  }

  if (m_queue.cbegin() == periodEnd)
    return;

  /* Dispatch buffers */
  for (auto it = m_queue.begin(); it != periodEnd;) {
#if 0
        char str[64];
        sprintf(str, "MIDI %zu %f ", it->second.size(), it->first);
        for (uint8_t byte : it->second)
            sprintf(str + strlen(str), "%02X ", byte);
        openlog("LogIt", (LOG_CONS|LOG_PERROR|LOG_PID), LOG_DAEMON);
        syslog(LOG_EMERG, "%s\n", str);
        closelog();
#endif
    m_decoder.receiveBytes(it->second.cbegin(), it->second.cend());
    it = m_queue.erase(it);
  }
}

void BooBackendMIDIReader::noteOff(uint8_t chan, uint8_t key, uint8_t velocity) {
  for (ObjToken<Sequencer>& seq : m_engine.getActiveSequencers())
    seq->keyOff(chan, key, velocity);
#if 0
    openlog("LogIt", (LOG_CONS|LOG_PERROR|LOG_PID), LOG_DAEMON);
    syslog(LOG_EMERG, "NoteOff %d", key);
    closelog();
#endif
}

void BooBackendMIDIReader::noteOn(uint8_t chan, uint8_t key, uint8_t velocity) {
  for (ObjToken<Sequencer>& seq : m_engine.getActiveSequencers())
    seq->keyOn(chan, key, velocity);
#if 0
    openlog("LogIt", (LOG_CONS|LOG_PERROR|LOG_PID), LOG_DAEMON);
    syslog(LOG_EMERG, "NoteOn %d", key);
    closelog();
#endif
}

void BooBackendMIDIReader::notePressure(uint8_t /*chan*/, uint8_t /*key*/, uint8_t /*pressure*/) {}

void BooBackendMIDIReader::controlChange(uint8_t chan, uint8_t control, uint8_t value) {
  for (ObjToken<Sequencer>& seq : m_engine.getActiveSequencers())
    seq->setCtrlValue(chan, control, value);
}

void BooBackendMIDIReader::programChange(uint8_t chan, uint8_t program) {
  for (ObjToken<Sequencer>& seq : m_engine.getActiveSequencers())
    seq->setChanProgram(chan, program);
}

void BooBackendMIDIReader::channelPressure(uint8_t /*chan*/, uint8_t /*pressure*/) {}

void BooBackendMIDIReader::pitchBend(uint8_t chan, int16_t pitch) {
  for (ObjToken<Sequencer>& seq : m_engine.getActiveSequencers())
    seq->setPitchWheel(chan, (pitch - 0x2000) / float(0x2000));
}

void BooBackendMIDIReader::allSoundOff(uint8_t chan) {
  for (ObjToken<Sequencer>& seq : m_engine.getActiveSequencers())
    seq->allOff(chan, true);
}

void BooBackendMIDIReader::resetAllControllers(uint8_t /*chan*/) {}

void BooBackendMIDIReader::localControl(uint8_t /*chan*/, bool /*on*/) {}

void BooBackendMIDIReader::allNotesOff(uint8_t chan) {
  for (ObjToken<Sequencer>& seq : m_engine.getActiveSequencers())
    seq->allOff(chan, false);
}

void BooBackendMIDIReader::omniMode(uint8_t /*chan*/, bool /*on*/) {}

void BooBackendMIDIReader::polyMode(uint8_t /*chan*/, bool /*on*/) {}

void BooBackendMIDIReader::sysex(const void* /*data*/, size_t /*len*/) {}

void BooBackendMIDIReader::timeCodeQuarterFrame(uint8_t /*message*/, uint8_t /*value*/) {}

void BooBackendMIDIReader::songPositionPointer(uint16_t /*pointer*/) {}

void BooBackendMIDIReader::songSelect(uint8_t /*song*/) {}

void BooBackendMIDIReader::tuneRequest() {}

void BooBackendMIDIReader::startSeq() {}

void BooBackendMIDIReader::continueSeq() {}

void BooBackendMIDIReader::stopSeq() {}

void BooBackendMIDIReader::reset() {}

BooBackendVoiceAllocator::BooBackendVoiceAllocator(boo::IAudioVoiceEngine& booEngine) : m_booEngine(booEngine) {
  booEngine.setCallbackInterface(this);
}

std::unique_ptr<IBackendVoice> BooBackendVoiceAllocator::allocateVoice(Voice& clientVox, double sampleRate,
                                                                       bool dynamicPitch) {
  return std::make_unique<BooBackendVoice>(m_booEngine, clientVox, sampleRate, dynamicPitch);
}

std::unique_ptr<IBackendSubmix> BooBackendVoiceAllocator::allocateSubmix(Submix& clientSmx, bool mainOut, int busId) {
  return std::make_unique<BooBackendSubmix>(m_booEngine, clientSmx, mainOut, busId);
}

std::vector<std::pair<std::string, std::string>> BooBackendVoiceAllocator::enumerateMIDIDevices() {
  return m_booEngine.enumerateMIDIInputs();
}

std::unique_ptr<IMIDIReader> BooBackendVoiceAllocator::allocateMIDIReader(Engine& engine) {
  return std::make_unique<BooBackendMIDIReader>(engine, m_booEngine.useMIDILock());
}

void BooBackendVoiceAllocator::setCallbackInterface(Engine* engine) { m_cbInterface = engine; }

AudioChannelSet BooBackendVoiceAllocator::getAvailableSet() { return AudioChannelSet(m_booEngine.getAvailableSet()); }

void BooBackendVoiceAllocator::setVolume(float vol) { m_booEngine.setVolume(vol); }

void BooBackendVoiceAllocator::on5MsInterval(boo::IAudioVoiceEngine& engine, double dt) {
  if (m_cbInterface)
    m_cbInterface->_on5MsInterval(*this, dt);
}

void BooBackendVoiceAllocator::onPumpCycleComplete(boo::IAudioVoiceEngine& engine) {
  if (m_cbInterface)
    m_cbInterface->_onPumpCycleComplete(*this);
}
} // namespace amuse

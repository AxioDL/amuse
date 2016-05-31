#include "amuse/Voice.hpp"
#include "amuse/Submix.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/Common.hpp"
#include "amuse/Engine.hpp"
#include "amuse/DSPCodec.h"
#include "amuse/N64MusyXCodec.h"
#include <cmath>
#include <string.h>

namespace amuse
{
extern "C" const float VolumeLUT[];

void Voice::_destroy()
{
    Entity::_destroy();

    for (std::shared_ptr<Voice>& vox : m_childVoices)
        vox->_destroy();
}

Voice::~Voice()
{
    //fprintf(stderr, "DEALLOC %d\n", m_vid);
}

Voice::Voice(Engine& engine, const AudioGroup& group, int groupId, int vid, bool emitter, Submix* smx)
: Entity(engine, group, groupId), m_vid(vid), m_emitter(emitter), m_submix(smx)
{
    //fprintf(stderr, "ALLOC %d\n", m_vid);
}

Voice::Voice(Engine& engine, const AudioGroup& group, int groupId, ObjectId oid, int vid, bool emitter, Submix* smx)
: Entity(engine, group, groupId, oid), m_vid(vid), m_emitter(emitter), m_submix(smx)
{
    //fprintf(stderr, "ALLOC %d\n", m_vid);
}

void Voice::_reset()
{
    m_curAftertouch = 0;
    m_pitchWheelUp = 600;
    m_pitchWheelDown = 600;
    m_pitchWheelVal = 0;
    m_pitchDirty = true;
    m_pitchSweep1 = 0;
    m_pitchSweep1Times = 0;
    m_pitchSweep1It = 0;
    m_pitchSweep2 = 0;
    m_pitchSweep2Times = 0;
    m_pitchSweep2It = 0;
    m_envelopeTime = -1.f;
    m_panningTime = -1.f;
    m_spanningTime = -1.f;
    m_vibratoLevel = 0;
    m_vibratoModLevel = 0;
    m_vibratoPeriod = 0.f;
    m_tremoloScale = 0.f;
    m_tremoloModScale = 0.f;
    m_lfoPeriods[0] = 0.f;
    m_lfoPeriods[1] = 0.f;
}

void Voice::_macroSampleEnd()
{
    if (m_sampleEndTrap.macroId != 0xffff)
    {
        if (m_sampleEndTrap.macroId == m_state.m_header.m_macroId)
        {
            m_state.m_pc.back().second = m_sampleEndTrap.macroStep;
            m_state.m_inWait = false;
        }
        else
            loadSoundObject(m_sampleEndTrap.macroId, m_sampleEndTrap.macroStep,
                            m_state.m_ticksPerSec, m_state.m_initKey,
                            m_state.m_initVel, m_state.m_initMod);
    }
    else
        m_state.sampleEndNotify(*this);
}

bool Voice::_checkSamplePos(bool& looped)
{
    looped = false;
    if (!m_curSample)
        return true;

    if (m_curSamplePos >= m_lastSamplePos)
    {
        if (m_curSample->first.m_loopLengthSamples)
        {
            /* Turn over looped sample */
            m_curSamplePos = m_curSample->first.m_loopStartSample;
            if (m_curFormat == SampleFormat::DSP)
            {
                m_prev1 = m_curSample->second.dsp.m_hist1;
                m_prev2 = m_curSample->second.dsp.m_hist2;
            }
            looped = true;
        }
        else
        {
            /* Notify sample end */
            _macroSampleEnd();
            m_curSample = nullptr;
            return true;
        }
    }

    /* Looped samples issue sample end when ADSR envelope complete */
    if (m_volAdsr.isComplete())
    {
        _macroSampleEnd();
        m_curSample = nullptr;
        return true;
    }

    return false;
}

void Voice::_doKeyOff()
{
    m_volAdsr.keyOff();
    m_pitchAdsr.keyOff();
    m_state.keyoffNotify(*this);
}

void Voice::_setTotalPitch(int32_t cents, bool slew)
{
    //fprintf(stderr, "PITCH %d\n", cents);
    int32_t interval = cents - m_curSample->first.m_pitch * 100;
    double ratio = std::exp2(interval / 1200.0);
    m_sampleRate = m_curSample->first.m_sampleRate * ratio;
    m_backendVoice->setPitchRatio(ratio, slew);
}

bool Voice::_isRecursivelyDead()
{
    if (m_voxState != VoiceState::Dead)
        return false;
    for (std::shared_ptr<Voice>& vox : m_childVoices)
        if (!vox->_isRecursivelyDead())
            return false;
    return true;
}

void Voice::_bringOutYourDead()
{
    for (auto it = m_childVoices.begin() ; it != m_childVoices.end() ;)
    {
        Voice* vox = it->get();
        vox->_bringOutYourDead();
        if (vox->_isRecursivelyDead())
        {
            it = _destroyVoice(vox);
            continue;
        }
        ++it;
    }
}

std::shared_ptr<Voice> Voice::_findVoice(int vid, std::weak_ptr<Voice> thisPtr)
{
    if (m_vid == vid)
        return thisPtr.lock();

    for (std::shared_ptr<Voice>& vox : m_childVoices)
    {
        std::shared_ptr<Voice> ret = vox->_findVoice(vid, vox);
        if (ret)
            return ret;
    }

    return {};
}

std::unique_ptr<int8_t[]>& Voice::_ensureCtrlVals()
{
    if (m_ctrlValsSelf)
        return m_ctrlValsSelf;
    m_ctrlValsSelf.reset(new int8_t[128]);
    memset(m_ctrlValsSelf.get(), 0, 128);
    return m_ctrlValsSelf;
}

std::shared_ptr<Voice> Voice::_allocateVoice(double sampleRate, bool dynamicPitch)
{
    auto it = m_childVoices.emplace(m_childVoices.end(), new Voice(m_engine, m_audioGroup,
                                    m_groupId, m_engine.m_nextVid++, m_emitter, m_submix));
    if (m_submix)
        m_childVoices.back()->m_backendVoice =
            m_submix->m_backendSubmix->allocateVoice(*m_childVoices.back(), sampleRate, dynamicPitch);
    else
        m_childVoices.back()->m_backendVoice =
            m_engine.getBackend().allocateVoice(*m_childVoices.back(), sampleRate, dynamicPitch);
    m_childVoices.back()->m_engineIt = it;
    return m_childVoices.back();
}

std::list<std::shared_ptr<Voice>>::iterator Voice::_destroyVoice(Voice* voice)
{
    if (voice->m_destroyed)
        return m_childVoices.begin();

    voice->_destroy();
    return m_childVoices.erase(voice->m_engineIt);
}

static void ApplyVolume(float vol, int16_t& samp)
{
    /* -10dB to 0dB mapped to full volume range */
    samp *= VolumeLUT[int(vol * 65536)];
}

bool Voice::_advanceSample(int16_t& samp, int32_t& newPitch)
{
    double dt;

    /* Block linearized will use a larger `dt` for amplitude sampling;
     * significantly reducing the processing expense */
    switch (m_engine.m_ampMode)
    {
    case AmplitudeMode::PerSample:
        m_voiceSamples += 1;
        dt = 1.0 / m_sampleRate;
        break;
    case AmplitudeMode::BlockLinearized:
    {
        uint32_t rem = m_voiceSamples % 160;
        m_voiceSamples += 1;
        if (rem != 0)
        {
            /* Lerp within 160-sample block */
            float t = rem / 160.f;
            float l = m_lastLevel * (1.f - t) + m_nextLevel * t;

            /* Apply total volume to sample using decibel scale */
            ApplyVolume(l, samp);
            return false;
        }

        dt = 160.0 / m_sampleRate;
        break;
    }
    }

    m_voiceTime += dt;
    bool refresh = false;

    /* Process active envelope */
    if (m_envelopeTime >= 0.0)
    {
        m_envelopeTime += dt;
        float start = m_envelopeStart / 127.f;
        float end = m_envelopeEnd / 127.f;
        double t = std::max(0.0, std::min(1.0, m_envelopeTime / m_envelopeDur));
        if (m_envelopeCurve)
            t = (*m_envelopeCurve)[int(t*127.f)] / 127.f;
        m_curVol = (start * (1.0f - t)) + (end * t);

        /* Done with envelope */
        if (m_envelopeTime > m_envelopeDur)
            m_envelopeTime = -1.f;
    }

    /* Factor in ADSR envelope state */
    float adsr = m_volAdsr.advance(dt);
    m_lastLevel = m_nextLevel;
    m_nextLevel = m_userVol * m_curVol * adsr * (m_state.m_curVel / 127.f);

    /* Apply tremolo */
    if (m_state.m_tremoloSel && (m_tremoloScale || m_tremoloModScale))
    {
        float t = m_state.m_tremoloSel.evaluate(*this, m_state);
        if (m_tremoloScale && m_tremoloModScale)
        {
            float fac = (1.0f - t) + (m_tremoloScale * t);
            float modT = getModWheel() / 127.f;
            float modFac = (1.0f - modT) + (m_tremoloModScale * modT);
            m_nextLevel *= fac * modFac;
        }
        else if (m_tremoloScale)
        {
            float fac = (1.0f - t) + (m_tremoloScale * t);
            m_nextLevel *= fac;
        }
        else if (m_tremoloModScale)
        {
            float modT = getModWheel() / 127.f;
            float modFac = (1.0f - modT) + (m_tremoloModScale * modT);
            m_nextLevel *= modFac;
        }
    }

    m_nextLevel = ClampFull<float>(m_nextLevel);

    /* Apply total volume to sample using decibel scale */
    ApplyVolume(m_nextLevel, samp);

    /* Process active pan-sweep */
    if (m_panningTime >= 0.f)
    {
        m_panningTime += dt;
        float start = (m_panPos - 64) / 64.f;
        float end = (m_panPos + m_panWidth - 64) / 64.f;
        float t = std::max(0.f, std::min(1.f, m_panningTime / m_panningDur));
        _setPan((start * (1.0f - t)) + (end * t));
        refresh = true;

        /* Done with panning */
        if (m_panningTime > m_panningDur)
            m_panningTime = -1.f;
    }

    /* Process active span-sweep */
    if (m_spanningTime >= 0.f)
    {
        m_spanningTime += dt;
        float start = (m_spanPos - 64) / 64.f;
        float end = (m_spanPos + m_spanWidth - 64) / 64.f;
        float t = std::max(0.f, std::min(1.f, m_spanningTime / m_spanningDur));
        _setSurroundPan((start * (1.0f - t)) + (end * t));
        refresh = true;

        /* Done with spanning */
        if (m_spanningTime > m_spanningDur)
            m_spanningTime = -1.f;
    }

    /* Calculate total pitch */
    newPitch = m_curPitch;
    refresh |= m_pitchDirty;
    m_pitchDirty = false;
    if (m_pitchEnv)
    {
        newPitch = m_curPitch * m_pitchAdsr.advance(dt);
        refresh = true;
    }

    /* Process pitch sweep 1 */
    if (m_pitchSweep1It < m_pitchSweep1Times)
    {
        ++m_pitchSweep1It;
        m_pitchSweep1 = m_pitchSweep1Add * m_pitchSweep1It / m_pitchSweep1Times;
        refresh = true;
    }

    /* Process pitch sweep 2 */
    if (m_pitchSweep2It < m_pitchSweep2Times)
    {
        ++m_pitchSweep2It;
        m_pitchSweep2 = m_pitchSweep2Add * m_pitchSweep2It / m_pitchSweep2Times;
        refresh = true;
    }

    /* True if backend voice needs reconfiguration before next sample */
    return refresh;
}

uint32_t Voice::_GetBlockSampleCount(SampleFormat fmt)
{
    switch (fmt)
    {
    default:
        return 1;
    case Voice::SampleFormat::DSP:
        return 14;
    case Voice::SampleFormat::N64:
        return 64;
    }
}

size_t Voice::supplyAudio(size_t samples, int16_t* data)
{
    uint32_t samplesRem = samples;
    size_t samplesProc = 0;

    /* Process SoundMacro; bootstrapping sample if needed */
    bool dead = m_state.advance(*this, samples / m_sampleRate);

    if (m_curSample)
    {
        uint32_t blockSampleCount = _GetBlockSampleCount(m_curFormat);
        uint32_t block;
        int32_t curPitch = m_curPitch;
        bool refresh = false;

        bool looped = true;
        while (looped && samplesRem)
        {
            block = m_curSamplePos / blockSampleCount;
            uint32_t rem = m_curSamplePos % blockSampleCount;

            if (rem)
            {
                uint32_t remCount = std::min(samplesRem, m_lastSamplePos - block * blockSampleCount);
                uint32_t decSamples;

                switch (m_curFormat)
                {
                case SampleFormat::DSP:
                {
                    decSamples = DSPDecompressFrameRanged(data, m_curSampleData + 8 * block,
                                                          m_curSample->second.dsp.m_coefs,
                                                          &m_prev1, &m_prev2, rem, remCount);
                    break;
                }
                case SampleFormat::N64:
                {
                    decSamples = N64MusyXDecompressFrameRanged(data, m_curSampleData + 256 + 40 * block,
                                                               m_curSample->second.vadpcm.m_coefs,
                                                               rem, remCount);
                    break;
                }
                case SampleFormat::PCM:
                {
                    const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                    remCount = std::min(samplesRem, m_lastSamplePos - m_curSamplePos);
                    for (uint32_t i=0 ; i<remCount ; ++i)
                        data[i] = SBig(pcm[m_curSamplePos+i]);
                    decSamples = remCount;
                    break;
                }
                case SampleFormat::PCM_PC:
                {
                    const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                    remCount = std::min(samplesRem, m_lastSamplePos - m_curSamplePos);
                    memcpy(data, pcm + m_curSamplePos, remCount * sizeof(int16_t));
                    decSamples = remCount;
                    break;
                }
                default:
                    memset(data, 0, sizeof(int16_t) * samples);
                    return samples;
                }

                /* Per-sample processing */
                for (uint32_t i=0 ; i<decSamples ; ++i)
                {
                    ++samplesProc;
                    ++m_curSamplePos;
                    refresh |= _advanceSample(data[i], curPitch);
                }

                samplesRem -= decSamples;
                data += decSamples;
            }

            if (_checkSamplePos(looped))
            {
                if (samplesRem)
                    memset(data, 0, sizeof(int16_t) * samplesRem);
                return samples;
            }
            if (looped)
                continue;

            while (samplesRem)
            {
                block = m_curSamplePos / blockSampleCount;
                uint32_t remCount = std::min(samplesRem, m_lastSamplePos - block * blockSampleCount);
                uint32_t decSamples;

                switch (m_curFormat)
                {
                    case SampleFormat::DSP:
                    {
                        decSamples = DSPDecompressFrame(data, m_curSampleData + 8 * block,
                                                        m_curSample->second.dsp.m_coefs,
                                                        &m_prev1, &m_prev2, remCount);
                        break;
                    }
                    case SampleFormat::N64:
                    {
                        decSamples = N64MusyXDecompressFrame(data, m_curSampleData + 256 + 40 * block,
                                                             m_curSample->second.vadpcm.m_coefs,
                                                             remCount);
                        break;
                    }
                    case SampleFormat::PCM:
                    {
                        const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                        remCount = std::min(samplesRem, m_lastSamplePos - m_curSamplePos);
                        for (uint32_t i=0 ; i<remCount ; ++i)
                            data[i] = SBig(pcm[m_curSamplePos+i]);
                        decSamples = remCount;
                        break;
                    }
                    case SampleFormat::PCM_PC:
                    {
                        const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                        remCount = std::min(samplesRem, m_lastSamplePos - m_curSamplePos);
                        memcpy(data, pcm + m_curSamplePos, remCount * sizeof(int16_t));
                        decSamples = remCount;
                        break;
                    }
                    default:
                        memset(data, 0, sizeof(int16_t) * samples);
                        return samples;
                }

                /* Per-sample processing */
                for (uint32_t i=0 ; i<decSamples ; ++i)
                {
                    ++samplesProc;
                    ++m_curSamplePos;
                    refresh |= _advanceSample(data[i], curPitch);
                }

                samplesRem -= decSamples;
                data += decSamples;

                if (_checkSamplePos(looped))
                {
                    if (samplesRem)
                        memset(data, 0, sizeof(int16_t) * samplesRem);
                    return samples;
                }
                if (looped)
                    break;
            }
        }

        if (refresh)
            _setTotalPitch(curPitch + m_pitchSweep1 + m_pitchSweep2 + m_pitchWheelVal, true);
    }
    else
        memset(data, 0, sizeof(int16_t) * samples);

    if (dead && (!m_curSample || m_voxState == VoiceState::KeyOff) &&
        m_sampleEndTrap.macroId == 0xffff &&
        m_messageTrap.macroId == 0xffff &&
        (!m_curSample || (m_curSample && m_volAdsr.isComplete())))
    {
        m_curSample = nullptr;
        m_voxState = VoiceState::Dead;
        m_backendVoice->stop();
    }
    return samples;
}

int Voice::maxVid() const
{
    int maxVid = m_vid;
    for (const std::shared_ptr<Voice>& vox : m_childVoices)
        maxVid = std::max(maxVid, vox->maxVid());
    return maxVid;
}

std::shared_ptr<Voice> Voice::_startChildMacro(ObjectId macroId, int macroStep, double ticksPerSec,
                                               uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc)
{
    std::shared_ptr<Voice> vox = _allocateVoice(32000.0, true);
    if (!vox->loadSoundObject(macroId, macroStep, ticksPerSec, midiKey,
                              midiVel, midiMod, pushPc))
    {
        _destroyVoice(vox.get());
        return {};
    }
    vox->setVolume(m_userVol);
    vox->setPan(m_userPan);
    vox->setSurroundPan(m_userSpan);
    return vox;
}

std::shared_ptr<Voice> Voice::startChildMacro(int8_t addNote, ObjectId macroId, int macroStep)
{
    return _startChildMacro(macroId, macroStep, 1000.0, m_state.m_initKey + addNote,
                            m_state.m_initVel, m_state.m_initMod);
}

bool Voice::_loadSoundMacro(const unsigned char* macroData, int macroStep, double ticksPerSec,
                            uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc)
{
    if (m_state.m_pc.empty())
        m_state.initialize(macroData, macroStep, ticksPerSec, midiKey, midiVel, midiMod,
                           m_audioGroup.getDataFormat() != DataFormat::PC);
    else
    {
        if (!pushPc)
            m_state.m_pc.clear();
        m_state.m_pc.push_back({macroData, macroStep});
        m_state.m_header = *reinterpret_cast<const SoundMacroState::Header*>(macroData);
        if (m_audioGroup.getDataFormat() != DataFormat::PC)
            m_state.m_header.swapBig();
    }

    m_voxState = VoiceState::Playing;
    m_backendVoice->start();
    return true;
}

bool Voice::_loadKeymap(const Keymap* keymap, int macroStep, double ticksPerSec,
                        uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc)
{
    const Keymap& km = keymap[midiKey];
    ObjectId oid = (m_audioGroup.getDataFormat() == DataFormat::PC) ? km.objectId : SBig(km.objectId);
    midiKey += km.transpose;
    bool ret = loadSoundObject(oid, macroStep, ticksPerSec, midiKey, midiVel, midiMod, pushPc);
    m_curVol = 1.f;
    _setPan((km.pan - 64) / 64.f);
    _setSurroundPan(-1.f);
    return ret;
}

bool Voice::_loadLayer(const std::vector<const LayerMapping*>& layer, int macroStep, double ticksPerSec,
                       uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc)
{
    bool ret = false;
    for (const LayerMapping* mapping : layer)
    {
        if (midiKey >= mapping->keyLo && midiKey <= mapping->keyHi)
        {
            ObjectId oid = (m_audioGroup.getDataFormat() == DataFormat::PC) ? mapping->objectId : SBig(mapping->objectId);
            uint8_t mappingKey = midiKey + mapping->transpose;
            if (m_voxState != VoiceState::Playing)
            {
                ret |= loadSoundObject(oid, macroStep, ticksPerSec,
                                       mappingKey, midiVel, midiMod, pushPc);
                m_curVol = mapping->volume / 127.f;
                _setPan((mapping->pan - 64) / 64.f);
                _setSurroundPan((mapping->span - 64) / 64.f);
            }
            else
            {
                std::shared_ptr<Voice> vox =
                    _startChildMacro(oid, macroStep, ticksPerSec,
                                     mappingKey, midiVel, midiMod, pushPc);
                if (vox)
                {
                    vox->m_curVol = mapping->volume / 127.f;
                    vox->_setPan((mapping->pan - 64) / 64.f);
                    vox->_setSurroundPan((mapping->span - 64) / 64.f);
                    ret = true;
                }
            }
        }
    }
    return ret;
}

bool Voice::loadSoundObject(ObjectId objectId, int macroStep, double ticksPerSec,
                            uint8_t midiKey, uint8_t midiVel, uint8_t midiMod,
                            bool pushPc)
{
    const unsigned char* macroData = m_audioGroup.getPool().soundMacro(objectId);
    if (macroData)
        return _loadSoundMacro(macroData, macroStep, ticksPerSec, midiKey, midiVel, midiMod, pushPc);

    const Keymap* keymap = m_audioGroup.getPool().keymap(objectId);
    if (keymap)
        return _loadKeymap(keymap, macroStep, ticksPerSec, midiKey, midiVel, midiMod, pushPc);

    const std::vector<const LayerMapping*>* layer = m_audioGroup.getPool().layer(objectId);
    if (layer)
        return _loadLayer(*layer, macroStep, ticksPerSec, midiKey, midiVel, midiMod, pushPc);

    return false;
}

void Voice::_macroKeyOff()
{
    if (m_voxState == VoiceState::Playing)
    {
        if (m_sustained)
            m_sustainKeyOff = true;
        else
            _doKeyOff();
        m_voxState = VoiceState::KeyOff;
    }
}

void Voice::keyOff()
{
    if (m_keyoffTrap.macroId != 0xffff)
    {
        if (m_keyoffTrap.macroId == m_state.m_header.m_macroId)
        {
            m_state.m_pc.back().second = m_keyoffTrap.macroStep;
            m_state.m_inWait = false;
        }
        else
            loadSoundObject(m_keyoffTrap.macroId, m_keyoffTrap.macroStep,
                            m_state.m_ticksPerSec, m_state.m_initKey,
                            m_state.m_initVel, m_state.m_initMod);
    }
    else
        _macroKeyOff();

    for (const std::shared_ptr<Voice>& vox : m_childVoices)
        vox->keyOff();
}

void Voice::message(int32_t val)
{
    m_messageQueue.push_back(val);

    if (m_messageTrap.macroId != 0xffff)
    {
        if (m_messageTrap.macroId == m_state.m_header.m_macroId)
            m_state.m_pc.back().second = m_messageTrap.macroStep;
        else
            loadSoundObject(m_messageTrap.macroId, m_messageTrap.macroStep,
                            m_state.m_ticksPerSec, m_state.m_initKey,
                            m_state.m_initVel, m_state.m_initMod);
    }
}

void Voice::startSample(int16_t sampId, int32_t offset)
{
    m_curSample = m_audioGroup.getSample(sampId);
    if (m_curSample)
    {
        _reset();
        m_sampleRate = m_curSample->first.m_sampleRate;
        m_curPitch = m_curSample->first.m_pitch;
        setPitchWheel(m_curPitchWheel);
        m_backendVoice->resetSampleRate(m_curSample->first.m_sampleRate);

        int32_t numSamples = m_curSample->first.m_numSamples & 0xffffff;
        if (offset)
        {
            if (m_curSample->first.m_loopLengthSamples)
            {
                if (offset > int32_t(m_curSample->first.m_loopStartSample))
                    offset = ((offset - m_curSample->first.m_loopStartSample) %
                              m_curSample->first.m_loopLengthSamples) +
                              m_curSample->first.m_loopStartSample;
            }
            else
                offset = clamp(0, offset, numSamples);
        }
        m_curSamplePos = offset;
        m_curSampleData = m_audioGroup.getSampleData(m_curSample->first.m_sampleOff);
        m_prev1 = 0;
        m_prev2 = 0;

        if (m_audioGroup.getDataFormat() == DataFormat::PC)
            m_curFormat = SampleFormat::PCM_PC;
        else
            m_curFormat = SampleFormat(m_curSample->first.m_numSamples >> 24);

        if (m_curFormat == SampleFormat::DSP_DRUM)
            m_curFormat = SampleFormat::DSP;

        m_lastSamplePos = m_curSample->first.m_loopLengthSamples ?
            (m_curSample->first.m_loopStartSample + m_curSample->first.m_loopLengthSamples) : numSamples;

        bool looped;
        _checkSamplePos(looped);

        /* Seek DSPADPCM state if needed */
        if (m_curSample && m_curSamplePos && m_curFormat == SampleFormat::DSP)
        {
            uint32_t block = m_curSamplePos / 14;
            uint32_t rem = m_curSamplePos % 14;
            for (uint32_t b = 0 ; b < block ; ++b)
                DSPDecompressFrameStateOnly(m_curSampleData + 8 * b, m_curSample->second.dsp.m_coefs,
                                            &m_prev1, &m_prev2, 14);

            if (rem)
                DSPDecompressFrameStateOnly(m_curSampleData + 8 * block, m_curSample->second.dsp.m_coefs,
                                            &m_prev1, &m_prev2, rem);
        }
    }
}

void Voice::stopSample()
{
    m_curSample = nullptr;
}

void Voice::setVolume(float vol)
{
    m_userVol = clamp(0.f, vol, 1.f);
    for (std::shared_ptr<Voice>& vox : m_childVoices)
        vox->setVolume(vol);
}

void Voice::_setPan(float pan)
{
    m_curPan = clamp(-1.f, pan, 1.f);
    float totalPan = clamp(-1.f, m_curPan + m_userPan, 1.f);
    float totalSpan = clamp(-1.f, m_curSpan + m_userSpan, 1.f);
    float coefs[8];

    /* Left */
    coefs[0] = (totalPan <= 0.f) ? 1.f : (1.f - totalPan);
    coefs[0] *= (totalSpan <= 0.f) ? 1.f : (1.f - totalSpan);

    /* Right */
    coefs[1] = (totalPan >= 0.f) ? 1.f : (1.f + totalPan);
    coefs[1] *= (totalSpan <= 0.f) ? 1.f : (1.f - totalSpan);

    /* Back Left */
    coefs[2] = (totalPan <= 0.f) ? 1.f : (1.f - totalPan);
    coefs[2] *= (totalSpan >= 0.f) ? 1.f : (1.f + totalSpan);

    /* Back Right */
    coefs[3] = (totalPan >= 0.f) ? 1.f : (1.f + totalPan);
    coefs[3] *= (totalSpan >= 0.f) ? 1.f : (1.f + totalSpan);

    /* Center */
    coefs[4] = 1.f - std::fabs(totalPan);

    /* LFE */
    coefs[5] = 1.f;

    /* Side Left */
    coefs[6] = (totalPan <= 0.f) ? 1.f : (1.f - totalPan);
    coefs[6] *= 1.f - std::fabs(totalSpan);

    /* Side Right */
    coefs[7] = (totalPan >= 0.f) ? 1.f : (1.f + totalPan);
    coefs[7] *= 1.f - std::fabs(totalSpan);

    m_backendVoice->setMatrixCoefficients(coefs, true);

    for (int i=0 ; i<8 ; ++i)
        coefs[i] *= m_curReverbVol;
    m_backendVoice->setSubmixMatrixCoefficients(coefs, true);
}

void Voice::setPan(float pan)
{
    m_userPan = pan;
    _setPan(m_curPan);
    for (std::shared_ptr<Voice>& vox : m_childVoices)
        vox->setPan(pan);
}

void Voice::_setSurroundPan(float span)
{
    m_curSpan = clamp(-1.f, span, 1.f);
    _setPan(m_curPan);
}

void Voice::setSurroundPan(float span)
{
    m_userSpan = span;
    _setSurroundPan(m_curSpan);
    for (std::shared_ptr<Voice>& vox : m_childVoices)
        vox->setSurroundPan(span);
}

void Voice::startEnvelope(double dur, float vol, const Curve* envCurve)
{
    m_envelopeTime = m_voiceTime;
    m_envelopeDur = dur;
    m_envelopeStart = clamp(0.f, m_curVol, 1.f);
    m_envelopeEnd = clamp(0.f, vol, 1.f);
    m_envelopeCurve = envCurve;
}

void Voice::startFadeIn(double dur, float vol, const Curve* envCurve)
{
    m_envelopeTime = m_voiceTime;
    m_envelopeDur = dur;
    m_envelopeStart = 0.f;
    m_envelopeEnd = clamp(0.f, vol, 1.f);
    m_envelopeCurve = envCurve;
}

void Voice::startPanning(double dur, uint8_t panPos, int8_t panWidth)
{
    m_panningTime = m_voiceTime;
    m_panningDur = dur;
    m_panPos = panPos;
    m_panWidth = panWidth;
}

void Voice::startSpanning(double dur, uint8_t spanPos, int8_t spanWidth)
{
    m_spanningTime = m_voiceTime;
    m_spanningDur = dur;
    m_spanPos = spanPos;
    m_spanWidth = spanWidth;
}

void Voice::setPitchKey(int32_t cents)
{
    m_curPitch = cents;
    m_pitchDirty = true;
}

void Voice::setPedal(bool pedal)
{
    if (m_sustained && !pedal && m_sustainKeyOff)
    {
        m_sustainKeyOff = false;
        _doKeyOff();
    }
    m_sustained = pedal;

    for (std::shared_ptr<Voice>& vox : m_childVoices)
        vox->setPedal(pedal);
}

void Voice::setDoppler(float)
{
}

void Voice::setVibrato(int32_t level, int32_t modLevel, float period)
{
    m_vibratoLevel = level;
    m_vibratoModLevel = modLevel;
    m_vibratoPeriod = period;
}

void Voice::setMod2VibratoRange(int32_t modLevel)
{
    m_vibratoModLevel = modLevel;
}

void Voice::setTremolo(float tremoloScale, float tremoloModScale)
{
    m_tremoloScale = tremoloScale;
    m_tremoloModScale = tremoloModScale;
}

void Voice::setPitchSweep1(uint8_t times, int16_t add)
{
    m_pitchSweep1 = 0;
    m_pitchSweep1It = 0;
    m_pitchSweep1Times = times * 160;
    m_pitchSweep1Add = add;
}

void Voice::setPitchSweep2(uint8_t times, int16_t add)
{
    m_pitchSweep2 = 0;
    m_pitchSweep2It = 0;
    m_pitchSweep2Times = times * 160;
    m_pitchSweep2Add = add;
}

void Voice::setReverbVol(float rvol)
{
    m_curReverbVol = clamp(0.f, rvol, 1.f);
    _setPan(m_curPan);
    for (std::shared_ptr<Voice>& vox : m_childVoices)
        vox->setReverbVol(rvol);
}

void Voice::setAdsr(ObjectId adsrId, bool dls)
{
    if (dls)
    {
        const ADSRDLS* adsr = m_audioGroup.getPool().tableAsAdsrDLS(adsrId);
        if (adsr)
        {
            m_volAdsr.reset(adsr, m_state.m_initKey, m_state.m_initVel);
            if (m_voxState == VoiceState::KeyOff)
                m_volAdsr.keyOff();
        }
    }
    else
    {
        const ADSR* adsr = m_audioGroup.getPool().tableAsAdsr(adsrId);
        if (adsr)
        {
            m_volAdsr.reset(adsr);
            if (m_voxState == VoiceState::KeyOff)
                m_volAdsr.keyOff();
        }
    }
}

void Voice::setPitchFrequency(uint32_t hz, uint16_t fine)
{
    m_sampleRate = hz + fine / 65536.0;
    m_backendVoice->setPitchRatio(1.0, false);
    m_backendVoice->resetSampleRate(m_sampleRate);
}

void Voice::setPitchAdsr(ObjectId adsrId, int32_t cents)
{
    const ADSRDLS* adsr = m_audioGroup.getPool().tableAsAdsrDLS(adsrId);
    if (adsr)
    {
        m_pitchAdsr.reset(adsr, m_state.m_initKey, m_state.m_initVel);
        m_pitchEnvRange = cents;
        m_pitchEnv = true;
    }
}

void Voice::setPitchWheel(float pitchWheel)
{
    m_curPitchWheel = amuse::clamp(-1.f, pitchWheel, 1.f);
    if (pitchWheel > 0.f)
        m_pitchWheelVal = m_pitchWheelUp * m_curPitchWheel;
    else if (pitchWheel < 0.f)
        m_pitchWheelVal = m_pitchWheelDown * m_curPitchWheel;
    else
        m_pitchWheelVal = 0;
    m_pitchDirty = true;

    for (std::shared_ptr<Voice>& vox : m_childVoices)
        vox->setPitchWheel(pitchWheel);
}

void Voice::setPitchWheelRange(int8_t up, int8_t down)
{
    m_pitchWheelUp = up * 100;
    m_pitchWheelDown = down * 100;
    setPitchWheel(m_curPitchWheel);
}

void Voice::setAftertouch(uint8_t aftertouch)
{
    m_curAftertouch = aftertouch;
    for (std::shared_ptr<Voice>& vox : m_childVoices)
        vox->setAftertouch(aftertouch);
}

void Voice::_notifyCtrlChange(uint8_t ctrl, int8_t val)
{
    if (ctrl == 64)
    {
        if (val >= 64)
            setPedal(true);
        else
            setPedal(false);
    }
    else if (ctrl == 0x5b)
        setReverbVol(val / 127.f);

    for (std::shared_ptr<Voice>& vox : m_childVoices)
        vox->_notifyCtrlChange(ctrl, val);
}

size_t Voice::getTotalVoices() const
{
    size_t ret = 1;
    for (const std::shared_ptr<Voice>& vox : m_childVoices)
        ret += vox->getTotalVoices();
    return ret;
}

}

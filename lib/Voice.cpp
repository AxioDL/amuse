#include "amuse/Voice.hpp"
#include "amuse/Submix.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/Common.hpp"
#include "amuse/Engine.hpp"
#include "amuse/dsp.h"
#include <cmath>
#include <string.h>

namespace amuse
{

void Voice::_destroy()
{
    Entity::_destroy();
    if (m_submix)
        m_submix->m_activeVoices.erase(this);
}

Voice::Voice(Engine& engine, const AudioGroup& group, int vid, bool emitter, Submix* smx)
: Entity(engine, group), m_vid(vid), m_emitter(emitter), m_submix(smx)
{
    if (m_submix)
        m_submix->m_activeVoices.insert(this);
}

Voice::Voice(Engine& engine, const AudioGroup& group, ObjectId oid, int vid, bool emitter, Submix* smx)
: Entity(engine, group, oid), m_vid(vid), m_emitter(emitter), m_submix(smx)
{
    if (m_submix)
        m_submix->m_activeVoices.insert(this);
}

void Voice::_reset()
{
    m_curVol = 1.f;
    m_curReverbVol = 0.f;
    m_curPan = 0.f;
    m_curSpan = 0.f;
    m_pitchSweep1 = 0;
    m_pitchSweep1Times = 0;
    m_pitchSweep2 = 0;
    m_pitchSweep2Times = 0;
    m_envelopeTime = -1.f;
    m_panningTime = -1.f;
    m_vibratoLevel = 0;
    m_vibratoModLevel = 0;
    m_vibratoPeriod = 0.f;
    m_tremoloScale = 0.f;
    m_tremoloModScale = 0.f;
    m_lfoPeriods[0] = 0.f;
    m_lfoPeriods[1] = 0.f;
}

bool Voice::_checkSamplePos()
{
    if (m_curSamplePos >= m_lastSamplePos)
    {
        if (m_curSample->first.m_loopLengthSamples)
        {
            /* Turn over looped sample */
            m_curSamplePos = m_curSample->first.m_loopStartSample;
            m_prev1 = m_curSample->second.m_hist1;
            m_prev2 = m_curSample->second.m_hist2;
        }
        else
        {
            /* Notify sample end */
            m_state.sampleEndNotify(*this);
            m_curSample = nullptr;
            return true;
        }
    }
    return false;
}

void Voice::_doKeyOff()
{
    m_volAdsr.keyOff();
    m_pitchAdsr.keyOff();
}

void Voice::_setTotalPitch(int32_t cents)
{
    int32_t interval = cents - m_curSample->first.m_pitch * 100;
    double ratio = std::exp2(interval / 1200.0);
    m_sampleRate = m_curSample->first.m_sampleRate * ratio;
    m_backendVoice->setPitchRatio(ratio);
}

void Voice::_bringOutYourDead()
{
    for (auto it = m_childVoices.begin() ; it != m_childVoices.end() ;)
    {
        Voice* vox = it->get();
        vox->_bringOutYourDead();
        if (vox->m_dead)
        {
            it = _destroyVoice(vox);
            continue;
        }
        ++it;
    }
}

std::shared_ptr<Voice> Voice::_allocateVoice(double sampleRate, bool dynamicPitch)
{
    auto it = m_childVoices.emplace(m_childVoices.end(), new Voice(m_engine, m_audioGroup,
                                    m_engine.m_nextVid++, m_emitter, m_submix));
    m_childVoices.back()->m_backendVoice =
        m_engine.getBackend().allocateVoice(*m_childVoices.back(), sampleRate, dynamicPitch);
    m_childVoices.back()->m_engineIt = it;
    return m_childVoices.back();
}

std::list<std::shared_ptr<Voice>>::iterator Voice::_destroyVoice(Voice* voice)
{
    voice->_destroy();
    return m_childVoices.erase(voice->m_engineIt);
}

bool Voice::_advanceSample(int16_t& samp)
{
    double dt = 1.0 / m_sampleRate;
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
    float totalVol = m_curVol * m_volAdsr.nextSample(m_sampleRate);

    /* Apply tremolo */
    if (m_state.m_tremoloSel && (m_tremoloScale || m_tremoloModScale))
    {
        float t = m_state.m_tremoloSel.evaluate(*this, m_state);
        if (m_tremoloScale && m_tremoloModScale)
        {
            float fac = (1.0f - t) + (m_tremoloScale * t);
            float modT = getModWheel() / 127.f;
            float modFac = (1.0f - modT) + (m_tremoloModScale * modT);
            totalVol *= fac * modFac;
        }
        else if (m_tremoloScale)
        {
            float fac = (1.0f - t) + (m_tremoloScale * t);
            totalVol *= fac;
        }
        else if (m_tremoloModScale)
        {
            float modT = getModWheel() / 127.f;
            float modFac = (1.0f - modT) + (m_tremoloModScale * modT);
            totalVol *= modFac;
        }
    }

    /* Multiply sample with total volume */
    samp = ClampFull<int16_t>(samp * totalVol);

    /* Process active pan-sweep */
    if (m_panningTime >= 0.f)
    {
        m_panningTime += dt;
        float start = (m_panPos - 64) / 64.f;
        float end = (m_panPos + m_panWidth - 64) / 64.f;
        float t = std::max(0.f, std::min(1.f, m_panningTime / m_panningDur));
        setPan((start * (1.0f - t)) + (end * t));
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
        setSurroundPan((start * (1.0f - t)) + (end * t));
        refresh = true;

        /* Done with spanning */
        if (m_spanningTime > m_spanningDur)
            m_spanningTime = -1.f;
    }

    /* Calculate total pitch */
    int32_t totalPitch = m_curPitch;
    bool pitchDirty = m_pitchDirty;
    m_pitchDirty = false;
    if (m_pitchEnv)
    {
        totalPitch = m_curPitch * m_pitchAdsr.nextSample(m_sampleRate);
        pitchDirty = true;
    }

    /* Process pitch sweep 1 */
    if (m_pitchSweep1Times)
    {
        m_pitchSweep1 += m_pitchSweep1Add;
        --m_pitchSweep1Times;
        pitchDirty = true;
    }

    /* Process pitch sweep 2 */
    if (m_pitchSweep2Times)
    {
        m_pitchSweep2 += m_pitchSweep2Add;
        --m_pitchSweep2Times;
        pitchDirty = true;
    }

    /* Apply total pitch */
    if (pitchDirty)
    {
        _setTotalPitch(totalPitch + m_pitchSweep1 + m_pitchSweep2);
        refresh = true;
    }

    /* True if backend voice needs reconfiguration before next sample */
    return refresh;
}

size_t Voice::supplyAudio(size_t samples, int16_t* data)
{
    uint32_t samplesRem = samples;
    size_t samplesProc = 0;
    if (m_curSample)
    {
        m_dead = m_state.advance(*this, samples / m_sampleRate);

        uint32_t block = m_curSamplePos / 14;
        uint32_t rem = m_curSamplePos % 14;

        if (rem)
        {
            uint32_t remCount = std::min(samplesRem, m_lastSamplePos - block * 14);
            uint32_t decSamples;

            switch (m_curFormat)
            {
            case SampleFormat::DSP:
            {
                decSamples = DSPDecompressFrameRanged(data, m_curSampleData + 8 * block,
                                                      m_curSample->second.m_coefs,
                                                      &m_prev1, &m_prev2, rem, remCount);
                break;
            }
            case SampleFormat::PCM:
            {
                const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                for (uint32_t i=0 ; i<remCount ; ++i)
                    data[i] = SBig(pcm[m_curSamplePos+i]);
                decSamples = remCount;
                break;
            }
            default: return 0;
            }

            /* Per-sample processing */
            for (int i=0 ; i<decSamples ; ++i)
            {
                ++samplesProc;
                ++m_curSamplePos;
                if (_advanceSample(data[i]))
                    return samplesProc;
            }

            samplesRem -= decSamples;
            data += decSamples;
        }

        if (_checkSamplePos())
        {
            if (samplesRem)
                memset(data, 0, sizeof(int16_t) * samplesRem);
            return samples;
        }

        while (samplesRem)
        {
            block = m_curSamplePos / 14;
            uint32_t remCount = std::min(samplesRem, m_lastSamplePos - block * 14);
            uint32_t decSamples;

            switch (m_curFormat)
            {
            case SampleFormat::DSP:
            {
                decSamples = DSPDecompressFrame(data, m_curSampleData + 8 * block,
                                                      m_curSample->second.m_coefs,
                                                      &m_prev1, &m_prev2, remCount);
                break;
            }
            case SampleFormat::PCM:
            {
                const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                for (uint32_t i=0 ; i<remCount ; ++i)
                    data[i] = SBig(pcm[m_curSamplePos+i]);
                decSamples = remCount;
                break;
            }
            default: return 0;
            }

            /* Per-sample processing */
            for (int i=0 ; i<decSamples ; ++i)
            {
                ++samplesProc;
                ++m_curSamplePos;
                if (_advanceSample(data[i]))
                    return samplesProc;
            }

            samplesRem -= decSamples;
            data += decSamples;

            if (_checkSamplePos())
            {
                if (samplesRem)
                    memset(data, 0, sizeof(int16_t) * samplesRem);
                return samples;
            }
        }
    }
    else
    {
        m_dead = m_state.advance(*this, 0.0);
        memset(data, 0, sizeof(int16_t) * samples);
    }

    if (m_dead)
        m_voxState = VoiceState::Finished;
    return samples;
}

int Voice::maxVid() const
{
    int maxVid = m_vid;
    for (const std::shared_ptr<Voice>& vox : m_childVoices)
        maxVid = std::max(maxVid, vox->maxVid());
    return maxVid;
}

std::shared_ptr<Voice> Voice::startChildMacro(int8_t addNote, ObjectId macroId, int macroStep)
{
    std::shared_ptr<Voice> vox = _allocateVoice(32000.0, true);
    vox->loadSoundMacro(macroId, macroStep, 1000.0, m_state.m_initKey + addNote,
                        m_state.m_initVel, m_state.m_initMod);
    return vox;
}

bool Voice::loadSoundMacro(ObjectId macroId, int macroStep, double ticksPerSec,
                           uint8_t midiKey, uint8_t midiVel, uint8_t midiMod,
                           bool pushPc)
{
    const unsigned char* macroData = m_audioGroup.getPool().soundMacro(macroId);
    if (!macroData)
        return false;

    if (m_state.m_pc.empty())
        m_state.initialize(macroData, macroStep, ticksPerSec, midiKey, midiVel, midiMod);
    else
    {
        if (!pushPc)
            m_state.m_pc.clear();
        m_state.m_pc.push_back({macroData, macroStep});
        m_state.m_header = *reinterpret_cast<const SoundMacroState::Header*>(macroData);
        m_state.m_header.swapBig();
    }

    m_voxState = VoiceState::Playing;
    return true;
}

void Voice::keyOff()
{
    if (m_sustained)
        m_sustainKeyOff = true;
    else
        _doKeyOff();

    for (const std::shared_ptr<Voice>& vox : m_childVoices)
        vox->keyOff();

    m_voxState = VoiceState::KeyOff;
}

void Voice::message(int32_t val)
{
}

void Voice::startSample(int16_t sampId, int32_t offset)
{
    m_curSample = m_audioGroup.getSample(sampId);
    if (m_curSample)
    {
        _reset();
        m_sampleRate = m_curSample->first.m_sampleRate;
        m_curPitch = m_curSample->first.m_pitch;
        m_pitchDirty = true;
        m_backendVoice->stop();
        m_backendVoice->resetSampleRate(m_curSample->first.m_sampleRate);
        m_curSamplePos = offset;
        m_curSampleData = m_audioGroup.getSampleData(m_curSample->first.m_sampleOff);
        m_prev1 = 0;
        m_prev2 = 0;
        uint32_t numSamples = m_curSample->first.m_numSamples & 0xffffff;
        SampleFormat fmt = SampleFormat(m_curSample->first.m_numSamples >> 24);
        m_lastSamplePos = m_curSample->first.m_loopLengthSamples ?
            (m_curSample->first.m_loopStartSample + m_curSample->first.m_loopLengthSamples) : numSamples;

        if (fmt != SampleFormat::DSP && fmt != SampleFormat::PCM)
        {
            m_curSample = nullptr;
            return;
        }

        _checkSamplePos();

        /* Seek DSPADPCM state if needed */
        if (m_curSamplePos && fmt == SampleFormat::DSP)
        {
            uint32_t block = m_curSamplePos / 14;
            uint32_t rem = m_curSamplePos % 14;
            for (uint32_t b = 0 ; b < block ; ++b)
                DSPDecompressFrameStateOnly(m_curSampleData + 8 * b, m_curSample->second.m_coefs,
                                            &m_prev1, &m_prev2, 14);

            if (rem)
                DSPDecompressFrameStateOnly(m_curSampleData + 8 * block, m_curSample->second.m_coefs,
                                            &m_prev1, &m_prev2, rem);
        }

        m_backendVoice->start();
    }
}

void Voice::stopSample()
{
    m_backendVoice->stop();
    m_curSample = nullptr;
}

void Voice::setVolume(float vol)
{
    m_curVol = vol;
}

void Voice::setPan(float pan)
{
    m_curPan = pan;
}

void Voice::setSurroundPan(float span)
{
    m_curSpan = span;
}

void Voice::startEnvelope(double dur, float vol, const Curve* envCurve)
{
    m_envelopeTime = m_voiceTime;
    m_envelopeDur = dur;
    m_envelopeStart = m_curVol;
    m_envelopeEnd = vol;
    m_envelopeCurve = envCurve;
}

void Voice::startFadeIn(double dur, float vol, const Curve* envCurve)
{
    m_envelopeTime = m_voiceTime;
    m_envelopeDur = dur;
    m_envelopeStart = 0.f;
    m_envelopeEnd = m_curVol;
    m_envelopeCurve = envCurve;
}

void Voice::startPanning(double dur, uint8_t panPos, uint8_t panWidth)
{
    m_panningTime = m_voiceTime;
    m_panningDur = dur;
    m_panPos = panPos;
    m_panWidth = panWidth;
}

void Voice::startSpanning(double dur, uint8_t spanPos, uint8_t spanWidth)
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

void Voice::setModulation(float mod)
{
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

void Voice::setDoppler(float doppler)
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
    m_pitchSweep1Times = times;
    m_pitchSweep1Add = add;
}

void Voice::setPitchSweep2(uint8_t times, int16_t add)
{
    m_pitchSweep2 = 0;
    m_pitchSweep2Times = times;
    m_pitchSweep2Add = add;
}

void Voice::setReverbVol(float rvol)
{
    m_curReverbVol = rvol;
}

void Voice::setAdsr(ObjectId adsrId)
{
    const ADSR* adsr = m_audioGroup.getPool().tableAsAdsr(adsrId);
    m_volAdsr.reset(adsr);
}

void Voice::setPitchFrequency(uint32_t hz, uint16_t fine)
{
    m_sampleRate = hz + fine / 65536.0;
    m_backendVoice->setPitchRatio(1.0);
    m_backendVoice->resetSampleRate(m_sampleRate);
}

void Voice::setPitchAdsr(ObjectId adsrId, int32_t cents)
{
    const ADSR* adsr = m_audioGroup.getPool().tableAsAdsr(adsrId);
    m_pitchAdsr.reset(adsr);
    m_pitchEnvRange = cents;
    m_pitchEnv = true;
}

void Voice::setPitchWheelRange(int8_t up, int8_t down)
{
}

}

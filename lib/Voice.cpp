#include "amuse/Voice.hpp"
#include "amuse/Submix.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/Common.hpp"
#include "amuse/Engine.hpp"
#include "amuse/DSPCodec.hpp"
#include "amuse/N64MusyXCodec.hpp"
#include <cmath>
#include <cstring>

namespace amuse
{

void Voice::_destroy()
{
    Entity::_destroy();

    for (auto& vox : m_childVoices)
        vox->_destroy();

    m_studio.reset();
    m_backendVoice.reset();
    m_curSample.reset();
}

Voice::~Voice()
{
    // fprintf(stderr, "DEALLOC %d\n", m_vid);
}

Voice::Voice(Engine& engine, const AudioGroup& group, int groupId, int vid, bool emitter, ObjToken<Studio> studio)
: Entity(engine, group, groupId), m_vid(vid), m_emitter(emitter), m_studio(studio)
{
    // fprintf(stderr, "ALLOC %d\n", m_vid);
}

Voice::Voice(Engine& engine, const AudioGroup& group, int groupId, ObjectId oid, int vid, bool emitter,
             ObjToken<Studio> studio)
: Entity(engine, group, groupId, oid), m_vid(vid), m_emitter(emitter), m_studio(studio)
{
    // fprintf(stderr, "ALLOC %d\n", m_vid);
}

void Voice::_macroSampleEnd()
{
    if (m_sampleEndTrap.macroId != 0xffff)
    {
        if (m_sampleEndTrap.macroId == std::get<0>(m_state.m_pc.back()))
        {
            std::get<2>(m_state.m_pc.back()) = std::get<1>(m_state.m_pc.back())->assertPC(m_sampleEndTrap.macroStep);
            m_state.m_inWait = false;
        }
        else
            loadMacroObject(m_sampleEndTrap.macroId, m_sampleEndTrap.macroStep, m_state.m_ticksPerSec,
                            m_state.m_initKey, m_state.m_initVel, m_state.m_initMod);
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
        if (m_curSample->m_loopLengthSamples)
        {
            /* Turn over looped sample */
            m_curSamplePos = m_curSample->m_loopStartSample;
            if (m_curFormat == SampleFormat::DSP)
            {
                m_prev1 = m_curSample->m_ADPCMParms.dsp.m_hist1;
                m_prev2 = m_curSample->m_ADPCMParms.dsp.m_hist2;
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
    if (m_state.m_inWait && m_state.m_keyoffWait)
    {
        if (m_volAdsr.isAdsrSet() || m_state.m_useAdsrControllers)
            m_volAdsr.keyOff(*this);
        if (m_pitchAdsr.isAdsrSet())
            m_pitchAdsr.keyOff();
    }
    else
    {
        m_volAdsr.keyOff(*this);
        m_pitchAdsr.keyOff();
    }
    m_state.keyoffNotify(*this);
}

void Voice::_setTotalPitch(int32_t cents, bool slew)
{
    // fprintf(stderr, "PITCH %d %d  \n", cents, slew);
    int32_t interval = clamp(0, cents, 12700) - m_curSample->m_pitch * 100;
    double ratio = std::exp2(interval / 1200.0) * m_dopplerRatio;
    m_sampleRate = m_curSample->m_sampleRate * ratio;
    m_backendVoice->setPitchRatio(ratio, slew);
}

bool Voice::_isRecursivelyDead()
{
    if (m_voxState != VoiceState::Dead)
        return false;
    for (auto& vox : m_childVoices)
        if (!vox->_isRecursivelyDead())
            return false;
    return true;
}

void Voice::_bringOutYourDead()
{
    for (auto it = m_childVoices.begin(); it != m_childVoices.end();)
    {
        Voice* vox = it->get();
        vox->_bringOutYourDead();
        if (vox->_isRecursivelyDead())
        {
            it = _destroyVoice(it);
            continue;
        }
        ++it;
    }
}

ObjToken<Voice> Voice::_findVoice(int vid, ObjToken<Voice> thisPtr)
{
    if (m_vid == vid)
        return thisPtr;

    for (ObjToken<Voice>& vox : m_childVoices)
    {
        ObjToken<Voice> ret = vox->_findVoice(vid, vox);
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

std::list<ObjToken<Voice>>::iterator Voice::_allocateVoice(double sampleRate, bool dynamicPitch)
{
    auto it = m_childVoices.emplace(
        m_childVoices.end(), MakeObj<Voice>(m_engine, m_audioGroup, m_groupId, m_engine.m_nextVid++, m_emitter, m_studio));
    m_childVoices.back()->m_backendVoice =
        m_engine.getBackend().allocateVoice(*m_childVoices.back(), sampleRate, dynamicPitch);
    return it;
}

std::list<ObjToken<Voice>>::iterator Voice::_destroyVoice(std::list<ObjToken<Voice>>::iterator it)
{
    if ((*it)->m_destroyed)
        return m_childVoices.begin();

    (*it)->_destroy();
    return m_childVoices.erase(it);
}

template <typename T>
static T ApplyVolume(float vol, T samp)
{
    return samp * vol;
}

void Voice::_procSamplePre(int16_t& samp)
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
        dt = m_sampleRate * 160;
        if (rem != 0)
        {
            /* Lerp within 160-sample block */
            float t = rem / 160.f;
            float l = clamp(0.f, m_lastLevel * (1.f - t) + m_nextLevel * t, 1.f);

            /* Apply total volume to sample using decibel scale */
            samp = ApplyVolume(l * m_engine.m_masterVolume, samp);
            return;
        }

        dt = 160.0 / m_sampleRate;
        break;
    }
    }

    m_voiceTime += dt;

    /* Process active envelope */
    if (m_envelopeTime >= 0.0)
    {
        m_envelopeTime += dt;
        float start = m_envelopeStart;
        float end = m_envelopeEnd;
        float t = clamp(0.f, float(m_envelopeTime / m_envelopeDur), 1.f);
        if (m_envelopeCurve && m_envelopeCurve->data.size() >= 128)
            t = m_envelopeCurve->data[t * 127.f] / 127.f;
        m_curVol = clamp(0.f, (start * (1.0f - t)) + (end * t), 1.f);

        // printf("%d %f\n", m_vid, m_curVol);

        /* Done with envelope */
        if (m_envelopeTime > m_envelopeDur)
            m_envelopeTime = -1.f;
    }

    /* Dynamically evaluate per-sample SoundMacro parameters */

    /* Process user volume slew */
    if (m_engine.m_ampMode == AmplitudeMode::PerSample)
    {
        if (m_targetUserVol != m_curUserVol)
        {
            float samplesPer5Ms = m_sampleRate * 5.f / 1000.f;
            if (samplesPer5Ms > 1.f)
            {
                float adjRate = 1.f / samplesPer5Ms;
                if (m_targetUserVol < m_curUserVol)
                {
                    m_curUserVol -= adjRate;
                    if (m_targetUserVol > m_curUserVol)
                        m_curUserVol = m_targetUserVol;
                }
                else
                {
                    m_curUserVol += adjRate;
                    if (m_targetUserVol < m_curUserVol)
                        m_curUserVol = m_targetUserVol;
                }
            }
            else
                m_curUserVol = m_targetUserVol;
        }
    }
    else
        m_curUserVol = m_targetUserVol;

    /* Factor in ADSR envelope state */
    float adsr = m_volAdsr.advance(dt, *this);
    m_lastLevel = m_nextLevel;
    m_nextLevel = m_curUserVol * m_curVol * adsr * (m_state.m_curVel / 127.f);

    /* Apply tremolo */
    if (m_state.m_tremoloSel && (m_tremoloScale || m_tremoloModScale))
    {
        float t = m_state.m_tremoloSel.evaluate(m_voiceTime, *this, m_state) / 2.f;
        if (m_tremoloScale && m_tremoloModScale)
        {
            float fac = (1.0f - t) + (m_tremoloScale * t);
            float modT = m_state.m_modWheelSel ? (m_state.m_modWheelSel.evaluate(m_voiceTime, *this, m_state) / 2.f)
                                               : (getCtrlValue(1) / 127.f);
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
            float modT = m_state.m_modWheelSel ? (m_state.m_modWheelSel.evaluate(m_voiceTime, *this, m_state) / 2.f)
                                               : (getCtrlValue(1) / 127.f);
            float modFac = (1.0f - modT) + (m_tremoloModScale * modT);
            m_nextLevel *= modFac;
        }
    }

    m_nextLevel = clamp(0.f, m_nextLevel, 1.f);

    /* Apply total volume to sample using decibel scale */
    samp = ApplyVolume(m_nextLevel * m_engine.m_masterVolume, samp);
}

template <typename T>
T Voice::_procSampleMaster(double time, T samp)
{
    float evalVol = m_state.m_volumeSel ? (m_state.m_volumeSel.evaluate(time, *this, m_state) / 2.f) : 1.f;
    return ApplyVolume(clamp(0.f, evalVol, 1.f), samp);
}

template <typename T>
T Voice::_procSampleAuxA(double time, T samp)
{
    float evalVol = m_state.m_volumeSel ? (m_state.m_volumeSel.evaluate(time, *this, m_state) / 2.f) : 1.f;
    evalVol *= m_state.m_reverbSel ? (m_state.m_reverbSel.evaluate(time, *this, m_state) / 2.f) : m_curReverbVol;
    evalVol += m_state.m_preAuxASel ? (m_state.m_preAuxASel.evaluate(time, *this, m_state) / 2.f) : 0.f;
    return ApplyVolume(clamp(0.f, evalVol, 1.f), samp);
}

template <typename T>
T Voice::_procSampleAuxB(double time, T samp)
{
    float evalVol = m_state.m_volumeSel ? (m_state.m_volumeSel.evaluate(time, *this, m_state) / 2.f) : 1.f;
    evalVol *= m_state.m_postAuxB ? (m_state.m_postAuxB.evaluate(time, *this, m_state) / 2.f) : m_curAuxBVol;
    evalVol += m_state.m_preAuxBSel ? (m_state.m_preAuxBSel.evaluate(time, *this, m_state) / 2.f) : 0.f;
    return ApplyVolume(clamp(0.f, evalVol, 1.f), samp);
}

uint32_t Voice::_GetBlockSampleCount(SampleFormat fmt)
{
    switch (fmt)
    {
    default:
        return 1;
    case SampleFormat::DSP:
    case SampleFormat::DSP_DRUM:
        return 14;
    case SampleFormat::N64:
        return 64;
    }
}

static float TriangleWave(float t)
{
    t = std::fmod(t, 1.f);
    if (t < 0.25f)
        return t / 0.25f;
    if (t >= 0.75f)
        return (t - 0.75f) / 0.25f - 1.f;
    return (t - 0.25f) / 0.5f * -2.f + 1.f;
}

void Voice::preSupplyAudio(double dt)
{
    /* Process SoundMacro; bootstrapping sample if needed */
    bool dead = m_state.advance(*this, dt);

    /* Process per-block evaluators here */
    if (m_state.m_pedalSel)
    {
        bool pedal = m_state.m_pedalSel.evaluate(m_voiceTime, *this, m_state) >= 1.f;
        if (pedal != m_sustained)
            setPedal(pedal);
    }

    bool panDirty = false;
    if (m_state.m_panSel)
    {
        float evalPan = m_state.m_panSel.evaluate(m_voiceTime, *this, m_state);
        if (evalPan != m_curPan)
        {
            m_curPan = evalPan;
            panDirty = true;
        }
    }
    if (m_state.m_spanSel)
    {
        float evalSpan = m_state.m_spanSel.evaluate(m_voiceTime, *this, m_state);
        if (evalSpan != m_curSpan)
        {
            m_curSpan = evalSpan;
            panDirty = true;
        }
    }
    if (panDirty)
        _setPan(m_curPan);

    if (m_state.m_pitchWheelSel)
        _setPitchWheel(m_state.m_pitchWheelSel.evaluate(m_voiceTime, *this, m_state));

    /* Process active pan-sweep */
    bool refresh = false;
    if (m_panningTime >= 0.f)
    {
        m_panningTime += dt;
        float start = (m_panPos - 64) / 64.f;
        float end = (m_panPos + m_panWidth - 64) / 64.f;
        float t = clamp(0.f, m_panningTime / m_panningDur, 1.f);
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
        float t = clamp(0.f, m_spanningTime / m_spanningDur, 1.f);
        _setSurroundPan((start * (1.0f - t)) + (end * t));
        refresh = true;

        /* Done with spanning */
        if (m_spanningTime > m_spanningDur)
            m_spanningTime = -1.f;
    }

    /* Calculate total pitch */
    int32_t newPitch = m_curPitch;
    refresh |= m_pitchDirty;
    m_pitchDirty = false;
    if (m_portamentoTime >= 0.f)
    {
        m_portamentoTime += dt;
        float t = clamp(0.f, m_portamentoTime / m_state.m_portamentoTime, 1.f);

        newPitch = (m_curPitch * (1.0f - t)) + (m_portamentoTarget * t);
        refresh = true;

        /* Done with portamento */
        if (m_portamentoTime > m_state.m_portamentoTime)
        {
            m_portamentoTime = -1.f;
            m_curPitch = m_portamentoTarget;
        }
    }
    if (m_pitchEnv)
    {
        newPitch *= m_pitchAdsr.advance(dt);
        refresh = true;
    }

    /* Process vibrato */
    if (m_vibratoTime >= 0.f)
    {
        m_vibratoTime += dt;
        float vibrato = TriangleWave(m_vibratoTime / m_vibratoPeriod);
        if (m_vibratoModWheel)
            newPitch += m_vibratoModLevel * vibrato * (m_state.m_curMod / 127.f);
        else
            newPitch += m_vibratoLevel * vibrato;
        refresh = true;
    }

    /* Process pitch sweep 1 */
    if (m_pitchSweep1It < m_pitchSweep1Times)
    {
        ++m_pitchSweep1It;
        m_pitchSweep1 = m_pitchSweep1Add * m_pitchSweep1It;
        refresh = true;
    }
    else if (m_pitchSweep1Times != 0)
    {
        m_pitchSweep1It = 0;
        m_pitchSweep1 = 0;
        refresh = true;
    }

    /* Process pitch sweep 2 */
    if (m_pitchSweep2It < m_pitchSweep2Times)
    {
        ++m_pitchSweep2It;
        m_pitchSweep2 = m_pitchSweep2Add * m_pitchSweep2It;
        refresh = true;
    }
    else if (m_pitchSweep2Times != 0)
    {
        m_pitchSweep2It = 0;
        m_pitchSweep2 = 0;
        refresh = true;
    }

    if (m_curSample && refresh)
    {
        _setTotalPitch(newPitch + m_pitchSweep1 + m_pitchSweep2 + m_pitchWheelVal, m_needsSlew);
        m_needsSlew = true;
    }

    if (dead && (!m_curSample || m_voxState == VoiceState::KeyOff) && m_sampleEndTrap.macroId == 0xffff &&
        m_messageTrap.macroId == 0xffff && (!m_curSample || (m_curSample && m_volAdsr.isComplete())))
    {
        m_voxState = VoiceState::Dead;
        m_backendVoice->stop();
    }
}

size_t Voice::supplyAudio(size_t samples, int16_t* data)
{
    uint32_t samplesRem = samples;

    if (m_curSample)
    {
        uint32_t blockSampleCount = _GetBlockSampleCount(m_curFormat);
        uint32_t block;

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
                    decSamples =
                        DSPDecompressFrameRanged(data, m_curSampleData + 8 * block,
                                                 m_curSample->m_ADPCMParms.dsp.m_coefs,
                                                 &m_prev1, &m_prev2, rem, remCount);
                    break;
                }
                case SampleFormat::N64:
                {
                    decSamples = N64MusyXDecompressFrameRanged(data, m_curSampleData + 256 + 40 * block,
                                                               m_curSample->m_ADPCMParms.vadpcm.m_coefs, rem, remCount);
                    break;
                }
                case SampleFormat::PCM:
                {
                    const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                    remCount = std::min(samplesRem, m_lastSamplePos - m_curSamplePos);
                    for (uint32_t i = 0; i < remCount; ++i)
                        data[i] = SBig(pcm[m_curSamplePos + i]);
                    decSamples = remCount;
                    break;
                }
                case SampleFormat::PCM_PC:
                {
                    const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                    remCount = std::min(samplesRem, m_lastSamplePos - m_curSamplePos);
                    memmove(data, pcm + m_curSamplePos, remCount * sizeof(int16_t));
                    decSamples = remCount;
                    break;
                }
                default:
                    memset(data, 0, sizeof(int16_t) * samples);
                    return samples;
                }

                /* Per-sample processing */
                for (uint32_t i = 0; i < decSamples; ++i)
                {
                    ++m_curSamplePos;
                    _procSamplePre(data[i]);
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
                        m_curSample->m_ADPCMParms.dsp.m_coefs, &m_prev1, &m_prev2, remCount);
                    break;
                }
                case SampleFormat::N64:
                {
                    decSamples = N64MusyXDecompressFrame(data, m_curSampleData + 256 + 40 * block,
                                                         m_curSample->m_ADPCMParms.vadpcm.m_coefs, remCount);
                    break;
                }
                case SampleFormat::PCM:
                {
                    const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                    remCount = std::min(samplesRem, m_lastSamplePos - m_curSamplePos);
                    for (uint32_t i = 0; i < remCount; ++i)
                        data[i] = SBig(pcm[m_curSamplePos + i]);
                    decSamples = remCount;
                    break;
                }
                case SampleFormat::PCM_PC:
                {
                    const int16_t* pcm = reinterpret_cast<const int16_t*>(m_curSampleData);
                    remCount = std::min(samplesRem, m_lastSamplePos - m_curSamplePos);
                    memmove(data, pcm + m_curSamplePos, remCount * sizeof(int16_t));
                    decSamples = remCount;
                    break;
                }
                default:
                    memset(data, 0, sizeof(int16_t) * samples);
                    return samples;
                }

                /* Per-sample processing */
                for (uint32_t i = 0; i < decSamples; ++i)
                {
                    ++m_curSamplePos;
                    _procSamplePre(data[i]);
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
    }
    else
        memset(data, 0, sizeof(int16_t) * samples);

    if (m_voxState == VoiceState::Dead)
        m_curSample.reset();

    return samples;
}

void Voice::routeAudio(size_t frames, double dt, int busId, int16_t* in, int16_t* out)
{
    dt /= double(frames);

    switch (busId)
    {
    case 0:
    default:
    {
        for (uint32_t i = 0; i < frames; ++i)
            out[i] = _procSampleMaster(dt * i + m_voiceTime, in[i]);
        break;
    }
    case 1:
    {
        for (uint32_t i = 0; i < frames; ++i)
            out[i] = _procSampleAuxA(dt * i + m_voiceTime, in[i]);
        break;
    }
    case 2:
    {
        for (uint32_t i = 0; i < frames; ++i)
            out[i] = _procSampleAuxB(dt * i + m_voiceTime, in[i]);
        break;
    }
    }
}

void Voice::routeAudio(size_t frames, double dt, int busId, int32_t* in, int32_t* out)
{
    dt /= double(frames);

    switch (busId)
    {
    case 0:
    default:
    {
        for (uint32_t i = 0; i < frames; ++i)
            out[i] = _procSampleMaster(dt * i + m_voiceTime, in[i]);
        break;
    }
    case 1:
    {
        for (uint32_t i = 0; i < frames; ++i)
            out[i] = _procSampleAuxA(dt * i + m_voiceTime, in[i]);
        break;
    }
    case 2:
    {
        for (uint32_t i = 0; i < frames; ++i)
            out[i] = _procSampleAuxB(dt * i + m_voiceTime, in[i]);
        break;
    }
    }
}

void Voice::routeAudio(size_t frames, double dt, int busId, float* in, float* out)
{
    dt /= double(frames);

    switch (busId)
    {
    case 0:
    default:
    {
        for (uint32_t i = 0; i < frames; ++i)
            out[i] = _procSampleMaster(dt * i + m_voiceTime, in[i]);
        break;
    }
    case 1:
    {
        for (uint32_t i = 0; i < frames; ++i)
            out[i] = _procSampleAuxA(dt * i + m_voiceTime, in[i]);
        break;
    }
    case 2:
    {
        for (uint32_t i = 0; i < frames; ++i)
            out[i] = _procSampleAuxB(dt * i + m_voiceTime, in[i]);
        break;
    }
    }
}

int Voice::maxVid() const
{
    int maxVid = m_vid;
    for (const ObjToken<Voice>& vox : m_childVoices)
        maxVid = std::max(maxVid, vox->maxVid());
    return maxVid;
}

ObjToken<Voice> Voice::_startChildMacro(ObjectId macroId, int macroStep, double ticksPerSec, uint8_t midiKey,
                                        uint8_t midiVel, uint8_t midiMod, bool pushPc)
{
    std::list<ObjToken<Voice>>::iterator vox = _allocateVoice(NativeSampleRate, true);
    if (!(*vox)->loadMacroObject(macroId, macroStep, ticksPerSec, midiKey, midiVel, midiMod, pushPc))
    {
        _destroyVoice(vox);
        return {};
    }
    (*vox)->setVolume(m_targetUserVol);
    (*vox)->setPan(m_curPan);
    (*vox)->setSurroundPan(m_curSpan);
    return *vox;
}

ObjToken<Voice> Voice::startChildMacro(int8_t addNote, ObjectId macroId, int macroStep)
{
    return _startChildMacro(macroId, macroStep, 1000.0, m_state.m_initKey + addNote, m_state.m_initVel,
                            m_state.m_initMod);
}

bool Voice::_loadSoundMacro(SoundMacroId id, const SoundMacro* macroData, int macroStep, double ticksPerSec,
                            uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc)
{
    m_objectId = id;

    if (m_state.m_pc.empty())
        m_state.initialize(id, macroData, macroStep, ticksPerSec, midiKey, midiVel, midiMod);
    else
    {
        if (!pushPc)
            m_state.m_pc.clear();
        m_state.m_pc.emplace_back(id, macroData, macroData->assertPC(macroStep));
    }

    m_voxState = VoiceState::Playing;
    m_backendVoice->start();
    return true;
}

bool Voice::_loadKeymap(const Keymap* keymap, double ticksPerSec,
                        uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc)
{
    const Keymap& km = keymap[midiKey];
    midiKey += km.transpose;
    bool ret = loadMacroObject(km.macro.id, 0, ticksPerSec, midiKey, midiVel, midiMod, pushPc);
    m_curVol = 1.f;
    _setPan((km.pan - 64) / 64.f);
    _setSurroundPan(-1.f);
    return ret;
}

bool Voice::_loadLayer(const std::vector<LayerMapping>& layer, double ticksPerSec,
                       uint8_t midiKey, uint8_t midiVel, uint8_t midiMod, bool pushPc)
{
    bool ret = false;
    for (const LayerMapping& mapping : layer)
    {
        if (midiKey >= mapping.keyLo && midiKey <= mapping.keyHi)
        {
            uint8_t mappingKey = midiKey + mapping.transpose;
            if (m_voxState != VoiceState::Playing)
            {
                ret |= loadMacroObject(mapping.macro.id, 0, ticksPerSec, mappingKey, midiVel, midiMod, pushPc);
                m_curVol = mapping.volume / 127.f;
                _setPan((mapping.pan - 64) / 64.f);
                _setSurroundPan((mapping.span - 64) / 64.f);
            }
            else
            {
                ObjToken<Voice> vox =
                    _startChildMacro(mapping.macro.id, 0, ticksPerSec, mappingKey, midiVel, midiMod, pushPc);
                if (vox)
                {
                    vox->m_curVol = mapping.volume / 127.f;
                    vox->_setPan((mapping.pan - 64) / 64.f);
                    vox->_setSurroundPan((mapping.span - 64) / 64.f);
                    ret = true;
                }
            }
        }
    }
    return ret;
}

bool Voice::loadMacroObject(SoundMacroId macroId, int macroStep, double ticksPerSec, uint8_t midiKey, uint8_t midiVel,
                            uint8_t midiMod, bool pushPc)
{
    if (m_destroyed)
        return false;

    const SoundMacro* macroData = m_audioGroup.getPool().soundMacro(macroId);
    if (macroData)
        return _loadSoundMacro(macroId, macroData, macroStep, ticksPerSec, midiKey, midiVel, midiMod, pushPc);

    return false;
}

bool Voice::loadPageObject(ObjectId objectId, double ticksPerSec, uint8_t midiKey, uint8_t midiVel, uint8_t midiMod)
{
    if (m_destroyed)
        return false;

    if (objectId.id & 0x8000)
    {
        const std::vector<LayerMapping>* layer = m_audioGroup.getPool().layer(objectId);
        if (layer)
            return _loadLayer(*layer, ticksPerSec, midiKey, midiVel, midiMod);
    }
    else
    {
        const Keymap* keymap = m_audioGroup.getPool().keymap(objectId);
        if (keymap)
            return _loadKeymap(keymap, ticksPerSec, midiKey, midiVel, midiMod);
    }

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
    if (m_destroyed)
        return;

    if (m_keyoffTrap.macroId != 0xffff)
    {
        if (m_keyoffTrap.macroId == std::get<0>(m_state.m_pc.back()))
        {
            std::get<2>(m_state.m_pc.back()) = std::get<1>(m_state.m_pc.back())->assertPC(m_keyoffTrap.macroStep);
            m_state.m_inWait = false;
        }
        else
            loadMacroObject(m_keyoffTrap.macroId, m_keyoffTrap.macroStep, m_state.m_ticksPerSec, m_state.m_initKey,
                            m_state.m_initVel, m_state.m_initMod);
    }
    else if (!m_curSample || m_curSample->m_loopLengthSamples)
        _macroKeyOff();

    for (const ObjToken<Voice>& vox : m_childVoices)
        vox->keyOff();
}

void Voice::message(int32_t val)
{
    if (m_destroyed)
        return;

    m_latestMessage = val;

    if (m_messageTrap.macroId != 0xffff)
    {
        if (m_messageTrap.macroId == std::get<0>(m_state.m_pc.back()))
            std::get<2>(m_state.m_pc.back()) = std::get<1>(m_state.m_pc.back())->assertPC(m_messageTrap.macroStep);
        else
            loadMacroObject(m_messageTrap.macroId, m_messageTrap.macroStep, m_state.m_ticksPerSec, m_state.m_initKey,
                            m_state.m_initVel, m_state.m_initMod);
    }
}

void Voice::startSample(SampleId sampId, int32_t offset)
{
    if (m_destroyed)
        return;

    if (const SampleEntry* sample = m_audioGroup.getSample(sampId))
    {
        std::tie(m_curSample, m_curSampleData) = m_audioGroup.getSampleData(sampId, sample);

        m_sampleRate = m_curSample->m_sampleRate;
        m_curPitch = m_curSample->m_pitch;
        m_pitchDirty = true;
        _setPitchWheel(m_curPitchWheel);
        m_backendVoice->resetSampleRate(m_curSample->m_sampleRate);
        m_needsSlew = false;

        int32_t numSamples = m_curSample->m_numSamples & 0xffffff;
        if (offset)
        {
            if (m_curSample->m_loopLengthSamples)
            {
                if (offset > int32_t(m_curSample->m_loopStartSample))
                    offset =
                        ((offset - m_curSample->m_loopStartSample) % m_curSample->m_loopLengthSamples) +
                        m_curSample->m_loopStartSample;
            }
            else
                offset = clamp(0, offset, numSamples);
        }
        m_curSamplePos = offset;
        m_prev1 = 0;
        m_prev2 = 0;

        m_curFormat = SampleFormat(m_curSample->m_numSamples >> 24);
        if (m_curFormat == SampleFormat::DSP_DRUM)
            m_curFormat = SampleFormat::DSP;

        m_lastSamplePos = m_curSample->m_loopLengthSamples
                              ? (m_curSample->m_loopStartSample + m_curSample->m_loopLengthSamples)
                              : numSamples;

        bool looped;
        _checkSamplePos(looped);

        /* Seek DSPADPCM state if needed */
        if (m_curSample && m_curSamplePos && m_curFormat == SampleFormat::DSP)
        {
            uint32_t block = m_curSamplePos / 14;
            uint32_t rem = m_curSamplePos % 14;
            for (uint32_t b = 0; b < block; ++b)
                DSPDecompressFrameStateOnly(m_curSampleData + 8 * b, m_curSample->m_ADPCMParms.dsp.m_coefs, &m_prev1,
                                            &m_prev2, 14);

            if (rem)
                DSPDecompressFrameStateOnly(m_curSampleData + 8 * block, m_curSample->m_ADPCMParms.dsp.m_coefs, &m_prev1,
                                            &m_prev2, rem);
        }
    }
}

void Voice::stopSample() { m_curSample.reset(); }

void Voice::setVolume(float vol)
{
    if (m_destroyed)
        return;

    m_targetUserVol = clamp(0.f, vol, 1.f);
    for (ObjToken<Voice>& vox : m_childVoices)
        vox->setVolume(vol);
}

void Voice::_panLaw(float coefs[8], float frontPan, float backPan, float totalSpan) const
{
    /* -3dB panning law for various channel configs */
    switch (m_engine.m_channelSet)
    {
    case AudioChannelSet::Stereo:
    default:
        /* Left */
        coefs[0] = std::sqrt(-frontPan * 0.5f + 0.5f);

        /* Right */
        coefs[1] = std::sqrt(frontPan * 0.5f + 0.5f);

        break;

    case AudioChannelSet::Quad:
        /* Left */
        coefs[0] = -frontPan * 0.5f + 0.5f;
        coefs[0] *= -totalSpan * 0.5f + 0.5f;
        coefs[0] = std::sqrt(coefs[0]);

        /* Right */
        coefs[1] = frontPan * 0.5f + 0.5f;
        coefs[1] *= -totalSpan * 0.5f + 0.5f;
        coefs[1] = std::sqrt(coefs[1]);

        /* Back Left */
        coefs[2] = -backPan * 0.5f + 0.5f;
        coefs[2] *= totalSpan * 0.5f + 0.5f;
        coefs[2] = std::sqrt(coefs[2]);

        /* Back Right */
        coefs[3] = backPan * 0.5f + 0.5f;
        coefs[3] *= totalSpan * 0.5f + 0.5f;
        coefs[3] = std::sqrt(coefs[3]);

        break;

    case AudioChannelSet::Surround51:
        /* Left */
        coefs[0] = (frontPan <= 0.f) ? -frontPan : 0.f;
        coefs[0] *= -totalSpan * 0.5f + 0.5f;
        coefs[0] = std::sqrt(coefs[0]);

        /* Right */
        coefs[1] = (frontPan >= 0.f) ? frontPan : 0.f;
        coefs[1] *= -totalSpan * 0.5f + 0.5f;
        coefs[1] = std::sqrt(coefs[1]);

        /* Back Left */
        coefs[2] = -backPan * 0.5f + 0.5f;
        coefs[2] *= totalSpan * 0.5f + 0.5f;
        coefs[2] = std::sqrt(coefs[2]);

        /* Back Right */
        coefs[3] = backPan * 0.5f + 0.5f;
        coefs[3] *= totalSpan * 0.5f + 0.5f;
        coefs[3] = std::sqrt(coefs[3]);

        /* Center */
        coefs[4] = 1.f - std::fabs(frontPan);
        coefs[4] *= -totalSpan * 0.5f + 0.5f;
        coefs[4] = std::sqrt(coefs[4]);

        /* LFE */
        coefs[5] = 0.25f;

        break;

    case AudioChannelSet::Surround71:
        /* Left */
        coefs[0] = (frontPan <= 0.f) ? -frontPan : 0.f;
        coefs[0] *= (totalSpan <= 0.f) ? -totalSpan : 0.f;
        coefs[0] = std::sqrt(coefs[0]);

        /* Right */
        coefs[1] = (frontPan >= 0.f) ? frontPan : 0.f;
        coefs[1] *= (totalSpan <= 0.f) ? -totalSpan : 0.f;
        coefs[1] = std::sqrt(coefs[1]);

        /* Back Left */
        coefs[2] = -backPan * 0.5f + 0.5f;
        coefs[2] *= (totalSpan >= 0.f) ? totalSpan : 0.f;
        coefs[2] = std::sqrt(coefs[2]);

        /* Back Right */
        coefs[3] = backPan * 0.5f + 0.5f;
        coefs[3] *= (totalSpan >= 0.f) ? totalSpan : 0.f;
        coefs[3] = std::sqrt(coefs[3]);

        /* Center */
        coefs[4] = 1.f - std::fabs(frontPan);
        coefs[4] *= (totalSpan <= 0.f) ? -totalSpan : 0.f;
        coefs[4] = std::sqrt(coefs[4]);

        /* LFE */
        coefs[5] = 0.25f;

        /* Side Left */
        coefs[6] = -backPan * 0.5f + 0.5f;
        coefs[6] *= 1.f - std::fabs(totalSpan);
        coefs[6] = std::sqrt(coefs[6]);

        /* Side Right */
        coefs[7] = backPan * 0.5f + 0.5f;
        coefs[7] *= 1.f - std::fabs(totalSpan);
        coefs[7] = std::sqrt(coefs[7]);

        break;
    }
}

void Voice::_setPan(float pan)
{
    if (m_destroyed || m_emitter)
        return;

    m_curPan = clamp(-1.f, pan, 1.f);
    float totalPan = clamp(-1.f, m_curPan, 1.f);
    float totalSpan = clamp(-1.f, m_curSpan, 1.f);
    float coefs[8] = {};
    _panLaw(coefs, totalPan, totalPan, totalSpan);
    _setChannelCoefs(coefs);
}

void Voice::setPan(float pan)
{
    if (m_destroyed)
        return;

    _setPan(pan);
    for (ObjToken<Voice>& vox : m_childVoices)
        vox->setPan(pan);
}

void Voice::_setSurroundPan(float span)
{
    m_curSpan = clamp(-1.f, span, 1.f);
    _setPan(m_curPan);
}

void Voice::setSurroundPan(float span)
{
    if (m_destroyed)
        return;

    _setSurroundPan(span);
    for (ObjToken<Voice>& vox : m_childVoices)
        vox->setSurroundPan(span);
}

void Voice::_setChannelCoefs(const float coefs[8])
{
    m_backendVoice->setChannelLevels(m_studio->getMaster().m_backendSubmix.get(), coefs, true);
    m_backendVoice->setChannelLevels(m_studio->getAuxA().m_backendSubmix.get(), coefs, true);
    m_backendVoice->setChannelLevels(m_studio->getAuxB().m_backendSubmix.get(), coefs, true);
}

void Voice::setChannelCoefs(const float coefs[8])
{
    if (m_destroyed)
        return;

    _setChannelCoefs(coefs);
    for (ObjToken<Voice>& vox : m_childVoices)
        vox->setChannelCoefs(coefs);
}

void Voice::startEnvelope(double dur, float vol, const Curve* envCurve)
{
    m_envelopeTime = 0.f;
    m_envelopeDur = dur;
    m_envelopeStart = clamp(0.f, m_curVol, 1.f);
    m_envelopeEnd = clamp(0.f, vol, 1.f);
    m_envelopeCurve = envCurve;
}

void Voice::startFadeIn(double dur, float vol, const Curve* envCurve)
{
    m_envelopeTime = 0.f;
    m_envelopeDur = dur;
    m_envelopeStart = 0.f;
    m_envelopeEnd = clamp(0.f, vol, 1.f);
    m_envelopeCurve = envCurve;
}

void Voice::startPanning(double dur, uint8_t panPos, int8_t panWidth)
{
    m_panningTime = 0.f;
    m_panningDur = dur;
    m_panPos = panPos;
    m_panWidth = panWidth;
}

void Voice::startSpanning(double dur, uint8_t spanPos, int8_t spanWidth)
{
    m_spanningTime = 0.f;
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
    if (m_destroyed)
        return;

    if (m_sustained && !pedal && m_sustainKeyOff)
    {
        m_sustainKeyOff = false;
        _doKeyOff();
    }
    m_sustained = pedal;

    for (ObjToken<Voice>& vox : m_childVoices)
        vox->setPedal(pedal);
}

void Voice::setDoppler(float) {}

void Voice::setVibrato(int32_t level, bool modScale, float period)
{
    m_vibratoTime = std::fabs(period) < FLT_EPSILON ? -1.f : 0.f;
    m_vibratoLevel = level;
    m_vibratoModWheel = modScale;
    m_vibratoPeriod = period;
}

void Voice::setMod2VibratoRange(int32_t modLevel) { m_vibratoModLevel = modLevel; }

void Voice::setTremolo(float tremoloScale, float tremoloModScale)
{
    m_tremoloScale = tremoloScale;
    m_tremoloModScale = tremoloModScale;
}

void Voice::setPitchSweep1(uint8_t times, int16_t add)
{
    m_pitchSweep1 = 0;
    m_pitchSweep1It = 0;
    m_pitchSweep1Times = times;
    m_pitchSweep1Add = add;
}

void Voice::setPitchSweep2(uint8_t times, int16_t add)
{
    m_pitchSweep2 = 0;
    m_pitchSweep2It = 0;
    m_pitchSweep2Times = times;
    m_pitchSweep2Add = add;
}

void Voice::setReverbVol(float rvol)
{
    if (m_destroyed)
        return;

    m_curReverbVol = clamp(0.f, rvol, 1.f);
    for (ObjToken<Voice>& vox : m_childVoices)
        vox->setReverbVol(rvol);
}

void Voice::setAuxBVol(float bvol)
{
    if (m_destroyed)
        return;

    m_curAuxBVol = clamp(0.f, bvol, 1.f);
    for (ObjToken<Voice>& vox : m_childVoices)
        vox->setAuxBVol(bvol);
}

void Voice::setAdsr(ObjectId adsrId, bool dls)
{
    if (m_destroyed)
        return;

    if (dls)
    {
        const ADSRDLS* adsr = m_audioGroup.getPool().tableAsAdsrDLS(adsrId);
        if (adsr)
        {
            m_volAdsr.reset(adsr, m_state.m_initKey, m_state.m_initVel);
            if (m_voxState == VoiceState::KeyOff)
                m_volAdsr.keyOff(*this);
        }
    }
    else
    {
        const ADSR* adsr = m_audioGroup.getPool().tableAsAdsr(adsrId);
        if (adsr)
        {
            m_volAdsr.reset(adsr);
            if (m_voxState == VoiceState::KeyOff)
                m_volAdsr.keyOff(*this);
        }
    }
}

void Voice::setPitchFrequency(uint32_t hz, uint16_t fine)
{
    if (m_destroyed)
        return;

    m_sampleRate = hz + fine / 65536.0;
    m_backendVoice->setPitchRatio(1.0, false);
    m_backendVoice->resetSampleRate(m_sampleRate);
}

void Voice::setPitchAdsr(ObjectId adsrId, int32_t cents)
{
    if (m_destroyed)
        return;

    const ADSRDLS* adsr = m_audioGroup.getPool().tableAsAdsrDLS(adsrId);
    if (adsr)
    {
        m_pitchAdsr.reset(adsr, m_state.m_initKey, m_state.m_initVel);
        m_pitchEnvRange = cents;
        m_pitchEnv = true;
    }
}

void Voice::_setPitchWheel(float pitchWheel)
{
    if (m_destroyed)
        return;

    if (pitchWheel > 0.f)
        m_pitchWheelVal = m_pitchWheelUp * m_curPitchWheel;
    else if (pitchWheel < 0.f)
        m_pitchWheelVal = m_pitchWheelDown * m_curPitchWheel;
    else
        m_pitchWheelVal = 0;
    m_pitchDirty = true;
}

void Voice::setPitchWheel(float pitchWheel)
{
    if (m_destroyed)
        return;

    m_curPitchWheel = amuse::clamp(-1.f, pitchWheel, 1.f);
    _setPitchWheel(m_curPitchWheel);

    for (ObjToken<Voice>& vox : m_childVoices)
        vox->setPitchWheel(pitchWheel);
}

void Voice::setPitchWheelRange(int8_t up, int8_t down)
{
    if (m_destroyed)
        return;

    m_pitchWheelUp = up * 100;
    m_pitchWheelDown = down * 100;
    _setPitchWheel(m_curPitchWheel);
}

void Voice::setAftertouch(uint8_t aftertouch)
{
    if (m_destroyed)
        return;

    m_curAftertouch = aftertouch;
    for (ObjToken<Voice>& vox : m_childVoices)
        vox->setAftertouch(aftertouch);
}

bool Voice::doPortamento(uint8_t newNote)
{
    bool pState;
    switch (m_state.m_portamentoMode)
    {
    case SoundMacro::CmdPortamento::PortState::Disable:
    default:
        pState = false;
        break;
    case SoundMacro::CmdPortamento::PortState::Enable:
        pState = true;
        break;
    case SoundMacro::CmdPortamento::PortState::MIDIControlled:
        pState = m_state.m_portamentoSel ? (m_state.m_portamentoSel.evaluate(m_voiceTime, *this, m_state) >= 1.f)
                                         : (getCtrlValue(65) >= 64);
        break;
    }

    if (!pState)
        return false;

    m_portamentoTime = 0.f;
    m_portamentoTarget = newNote * 100;
    m_state.m_initKey = newNote;
    return true;
}

void Voice::_notifyCtrlChange(uint8_t ctrl, int8_t val)
{
    if (ctrl == 0x40)
    {
        if (val >= 0x40)
            setPedal(true);
        else
            setPedal(false);
    }
    else if (ctrl == 0x5b)
    {
        setReverbVol(val / 127.f);
    }
    else if (ctrl == 0x5d)
    {
        setAuxBVol(val / 127.f);
    }
    else if (ctrl == 0x1)
    {
        m_state.m_curMod = uint8_t(val);
    }

    for (ObjToken<Voice>& vox : m_childVoices)
        vox->_notifyCtrlChange(ctrl, val);
}

size_t Voice::getTotalVoices() const
{
    size_t ret = 1;
    for (const ObjToken<Voice>& vox : m_childVoices)
        ret += vox->getTotalVoices();
    return ret;
}

void Voice::kill()
{
    if (m_destroyed)
        return;

    m_voxState = VoiceState::Dead;
    m_backendVoice->stop();
    for (const ObjToken<Voice>& vox : m_childVoices)
        vox->kill();
}
}

#include "amuse/Voice.hpp"
#include "amuse/Submix.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/AudioGroup.hpp"
#include "amuse/dsp.h"
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
}

size_t Voice::supplyAudio(size_t samples, int16_t* data)
{
    uint32_t samplesRem = samples;
    if (m_curSample)
    {
        uint32_t block = m_curSamplePos / 14;
        uint32_t rem = m_curSamplePos % 14;

        if (rem)
        {
            uint32_t decSamples = DSPDecompressFrameRanged(data, m_curSampleData + 8 * block,
                                                           m_curSample->second.m_coefs,
                                                           &m_prev1, &m_prev2, rem,
                                                           std::min(samplesRem,
                                                                    m_lastSamplePos - block * 14));
            m_curSamplePos += decSamples;
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
            uint32_t decSamples = DSPDecompressFrame(data, m_curSampleData + 8 * block,
                                                     m_curSample->second.m_coefs,
                                                     &m_prev1, &m_prev2,
                                                     std::min(samplesRem,
                                                              m_lastSamplePos - block * 14));
            m_curSamplePos += decSamples;
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
        memset(data, 0, sizeof(int16_t) * samples);

    return samples;
}

Voice* Voice::startChildMacro(int8_t addNote, ObjectId macroId, int macroStep)
{
}

bool Voice::loadSoundMacro(ObjectId macroId, int macroStep, bool pushPc)
{
}

void Voice::keyOff()
{
    if (m_sustained)
        m_sustainKeyOff = true;
    else
        _doKeyOff();
}

void Voice::message(int32_t val)
{
}

void Voice::startSample(int16_t sampId, int32_t offset)
{
    m_curSample = m_audioGroup.getSample(sampId);
    if (m_curSample)
    {
        m_backendVoice->stop();
        m_backendVoice->resetSampleRate(m_curSample->first.m_sampleRate);
        m_backendVoice->start();
        m_curSamplePos = offset;
        m_curSampleData = m_audioGroup.getSampleData(m_curSample->first.m_sampleOff);
        m_prev1 = 0;
        m_prev2 = 0;
        m_lastSamplePos = m_curSample->first.m_loopLengthSamples ?
            (m_curSample->first.m_loopStartSample + m_curSample->first.m_loopLengthSamples) :
             m_curSample->first.m_numSamples;
    }
}

void Voice::stopSample()
{
    m_backendVoice->stop();
    m_curSample = nullptr;
}

void Voice::setVolume(float vol)
{
}

void Voice::setPanning(float pan)
{
}

void Voice::setSurroundPanning(float span)
{
}

void Voice::setPitchKey(int32_t cents)
{
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
}

void Voice::setDoppler(float doppler)
{
}

void Voice::setReverbVol(float rvol)
{
}

void Voice::setAdsr(ObjectId adsrId)
{
}

void Voice::setPitchFrequency(uint32_t hz, uint16_t fine)
{
}

void Voice::setPitchAdsr(ObjectId adsrId, int32_t cents)
{
}

void Voice::setPitchWheelRange(int8_t up, int8_t down)
{
}

}

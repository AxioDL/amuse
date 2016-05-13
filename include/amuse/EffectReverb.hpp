#ifndef __AMUSE_EFFECTREVERB_HPP__
#define __AMUSE_EFFECTREVERB_HPP__

#include "EffectBase.hpp"
#include "amuse/Common.hpp"
#include <memory>

namespace amuse
{

/** Delay state for one 'tap' of the reverb effect */
struct ReverbDelayLine
{
    int32_t x0_inPoint = 0;
    int32_t x4_outPoint = 0;
    int32_t x8_length = 0;
    std::unique_ptr<float[]> xc_inputs;
    float x10_lastInput = 0.f;

    void allocate(int32_t delay);
    void setdelay(int32_t delay);
};

/** Reverb effect with configurable reflection filtering */
template <typename T, size_t AP, size_t C>
class EffectReverb : public EffectBase<T>
{
protected:
    ReverbDelayLine x0_x0_AP[8][AP] = {}; /**< All-pass delay lines */
    ReverbDelayLine x78_xb4_C[8][C] = {}; /**< Comb delay lines */
    float xf0_x168_allPassCoef = 0.f; /**< All-pass mix coefficient */
    float xf4_x16c_combCoef[8][C] = {}; /**< Comb mix coefficients */
    float x10c_x190_lpLastout[8] = {}; /**< Last low-pass results */
    float x118_x19c_level = 0.f; /**< Internal wet/dry mix factor */
    float x11c_x1a0_damping = 0.f; /**< Low-pass damping */
    int32_t x120_x1a4_preDelayTime = 0; /**< Sample count of pre-delay */
    std::unique_ptr<float[]> x124_x1ac_preDelayLine[8]; /**< Dedicated pre-delay buffers */
    float* x130_x1b8_preDelayPtr[8] = {}; /**< Current pre-delay pointers */

    float x140_x1c8_coloration; /**< [0.0, 1.0] influences filter coefficients to define surface characteristics of a room */
    float x144_x1cc_mix; /**< [0.0, 1.0] dry/wet mix factor of reverb effect */
    float x148_x1d0_time; /**< [0.01, 10.0] time in seconds for reflection decay */
    float x14c_x1d4_damping; /**< [0.0, 1.0] damping factor influencing low-pass filter of reflections */
    float x150_x1d8_preDelay; /**< [0.0, 0.1] time in seconds before initial reflection heard */

    double m_sampleRate; /**< copy of sample rate */
    bool m_dirty = true; /**< needs update of internal parameter data */
    void _update();
public:
    EffectReverb(float coloration, float mix, float time,
                 float damping, float preDelay, double sampleRate);
    void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap);

    void setColoration(float coloration)
    {
        x140_x1c8_coloration = clamp(0.f, coloration, 1.f);
        m_dirty = true;
    }
    void setMix(float mix)
    {
        x144_x1cc_mix = clamp(0.f, mix, 1.f);
        m_dirty = true;
    }
    void setTime(float time)
    {
        x148_x1d0_time = clamp(0.01f, time, 10.f);
        m_dirty = true;
    }
    void setDamping(float damping)
    {
        x14c_x1d4_damping = clamp(0.f, damping, 1.f);
        m_dirty = true;
    }
    void setPreDelay(float preDelay)
    {
        x150_x1d8_preDelay = clamp(0.f, preDelay, 0.1f);
        m_dirty = true;
    }
};

/** Standard-quality 2-tap reverb */
template <typename T>
class EffectReverbStd : public EffectReverb<T, 2, 2>
{
public:
    EffectReverbStd(float coloration, float mix, float time,
                    float damping, float preDelay, double sampleRate);
};

/** High-quality 3-tap reverb */
template <typename T>
class EffectReverbHi : public EffectReverb<T, 2, 3>
{
    ReverbDelayLine x78_LP[8] = {}; /**< Per-channel low-pass delay-lines */
    float x1a8_internalCrosstalk;
    float x1dc_crosstalk; /**< [0.0, 1.0] factor defining how much reflections are allowed to bleed to other channels */
    void _update();
    void _handleReverb(T* audio, int chanIdx, int chanCount, int sampleCount);
    void _doCrosstalk(T* audio, float wet, float dry, int chanCount, int sampleCount);
public:
    EffectReverbHi(float coloration, float mix, float time,
                   float damping, float preDelay, float crosstalk, double sampleRate);
    void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap);

    void setCrosstalk(float crosstalk)
    {
        x1dc_crosstalk = clamp(0.f, crosstalk, 1.f);
        EffectReverb<T, 2, 3>::m_dirty = true;
    }
};

}

#endif // __AMUSE_EFFECTREVERB_HPP__

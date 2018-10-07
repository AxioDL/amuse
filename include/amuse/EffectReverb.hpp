#pragma once

#include "EffectBase.hpp"
#include "amuse/Common.hpp"
#include <memory>

namespace amuse
{

/** Parameters needed to create EffectReverbStd */
struct EffectReverbStdInfo
{
    float coloration = 0.f; /**< [0.0, 1.0] influences filter coefficients to define surface characteristics of a room */
    float mix = 0.f;        /**< [0.0, 1.0] dry/wet mix factor of reverb effect */
    float time = 0.01f;     /**< [0.01, 10.0] time in seconds for reflection decay */
    float damping = 0.f;    /**< [0.0, 1.0] damping factor influencing low-pass filter of reflections */
    float preDelay = 0.f;   /**< [0.0, 0.1] time in seconds before initial reflection heard */

    EffectReverbStdInfo() = default;
    EffectReverbStdInfo(float coloration, float mix, float time, float damping, float preDelay)
    : coloration(coloration), mix(mix), time(time), damping(damping), preDelay(preDelay) {}
};

/** Parameters needed to create EffectReverbHi */
struct EffectReverbHiInfo
{
    float coloration = 0.f; /**< [0.0, 1.0] influences filter coefficients to define surface characteristics of a room */
    float mix = 0.f;        /**< [0.0, 1.0] dry/wet mix factor of reverb effect */
    float time = 0.01f;     /**< [0.01, 10.0] time in seconds for reflection decay */
    float damping = 0.f;    /**< [0.0, 1.0] damping factor influencing low-pass filter of reflections */
    float preDelay = 0.f;   /**< [0.0, 0.1] time in seconds before initial reflection heard */
    float crosstalk = 0.f;  /**< [0.0, 1.0] factor defining how much reflections are allowed to bleed to other channels */

    EffectReverbHiInfo() = default;
    EffectReverbHiInfo(float coloration, float mix, float time, float damping, float preDelay, float crosstalk)
    : coloration(coloration), mix(mix), time(time), damping(damping), preDelay(preDelay), crosstalk(crosstalk) {}
};

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

template <typename T>
class EffectReverbStdImp;

template <typename T>
class EffectReverbHiImp;

/** Reverb effect with configurable reflection filtering */
class EffectReverbStd
{
protected:
    float x140_x1c8_coloration; /**< [0.0, 1.0] influences filter coefficients to define surface characteristics of a
                                   room */
    float x144_x1cc_mix;        /**< [0.0, 1.0] dry/wet mix factor of reverb effect */
    float x148_x1d0_time;       /**< [0.01, 10.0] time in seconds for reflection decay */
    float x14c_x1d4_damping;    /**< [0.0, 1.0] damping factor influencing low-pass filter of reflections */
    float x150_x1d8_preDelay;   /**< [0.0, 0.1] time in seconds before initial reflection heard */
    bool m_dirty = true;        /**< needs update of internal parameter data */

    template <typename T>
    friend class EffectReverbStdImp;
    template <typename T>
    friend class EffectReverbHiImp;
    EffectReverbStd(float coloration, float mix, float time, float damping, float preDelay);

public:
    template <typename T>
    using ImpType = EffectReverbStdImp<T>;

    void setColoration(float coloration)
    {
        x140_x1c8_coloration = clamp(0.f, coloration, 1.f);
        m_dirty = true;
    }
    float getColoration() const { return x140_x1c8_coloration; }

    void setMix(float mix)
    {
        x144_x1cc_mix = clamp(0.f, mix, 1.f);
        m_dirty = true;
    }
    float getMix() const { return x144_x1cc_mix; }

    void setTime(float time)
    {
        x148_x1d0_time = clamp(0.01f, time, 10.f);
        m_dirty = true;
    }
    float getTime() const { return x148_x1d0_time; }

    void setDamping(float damping)
    {
        x14c_x1d4_damping = clamp(0.f, damping, 1.f);
        m_dirty = true;
    }
    float getDamping() const { return x14c_x1d4_damping; }

    void setPreDelay(float preDelay)
    {
        x150_x1d8_preDelay = clamp(0.f, preDelay, 0.1f);
        m_dirty = true;
    }
    float getPreDelay() const { return x150_x1d8_preDelay; }

    void setParams(const EffectReverbStdInfo& info)
    {
        setColoration(info.coloration);
        setMix(info.mix);
        setTime(info.time);
        setDamping(info.damping);
        setPreDelay(info.preDelay);
    }
};

/** Reverb effect with configurable reflection filtering, adds per-channel low-pass and crosstalk */
class EffectReverbHi : public EffectReverbStd
{
    float x1dc_crosstalk; /**< [0.0, 1.0] factor defining how much reflections are allowed to bleed to other channels */

    template <typename T>
    friend class EffectReverbHiImp;
    EffectReverbHi(float coloration, float mix, float time, float damping, float preDelay, float crosstalk);

public:
    template <typename T>
    using ImpType = EffectReverbHiImp<T>;

    void setCrosstalk(float crosstalk)
    {
        x1dc_crosstalk = clamp(0.f, crosstalk, 1.f);
        m_dirty = true;
    }
    float getCrosstalk() const { return x1dc_crosstalk; }

    void setParams(const EffectReverbHiInfo& info)
    {
        setColoration(info.coloration);
        setMix(info.mix);
        setTime(info.time);
        setDamping(info.damping);
        setPreDelay(info.preDelay);
        setCrosstalk(info.crosstalk);
    }
};

/** Standard-quality 2-stage reverb */
template <typename T>
class EffectReverbStdImp : public EffectBase<T>, public EffectReverbStd
{
    ReverbDelayLine x0_AP[8][2] = {};              /**< All-pass delay lines */
    ReverbDelayLine x78_C[8][2] = {};              /**< Comb delay lines */
    float xf0_allPassCoef = 0.f;                   /**< All-pass mix coefficient */
    float xf4_combCoef[8][2] = {};                 /**< Comb mix coefficients */
    float x10c_lpLastout[8] = {};                  /**< Last low-pass results */
    float x118_level = 0.f;                        /**< Internal wet/dry mix factor */
    float x11c_damping = 0.f;                      /**< Low-pass damping */
    int32_t x120_preDelayTime = 0;                 /**< Sample count of pre-delay */
    std::unique_ptr<float[]> x124_preDelayLine[8]; /**< Dedicated pre-delay buffers */
    float* x130_preDelayPtr[8] = {};               /**< Current pre-delay pointers */

    double m_sampleRate; /**< copy of sample rate */
    void _setup(double sampleRate);
    void _update();

public:
    EffectReverbStdImp(float coloration, float mix, float time, float damping, float preDelay, double sampleRate);
    EffectReverbStdImp(const EffectReverbStdInfo& info, double sampleRate)
    : EffectReverbStdImp(info.coloration, info.mix, info.time, info.damping, info.preDelay, sampleRate) {}

    void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap);
    void resetOutputSampleRate(double sampleRate) { _setup(sampleRate); }

    EffectType Isa() const { return EffectType::ReverbStd; }
};

/** High-quality 3-stage reverb with per-channel low-pass and crosstalk */
template <typename T>
class EffectReverbHiImp : public EffectBase<T>, public EffectReverbHi
{
    ReverbDelayLine x0_AP[8][2] = {};              /**< All-pass delay lines */
    ReverbDelayLine x78_LP[8] = {};                /**< Per-channel low-pass delay-lines */
    ReverbDelayLine xb4_C[8][3] = {};              /**< Comb delay lines */
    float x168_allPassCoef = 0.f;                  /**< All-pass mix coefficient */
    float x16c_combCoef[8][3] = {};                /**< Comb mix coefficients */
    float x190_lpLastout[8] = {};                  /**< Last low-pass results */
    float x19c_level = 0.f;                        /**< Internal wet/dry mix factor */
    float x1a0_damping = 0.f;                      /**< Low-pass damping */
    int32_t x1a4_preDelayTime = 0;                 /**< Sample count of pre-delay */
    std::unique_ptr<float[]> x1ac_preDelayLine[8]; /**< Dedicated pre-delay buffers */
    float* x1b8_preDelayPtr[8] = {};               /**< Current pre-delay pointers */
    float x1a8_internalCrosstalk = 0.f;

    double m_sampleRate; /**< copy of sample rate */
    void _setup(double sampleRate);
    void _update();
    void _handleReverb(T* audio, int chanIdx, int chanCount, int sampleCount);
    void _doCrosstalk(T* audio, float wet, float dry, int chanCount, int sampleCount);

public:
    EffectReverbHiImp(float coloration, float mix, float time, float damping, float preDelay, float crosstalk,
                      double sampleRate);
    EffectReverbHiImp(const EffectReverbHiInfo& info, double sampleRate)
    : EffectReverbHiImp(info.coloration, info.mix, info.time, info.damping, info.preDelay, info.crosstalk, sampleRate) {}

    void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap);
    void resetOutputSampleRate(double sampleRate) { _setup(sampleRate); }

    EffectType Isa() const { return EffectType::ReverbHi; }
};
}


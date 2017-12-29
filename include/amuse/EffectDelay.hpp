#ifndef __AMUSE_EFFECTDELAY_HPP__
#define __AMUSE_EFFECTDELAY_HPP__

#include "EffectBase.hpp"
#include "IBackendVoice.hpp"
#include "Common.hpp"
#include <cstdint>
#include <memory>

namespace amuse
{
template <typename T>
class EffectDelayImp;

/** Parameters needed to create EffectDelay */
struct EffectDelayInfo
{
    uint32_t delay[8];         /**< [10, 5000] time in ms of each channel's delay */
    uint32_t feedback[8] = {}; /**< [0, 100] percent to mix delayed signal with input signal */
    uint32_t output[8] = {};   /**< [0, 100] total output percent */

    static uint32_t lerp(uint32_t v0, uint32_t v1, float t) { return (1.f - t) * v0 + t * v1; }

    static void Interp3To8(uint32_t arr[8], uint32_t L, uint32_t R, uint32_t S)
    {
        arr[int(AudioChannel::FrontLeft)] = L;
        arr[int(AudioChannel::FrontRight)] = R;
        arr[int(AudioChannel::RearLeft)] = lerp(L, S, 0.75f);
        arr[int(AudioChannel::RearRight)] = lerp(R, S, 0.75f);
        arr[int(AudioChannel::FrontCenter)] = lerp(L, R, 0.5f);
        arr[int(AudioChannel::LFE)] = arr[int(AudioChannel::FrontCenter)];
        arr[int(AudioChannel::SideLeft)] = lerp(L, S, 0.5f);
        arr[int(AudioChannel::SideRight)] = lerp(R, S, 0.5f);
    }

    EffectDelayInfo() { std::fill_n(delay, 8, 10); }
    EffectDelayInfo(uint32_t delayL, uint32_t delayR, uint32_t delayS,
                    uint32_t feedbackL, uint32_t feedbackR, uint32_t feedbackS,
                    uint32_t outputL, uint32_t outputR, uint32_t outputS)
    {
        Interp3To8(delay, delayL, delayR, delayS);
        Interp3To8(feedback, feedbackL, feedbackR, feedbackS);
        Interp3To8(output, outputL, outputR, outputS);
    }
};

/** Mixes the audio back into itself after specified delay */
class EffectDelay
{
protected:
    uint32_t x3c_delay[8];    /**< [10, 5000] time in ms of each channel's delay */
    uint32_t x48_feedback[8]; /**< [0, 100] percent to mix delayed signal with input signal */
    uint32_t x54_output[8];   /**< [0, 100] total output percent */
    bool m_dirty = true;      /**< needs update of internal parameter data */
public:
    template <typename T>
    using ImpType = EffectDelayImp<T>;

    void setDelay(uint32_t delay)
    {
        delay = clamp(10u, delay, 5000u);
        for (int i = 0; i < 8; ++i)
            x3c_delay[i] = delay;
        m_dirty = true;
    }
    void setChanDelay(int chanIdx, uint32_t delay)
    {
        delay = clamp(10u, delay, 5000u);
        x3c_delay[chanIdx] = delay;
        m_dirty = true;
    }

    void setFeedback(uint32_t feedback)
    {
        feedback = clamp(0u, feedback, 100u);
        for (int i = 0; i < 8; ++i)
            x48_feedback[i] = feedback;
        m_dirty = true;
    }

    void setChanFeedback(int chanIdx, uint32_t feedback)
    {
        feedback = clamp(0u, feedback, 100u);
        x48_feedback[chanIdx] = feedback;
        m_dirty = true;
    }

    void setOutput(uint32_t output)
    {
        output = clamp(0u, output, 100u);
        for (int i = 0; i < 8; ++i)
            x54_output[i] = output;
        m_dirty = true;
    }

    void setChanOutput(int chanIdx, uint32_t output)
    {
        output = clamp(0u, output, 100u);
        x54_output[chanIdx] = output;
        m_dirty = true;
    }

    void setParams(const EffectDelayInfo& info)
    {
        for (int i = 0; i < 8; ++i)
        {
            x3c_delay[i] = clamp(10u, info.delay[i], 5000u);
            x48_feedback[i] = clamp(0u, info.feedback[i], 100u);
            x54_output[i] = clamp(0u, info.output[i], 100u);
        }
        m_dirty = true;
    }
};

/** Type-specific implementation of delay effect */
template <typename T>
class EffectDelayImp : public EffectBase<T>, public EffectDelay
{
    uint32_t x0_currentSize[8];      /**< per-channel delay-line buffer sizes */
    uint32_t xc_currentPos[8];       /**< per-channel block-index */
    uint32_t x18_currentFeedback[8]; /**< [0, 128] feedback attenuator */
    uint32_t x24_currentOutput[8];   /**< [0, 128] total attenuator */

    std::unique_ptr<T[]> x30_chanLines[8]; /**< delay-line buffers for each channel */

    uint32_t m_sampsPerMs;   /**< canonical count of samples per ms for the current backend */
    uint32_t m_blockSamples; /**< count of samples in a 5ms block */
    void _setup(double sampleRate);
    void _update();

public:
    EffectDelayImp(uint32_t initDelay, uint32_t initFeedback, uint32_t initOutput, double sampleRate);
    EffectDelayImp(const EffectDelayInfo& info, double sampleRate);

    void applyEffect(T* audio, size_t frameCount, const ChannelMap& chanMap);
    void resetOutputSampleRate(double sampleRate) { _setup(sampleRate); }
};
}

#endif // __AMUSE_EFFECTDELAY_HPP__

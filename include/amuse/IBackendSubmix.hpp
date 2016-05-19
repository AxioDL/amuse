#ifndef __AMUSE_IBACKENDSUBMIX_HPP__
#define __AMUSE_IBACKENDSUBMIX_HPP__

#include <memory>

namespace amuse
{
class IBackendVoice;
class Voice;

enum class SubmixFormat
{
    Int16,
    Int32,
    Float
};

/** Client-implemented submix instance */
class IBackendSubmix
{
public:
    virtual ~IBackendSubmix() = default;

    /** Set channel-gains for submix (AudioChannel enum for array index) */
    virtual void setChannelGains(const float gains[8])=0;

    /** Amuse obtains a new voice from the platform outputting to this submix */
    virtual std::unique_ptr<IBackendVoice> allocateVoice(Voice& clientVox, double sampleRate, bool dynamicPitch)=0;

    /** Amuse gets fixed sample rate of submix this way */
    virtual double getSampleRate() const=0;

    /** Amuse gets fixed sample format of submix this way */
    virtual SubmixFormat getSampleFormat() const=0;
};

}

#endif // __AMUSE_IBACKENDSUBMIX_HPP__

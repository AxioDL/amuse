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

    /** Set send level for submix (AudioChannel enum for array index) */
    virtual void setSendLevel(IBackendSubmix* submix, float level, bool slew)=0;

    /** Amuse gets fixed sample rate of submix this way */
    virtual double getSampleRate() const=0;

    /** Amuse gets fixed sample format of submix this way */
    virtual SubmixFormat getSampleFormat() const=0;
};

}

#endif // __AMUSE_IBACKENDSUBMIX_HPP__

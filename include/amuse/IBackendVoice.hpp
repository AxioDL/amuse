#ifndef __AMUSE_IBACKENDVOICE_HPP__
#define __AMUSE_IBACKENDVOICE_HPP__

namespace amuse
{

/** Same channel enums from boo, used for matrix coefficient table index */
enum class AudioChannel
{
    FrontLeft,
    FrontRight,
    RearLeft,
    RearRight,
    FrontCenter,
    LFE,
    SideLeft,
    SideRight,
    Unknown = 0xff
};

/** Same structure from boo, used to represent interleaved speaker layout */
struct ChannelMap
{
    unsigned m_channelCount = 0;
    AudioChannel m_channels[8] = {};
};

/** Client-implemented voice instance */
class IBackendVoice
{
public:
    virtual ~IBackendVoice() = default;

    /** Set new sample rate into platform voice (may result in artifacts while playing) */
    virtual void resetSampleRate(double sampleRate)=0;

    /** Set channel-gains for audio source (AudioChannel enum for array index) */
    virtual void setMatrixCoefficients(const float coefs[8], bool slew)=0;

    /** Called by client to dynamically adjust the pitch of voices with dynamic pitch enabled */
    virtual void setPitchRatio(double ratio, bool slew)=0;

    /** Instructs platform to begin consuming sample data; invoking callback as needed */
    virtual void start()=0;

    /** Instructs platform to stop consuming sample data */
    virtual void stop()=0;
};

}

#endif // __AMUSE_IBACKENDVOICE_HPP__

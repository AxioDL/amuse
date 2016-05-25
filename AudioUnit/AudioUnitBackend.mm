#include "AudioUnitBackend.hpp"
#ifdef __APPLE__
#include <Availability.h>
#if __MAC_OS_X_VERSION_MAX_ALLOWED >= 101100
#import <AudioUnit/AudioUnit.h>
#import <CoreAudioKit/CoreAudioKit.h>
#import <AVFoundation/AVFoundation.h>

#if !__has_feature(objc_arc)
#error ARC Required
#endif

#include "logvisor/logvisor.hpp"
#include "audiodev/AudioVoiceEngine.hpp"

static logvisor::Module Log("amuse::AudioUnitBackend");

struct AudioUnitVoiceEngine : boo::BaseAudioVoiceEngine
{
    std::vector<std::unique_ptr<float[]>> m_renderBufs;
    size_t m_frameBytes;

    void render(AudioBufferList* outputData)
    {
        if (m_renderBufs.size() < outputData->mNumberBuffers)
            m_renderBufs.resize(outputData->mNumberBuffers);

        for (int i=0 ; i<outputData->mNumberBuffers ; ++i)
        {
            std::unique_ptr<float[]>& buf = m_renderBufs[i];
            AudioBuffer& auBuf = outputData->mBuffers[i];
            if (!auBuf.mData)
            {
                buf.reset(new float[auBuf.mDataByteSize]);
                auBuf.mData = buf.get();
            }

            _pumpAndMixVoices(auBuf.mDataByteSize / 2 / 4, reinterpret_cast<float*>(auBuf.mData));
        }
    }

    boo::AudioChannelSet _getAvailableSet()
    {
        return boo::AudioChannelSet::Stereo;
    }

    std::vector<std::pair<std::string, std::string>> enumerateMIDIDevices() const
    {
        return {};
    }

    boo::ReceiveFunctor m_midiReceiver = nullptr;

    std::unique_ptr<boo::IMIDIIn> newVirtualMIDIIn(boo::ReceiveFunctor&& receiver)
    {
        m_midiReceiver = std::move(receiver);
        return {};
    }

    std::unique_ptr<boo::IMIDIOut> newVirtualMIDIOut()
    {
        return {};
    }

    std::unique_ptr<boo::IMIDIInOut> newVirtualMIDIInOut(boo::ReceiveFunctor&& receiver)
    {
        return {};
    }

    std::unique_ptr<boo::IMIDIIn> newRealMIDIIn(const char* name, boo::ReceiveFunctor&& receiver)
    {
        return {};
    }

    std::unique_ptr<boo::IMIDIOut> newRealMIDIOut(const char* name)
    {
        return {};
    }

    std::unique_ptr<boo::IMIDIInOut> newRealMIDIInOut(const char* name, boo::ReceiveFunctor&& receiver)
    {
        return {};
    }

    AudioUnitVoiceEngine()
    {
        m_mixInfo.m_channels = _getAvailableSet();
        unsigned chCount = ChannelCount(m_mixInfo.m_channels);

        m_mixInfo.m_sampleRate = 96000.0;
        m_mixInfo.m_sampleFormat = SOXR_FLOAT32_I;
        m_mixInfo.m_bitsPerSample = 32;
        m_5msFrames = 96000 * 5 / 1000;

        boo::ChannelMap& chMapOut = m_mixInfo.m_channelMap;
        chMapOut.m_channelCount = 2;
        chMapOut.m_channels[0] = boo::AudioChannel::FrontLeft;
        chMapOut.m_channels[1] = boo::AudioChannel::FrontRight;

        while (chMapOut.m_channelCount < chCount)
            chMapOut.m_channels[chMapOut.m_channelCount++] = boo::AudioChannel::Unknown;

        m_mixInfo.m_periodFrames = 2400;

        m_frameBytes = m_mixInfo.m_periodFrames * m_mixInfo.m_channelMap.m_channelCount * 4;
    }

    void pumpAndMixVoices()
    {
    }
};

@implementation AmuseAudioUnit
- (id)initWithComponentDescription:(AudioComponentDescription)componentDescription
                           options:(AudioComponentInstantiationOptions)options
                             error:(NSError * _Nullable *)outError;
{
    self = [super initWithComponentDescription:componentDescription options:options error:outError];
    if (!self)
        return nil;

    AUAudioUnitBus* outBus = [[AUAudioUnitBus alloc] initWithFormat:
                              [[AVAudioFormat alloc] initWithCommonFormat:AVAudioPCMFormatFloat32
                                                               sampleRate:96000.0
                                                                 channels:2
                                                              interleaved:TRUE]
                                error:outError];
    if (!outBus)
        return nil;

    m_outs = [[AUAudioUnitBusArray alloc] initWithAudioUnit:self busType:AUAudioUnitBusTypeOutput busses:@[outBus]];

    self.maximumFramesToRender = 2400;
    return self;
}

- (BOOL)allocateRenderResourcesAndReturnError:(NSError * _Nullable *)outError
{
    if (![super allocateRenderResourcesAndReturnError:outError])
        return FALSE;

    m_booBackend = std::make_unique<AudioUnitVoiceEngine>();
    if (!m_booBackend)
    {
        *outError = [NSError errorWithDomain:@"amuse" code:-1
                     userInfo:@{NSLocalizedDescriptionKey:@"Unable to construct boo mixer"}];
        return FALSE;
    }

    m_voxAlloc.emplace(*m_booBackend);
    m_engine.emplace(*m_voxAlloc);
    *outError = nil;
    return TRUE;
}

- (void)deallocateRenderResources
{
    m_engine = std::experimental::nullopt;
    m_voxAlloc = std::experimental::nullopt;
    m_booBackend.reset();
    [super deallocateRenderResources];
}

- (BOOL)renderResourcesAllocated
{
    if (m_engine)
        return TRUE;
    return FALSE;
}

- (AUAudioUnitBusArray*)outputBusses
{
    return m_outs;
}

- (BOOL)musicDeviceOrEffect
{
    return TRUE;
}

- (NSInteger)virtualMIDICableCount
{
    return 1;
}

- (AUInternalRenderBlock)internalRenderBlock
{
    AudioUnitVoiceEngine& voxEngine = static_cast<AudioUnitVoiceEngine&>(*m_booBackend);

    return ^AUAudioUnitStatus(AudioUnitRenderActionFlags* actionFlags, const AudioTimeStamp* timestamp,
             AUAudioFrameCount frameCount, NSInteger outputBusNumber, AudioBufferList* outputData,
             const AURenderEvent* realtimeEventListHead, AURenderPullInputBlock pullInputBlock)
    {
        /* Process MIDI events first */
        if (voxEngine.m_midiReceiver)
        {
            for (const AUMIDIEvent* event = &realtimeEventListHead->MIDI ;
                 event != nullptr ; event = &event->next->MIDI)
            {
                if (event->eventType == AURenderEventMIDI)
                {
                    voxEngine.m_midiReceiver(std::vector<uint8_t>(std::cbegin(event->data),
                                                                  std::cbegin(event->data) + event->length));
                }
            }
        }

        /* Output buffers */
        voxEngine.render(outputData);
        return noErr;
    };
}
@end

namespace amuse
{

void RegisterAudioUnit()
{
    AudioComponentDescription desc = {};
    desc.componentType = 'aumu';
    desc.componentSubType = 'amus';
    desc.componentManufacturer = 'AXDL';
    [AUAudioUnit registerSubclass:[AmuseAudioUnit class] asComponentDescription:desc name:@"Amuse" version:0100];
}

}

#endif
#endif

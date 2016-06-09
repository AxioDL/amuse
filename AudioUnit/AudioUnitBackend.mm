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
#import "AudioUnitViewController.hpp"

static logvisor::Module Log("amuse::AudioUnitBackend");

struct AudioUnitVoiceEngine : boo::BaseAudioVoiceEngine
{
    AudioGroupToken* m_reqGroup = nullptr;
    AudioGroupToken* m_curGroup = nullptr;
    std::vector<float> m_interleavedBuf;
    std::vector<std::unique_ptr<float[]>> m_renderBufs;
    size_t m_renderFrames = 0;
    AudioBufferList* m_outputData = nullptr;

    boo::AudioChannelSet _getAvailableSet()
    {
        return boo::AudioChannelSet::Stereo;
    }

    std::vector<std::pair<std::string, std::string>> enumerateMIDIDevices() const
    {
        return {};
    }

    boo::ReceiveFunctor* m_midiReceiver = nullptr;

    struct MIDIIn : public boo::IMIDIIn
    {
        MIDIIn(bool virt, boo::ReceiveFunctor&& receiver)
        : IMIDIIn(virt, std::move(receiver)) {}
        
        std::string description() const
        {
            return "AudioUnit MIDI";
        }
    };
    
    std::unique_ptr<boo::IMIDIIn> newVirtualMIDIIn(boo::ReceiveFunctor&& receiver)
    {
        std::unique_ptr<boo::IMIDIIn> ret = std::make_unique<MIDIIn>(true, std::move(receiver));
        m_midiReceiver = &ret->m_receiver;
        return ret;
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
    
    bool useMIDILock() const {return false;}

    AudioUnitVoiceEngine()
    {
        m_mixInfo.m_periodFrames = 512;
        m_mixInfo.m_sampleRate = 96000.0;
        m_mixInfo.m_sampleFormat = SOXR_FLOAT32_I;
        m_mixInfo.m_bitsPerSample = 32;
        _buildAudioRenderClient();
    }
    
    void _buildAudioRenderClient()
    {
        m_mixInfo.m_channels = _getAvailableSet();
        unsigned chCount = ChannelCount(m_mixInfo.m_channels);
        
        m_5msFrames = m_mixInfo.m_sampleRate * 5 / 1000;
        
        boo::ChannelMap& chMapOut = m_mixInfo.m_channelMap;
        chMapOut.m_channelCount = 2;
        chMapOut.m_channels[0] = boo::AudioChannel::FrontLeft;
        chMapOut.m_channels[1] = boo::AudioChannel::FrontRight;
        
        while (chMapOut.m_channelCount < chCount)
            chMapOut.m_channels[chMapOut.m_channelCount++] = boo::AudioChannel::Unknown;
    }
    
    void _rebuildAudioRenderClient(double sampleRate, size_t periodFrames)
    {
        m_mixInfo.m_periodFrames = periodFrames;
        m_mixInfo.m_sampleRate = sampleRate;
        _buildAudioRenderClient();
        
        for (boo::AudioVoice* vox : m_activeVoices)
            vox->_resetSampleRate(vox->m_sampleRateIn);
        for (boo::AudioSubmix* smx : m_activeSubmixes)
            smx->_resetOutputSampleRate();
    }

    void pumpAndMixVoices()
    {
        _pumpAndMixVoices(m_renderFrames, m_interleavedBuf.data());

        for (size_t i=0 ; i<m_renderBufs.size() ; ++i)
        {
            std::unique_ptr<float[]>& buf = m_renderBufs[i];
            AudioBuffer& auBuf = m_outputData->mBuffers[i];
            if (!auBuf.mData)
            {
                buf.reset(new float[auBuf.mDataByteSize / 4]);
                auBuf.mData = buf.get();
            }
            for (size_t f=0 ; f<m_renderFrames ; ++f)
            {
                float* bufOut = reinterpret_cast<float*>(auBuf.mData);
                bufOut[f] = m_interleavedBuf[f*2+i];
            }
        }
    }
    
    double getCurrentSampleRate() const {return m_mixInfo.m_sampleRate;}
};

@implementation AmuseAudioUnit
- (id)initWithComponentDescription:(AudioComponentDescription)componentDescription
                             error:(NSError * _Nullable *)outError
                    viewController:(AudioUnitViewController*)vc
{
    m_viewController = vc;
    vc->m_audioUnit = self;
    self = [super initWithComponentDescription:componentDescription error:outError];
    return self;
}

- (id)initWithComponentDescription:(AudioComponentDescription)componentDescription
                           options:(AudioComponentInstantiationOptions)options
                             error:(NSError * _Nullable *)outError
{
    self = [super initWithComponentDescription:componentDescription options:options error:outError];
    if (!self)
        return nil;
    
    AVAudioFormat* format = [[AVAudioFormat alloc] initStandardFormatWithSampleRate:96000.0 channels:2];
    m_outBus = [[AUAudioUnitBus alloc] initWithFormat:format error:outError];
    if (!m_outBus)
        return nil;
    //m_outBus.supportedChannelCounts = @[@1,@2];
    m_outBus.maximumChannelCount = 2;
    
    m_outs = [[AUAudioUnitBusArray alloc] initWithAudioUnit:self
                                                    busType:AUAudioUnitBusTypeOutput
                                                     busses:@[m_outBus]];

    m_booBackend = std::make_unique<AudioUnitVoiceEngine>();
    if (!m_booBackend)
    {
        *outError = [NSError errorWithDomain:@"amuse" code:-1
                                    userInfo:@{NSLocalizedDescriptionKey:@"Unable to construct boo mixer"}];
        return FALSE;
    }
    
    m_voxAlloc.emplace(*m_booBackend);
    m_engine.emplace(*m_voxAlloc);
    dispatch_sync(dispatch_get_main_queue(),
    ^{
        m_filePresenter = [[AudioGroupFilePresenter alloc] initWithAudioGroupClient:self];
    });
    
    self.maximumFramesToRender = 512;
    return self;
}

- (void)requestAudioGroup:(AudioGroupToken*)group
{
    AudioUnitVoiceEngine& voxEngine = static_cast<AudioUnitVoiceEngine&>(*m_booBackend);
    voxEngine.m_reqGroup = group;
}

- (BOOL)allocateRenderResourcesAndReturnError:(NSError **)outError
{
    if (![super allocateRenderResourcesAndReturnError:outError])
        return FALSE;
    
    size_t chanCount = m_outBus.format.channelCount;
    size_t renderFrames = self.maximumFramesToRender;
    
    NSLog(@"Alloc Chans: %zu Frames: %zu SampRate: %f", chanCount, renderFrames, m_outBus.format.sampleRate);
    
    AudioUnitVoiceEngine& voxEngine = static_cast<AudioUnitVoiceEngine&>(*m_booBackend);
    voxEngine.m_renderFrames = renderFrames;
    voxEngine.m_interleavedBuf.resize(renderFrames * std::max(2ul, chanCount));
    voxEngine.m_renderBufs.resize(chanCount);
    voxEngine._rebuildAudioRenderClient(m_outBus.format.sampleRate, renderFrames);
    
    *outError = nil;
    return TRUE;
}

- (void)deallocateRenderResources
{
    [super deallocateRenderResources];
    AudioUnitVoiceEngine& voxEngine = static_cast<AudioUnitVoiceEngine&>(*m_booBackend);
    voxEngine.m_renderBufs.clear();
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
    __block AudioUnitVoiceEngine& voxEngine = static_cast<AudioUnitVoiceEngine&>(*m_booBackend);
    __block amuse::Engine& amuseEngine = *m_engine;
    __block std::shared_ptr<amuse::Sequencer> curSeq;

    return ^AUAudioUnitStatus(AudioUnitRenderActionFlags* actionFlags, const AudioTimeStamp* timestamp,
             AUAudioFrameCount frameCount, NSInteger outputBusNumber, AudioBufferList* outputData,
             const AURenderEvent* realtimeEventListHead, AURenderPullInputBlock pullInputBlock)
    {
        /* Handle group load request */
        AudioGroupToken* reqGroup = voxEngine.m_reqGroup;
        if (voxEngine.m_curGroup != reqGroup)
        {
            voxEngine.m_curGroup = reqGroup;
            if (reqGroup->m_song)
            {
                if (curSeq)
                    curSeq->kill();
                curSeq = amuseEngine.seqPlay(reqGroup->m_id, -1, nullptr);
            }
        }
        
        /* Process MIDI events first */
        if (voxEngine.m_midiReceiver)
        {
            for (const AUMIDIEvent* event = &realtimeEventListHead->MIDI ;
                 event != nullptr ; event = &event->next->MIDI)
            {
                if (event->eventType == AURenderEventMIDI)
                {
                    (*voxEngine.m_midiReceiver)(std::vector<uint8_t>(std::cbegin(event->data),
                                                                     std::cbegin(event->data) + event->length),
                                                event->eventSampleTime / voxEngine.getCurrentSampleRate());
                }
            }
        }

        /* Output buffers */
        voxEngine.m_renderFrames = frameCount;
        voxEngine.m_outputData = outputData;
        amuseEngine.pumpEngine();
        return noErr;
    };
}

- (amuse::Engine&)getAmuseEngine
{
    return *m_engine;
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

#include "VSTBackend.hpp"
#include "audiodev/AudioVoiceEngine.hpp"
#include <logvisor/logvisor.hpp>

struct VSTVoiceEngine : boo::BaseAudioVoiceEngine
{
    std::vector<float> m_interleavedBuf;
    float** m_outputData = nullptr;
    size_t m_renderFrames = 0;

    int m_reqGroup = 0;
    int m_curGroup = 0;
    std::shared_ptr<amuse::Sequencer> m_curSeq;

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
            return "VST MIDI";
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

    VSTVoiceEngine()
    {
        m_mixInfo.m_periodFrames = 1024;
        m_mixInfo.m_sampleRate = 44100.0;
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

        for (size_t i=0 ; i<2 ; ++i)
        {
            float* bufOut = m_outputData[i];
            for (size_t f=0 ; f<m_renderFrames ; ++f)
                bufOut[f] = m_interleavedBuf[f*2+i];
        }
    }

    double getCurrentSampleRate() const {return m_mixInfo.m_sampleRate;}
};

namespace amuse
{

#define kBackendID CCONST ('a','m','u','s')

static logvisor::Module Log("amuse::AudioUnitBackend");

VSTBackend::VSTBackend(audioMasterCallback cb)
: AudioEffectX(cb, 0, 0), m_editor(*this)
{
    isSynth();
    setUniqueID(kBackendID);
    setNumInputs(0);
    setNumOutputs(2);
    setEditor(&m_editor);
    sizeWindow(800, 520);

    m_booBackend = std::make_unique<VSTVoiceEngine>();
    m_voxAlloc.emplace(*m_booBackend);
    m_engine.emplace(*m_voxAlloc);
}

VSTBackend::~VSTBackend()
{
    editor = nullptr;
}

AEffEditor* VSTBackend::getEditor()
{
    return &m_editor;
}

VstInt32 VSTBackend::processEvents(VstEvents* events)
{
    VSTVoiceEngine& engine = static_cast<VSTVoiceEngine&>(*m_booBackend);

    /* Handle group load request */
    int reqGroup = engine.m_reqGroup;
    if (engine.m_curGroup != reqGroup)
    {
        engine.m_curGroup = reqGroup;
        if (engine.m_curSeq)
            engine.m_curSeq->kill();
        engine.m_curSeq = m_engine->seqPlay(reqGroup, -1, nullptr);
    }

    if (engine.m_midiReceiver)
    {
        for (VstInt32 i=0 ; i<events->numEvents ; ++i)
        {
            VstMidiEvent* evt = reinterpret_cast<VstMidiEvent*>(events->events[i]);
            if (evt->type == kVstMidiType)
            {
                (*engine.m_midiReceiver)(std::vector<uint8_t>(std::cbegin(evt->midiData),
                                                              std::cbegin(evt->midiData) + evt->byteSize),
                                         (m_curFrame + evt->deltaFrames) / sampleRate);
            }
        }
    }

    return 1;
}

void VSTBackend::processReplacing(float**, float** outputs, VstInt32 sampleFrames)
{
    VSTVoiceEngine& engine = static_cast<VSTVoiceEngine&>(*m_booBackend);

    /* Output buffers */
    engine.m_renderFrames = sampleFrames;
    engine.m_outputData = outputs;
    m_engine->pumpEngine();

    m_curFrame += sampleFrames;
}

VstInt32 VSTBackend::canDo(char* text)
{
    VstInt32 returnCode = 0;

    if (!strcmp(text, "receiveVstEvents"))
        returnCode = 1;
    else if (!strcmp(text, "receiveVstMidiEvent"))
        returnCode = 1;

    return returnCode;
}

VstPlugCategory VSTBackend::getPlugCategory()
{
    return kPlugCategSynth;
}

bool VSTBackend::getEffectName(char* text)
{
    strcpy(text, "Amuse");
    return true;
}

bool VSTBackend::getProductString(char* text)
{
    strcpy(text, "Amuse");
    return true;
}

bool VSTBackend::getVendorString(char* text)
{
    strcpy(text, "AxioDL");
    return true;
}

bool VSTBackend::getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text)
{
    strcpy(text, "Sampler");
    return true;
}

bool VSTBackend::getOutputProperties(VstInt32 index, VstPinProperties* properties)
{
    bool returnCode = false;
    if (index == 0)
    {
        strcpy(properties->label, "Amuse Out");
        properties->flags = kVstPinIsStereo | kVstPinIsActive;
        properties->arrangementType = kSpeakerArrStereo;
        returnCode = true;
    }
    return returnCode;
}

VstInt32 VSTBackend::getNumMidiInputChannels()
{
    return 1;
}

void VSTBackend::setSampleRate(float sampleRate)
{
    AudioEffectX::setSampleRate(sampleRate);
    VSTVoiceEngine& engine = static_cast<VSTVoiceEngine&>(*m_booBackend);
    engine._rebuildAudioRenderClient(sampleRate, engine.mixInfo().m_periodFrames);
}

void VSTBackend::setBlockSize(VstInt32 blockSize)
{
    AudioEffectX::setBlockSize(blockSize);
    VSTVoiceEngine& engine = static_cast<VSTVoiceEngine&>(*m_booBackend);
    engine.m_interleavedBuf.resize(blockSize * 2);
    engine._rebuildAudioRenderClient(engine.mixInfo().m_sampleRate, blockSize);
}

}

AudioEffect* createEffectInstance(audioMasterCallback audioMaster)
{
    return new amuse::VSTBackend(audioMaster);
}

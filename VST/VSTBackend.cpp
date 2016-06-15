#include "VSTBackend.hpp"
#include "audiodev/AudioVoiceEngine.hpp"
#include <Shlobj.h>
#include <logvisor/logvisor.hpp>

#undef min
#undef max

struct VSTVoiceEngine : boo::BaseAudioVoiceEngine
{
    std::vector<float> m_interleavedBuf;
    float** m_outputData = nullptr;
    size_t m_renderFrames = 0;
    size_t m_curBufFrame = 0;

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
        m_curBufFrame = m_5msFrames;
        m_mixInfo.m_periodFrames = m_5msFrames;
        m_interleavedBuf.resize(m_5msFrames * 2);

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
        for (size_t f=0 ; f<m_renderFrames ;)
        {
            size_t remRenderFrames = std::min(m_renderFrames - f, m_5msFrames - m_curBufFrame);
            for (size_t i=0 ; i<2 ; ++i)
            {
                float* bufOut = m_outputData[i];
                for (size_t lf=0 ; lf<remRenderFrames ; ++lf)
                    bufOut[f+lf] = m_interleavedBuf[(m_curBufFrame+lf)*2+i];
            }
            m_curBufFrame += remRenderFrames;
            f += remRenderFrames;

            if (m_curBufFrame == m_5msFrames)
            {
                _pumpAndMixVoices(m_5msFrames, m_interleavedBuf.data());
                m_curBufFrame = 0;

                remRenderFrames = std::min(m_renderFrames - f, m_5msFrames);
                for (size_t i=0 ; i<2 ; ++i)
                {
                    float* bufOut = m_outputData[i];
                    for (size_t lf=0 ; lf<remRenderFrames ; ++lf)
                        bufOut[f+lf] = m_interleavedBuf[lf*2+i];
                }
                f += remRenderFrames;
            }
        }
    }

    double getCurrentSampleRate() const {return m_mixInfo.m_sampleRate;}
};

namespace amuse
{

#define kBackendID CCONST ('a','m','u','s')

static logvisor::Module Log("amuse::AudioUnitBackend");

VSTBackend::VSTBackend(audioMasterCallback cb)
: AudioEffectX(cb, 0, 0), m_filePresenter(*this), m_editor(*this)
{
    isSynth();
    setUniqueID(kBackendID);
    setNumInputs(0);
    setNumOutputs(2);
    setEditor(&m_editor);
    sizeWindow(600, 420);
    programsAreChunks();

    m_booBackend = std::make_unique<VSTVoiceEngine>();
    m_voxAlloc.emplace(*m_booBackend);
    m_engine.emplace(*m_voxAlloc);

    WCHAR path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, path)))
    {
        m_userDir = std::wstring(path) + L"\\Amuse";
        CreateDirectory(m_userDir.c_str(), nullptr);
    }

    m_filePresenter.update();
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
    std::unique_lock<std::mutex> lk(m_lock);
    VSTVoiceEngine& engine = static_cast<VSTVoiceEngine&>(*m_booBackend);

    /* Handle group load request */
    if (m_curGroup != m_reqGroup)
    {
        m_curGroup = m_reqGroup;
        if (m_curSeq)
            m_curSeq->kill();
        m_curSeq = m_engine->seqPlay(m_reqGroup, -1, nullptr);
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
    std::unique_lock<std::mutex> lk(m_lock);
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
    engine._rebuildAudioRenderClient(engine.mixInfo().m_sampleRate, blockSize);
}

void VSTBackend::loadGroupFile(int collectionIdx, int fileIdx)
{
    std::unique_lock<std::mutex> lk(m_lock);

    if (m_curSeq)
    {
        m_curSeq->kill();
        m_curSeq.reset();
        m_curGroup = -1;
        m_reqGroup = -1;
    }

    if (collectionIdx < m_filePresenter.m_iteratorVec.size())
    {
        AudioGroupFilePresenter::CollectionIterator& it = m_filePresenter.m_iteratorVec[collectionIdx];
        if (fileIdx < it->second->m_iteratorVec.size())
        {
            AudioGroupCollection::GroupIterator& git = it->second->m_iteratorVec[fileIdx];
            if (m_curData)
                m_curData->removeFromEngine(*m_engine);
            git->second->addToEngine(*m_engine);
            m_curData = git->second.get();
        }
    }
}

void VSTBackend::setGroup(int groupIdx, bool immediate)
{
    std::unique_lock<std::mutex> lk(m_lock);

    if (!m_curData)
        return;

    if (groupIdx < m_curData->m_groupTokens.size())
    {
        const AudioGroupDataCollection::GroupToken& groupTok = m_curData->m_groupTokens[groupIdx];
        m_reqGroup = groupTok.m_groupId;
        if (immediate)
        {
            if (m_curSeq)
                m_curSeq->kill();
            m_curSeq = m_engine->seqPlay(groupTok.m_groupId, -1, nullptr);
        }
    }
}

void VSTBackend::setNormalProgram(int programNo)
{
    std::unique_lock<std::mutex> lk(m_lock);

    if (!m_curSeq)
        return;
    m_curSeq->setChanProgram(0, programNo);
}

void VSTBackend::setDrumProgram(int programNo)
{
    std::unique_lock<std::mutex> lk(m_lock);

    if (!m_curSeq)
        return;
    m_curSeq->setChanProgram(9, programNo);
}

VstInt32 VSTBackend::getChunk(void** data, bool)
{
    size_t allocSz = 14;
    if (m_curData)
        allocSz += (m_curData->m_path.size() - m_userDir.size() - 1) * 2;

    uint8_t* buf = new uint8_t[allocSz];
    if (m_curData)
        memmove(buf, m_curData->m_path.data() + m_userDir.size() + 1, allocSz - 12);
    else
        *reinterpret_cast<wchar_t*>(buf) = L'\0';
    uint32_t* intVals = reinterpret_cast<uint32_t*>(buf + allocSz - 12);
    intVals[0] = m_listenProg;
    intVals[1] = m_editor.m_selGroupIdx;
    intVals[2] = m_editor.m_selPageIdx;
    *data = buf;
    return allocSz;
}

VstInt32 VSTBackend::setChunk(void* data, VstInt32 byteSize, bool)
{
    if (byteSize < 14)
        return 0;

    wchar_t* path = reinterpret_cast<wchar_t*>(data);
    uint32_t* intVals = reinterpret_cast<uint32_t*>(path + wcslen(path) + 1);
    std::wstring targetPath = m_userDir + L'\\' + path;
    m_listenProg = intVals[0];
    uint32_t groupIdx = intVals[1];
    uint32_t pageIdx = intVals[2];

    size_t colIdx = 0;
    for (auto& collection : m_filePresenter.m_audioGroupCollections)
    {
        size_t fileIdx = 0;
        for (auto& file : collection.second->m_groups)
        {
            if (!file.second->m_path.compare(targetPath))
            {
                m_editor.selectCollection(LPARAM(0x80000000 | (colIdx << 16) | fileIdx));
                m_editor.selectGroup(groupIdx);
                m_editor.selectPage(pageIdx);
                m_editor._reselectColumns();
            }
            ++fileIdx;
        }
        ++colIdx;
    }

    return 1;
}

}

AudioEffect* createEffectInstance(audioMasterCallback audioMaster)
{
    return new amuse::VSTBackend(audioMaster);
}

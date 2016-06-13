#ifndef __AMUSE_VSTBACKEND_HPP__
#define __AMUSE_VSTBACKEND_HPP__

#include "audioeffectx.h"
#include "VSTEditor.hpp"
#include <memory>
#include "optional.hpp"

#include "amuse/BooBackend.hpp"
#include "amuse/Engine.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendSubmix.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"
#include "AudioGroupFilePresenter.hpp"

namespace amuse
{
class VSTBackend;

/** Backend voice allocator implementation for AudioUnit mixer */
class VSTBackendVoiceAllocator : public BooBackendVoiceAllocator
{
public:
    VSTBackendVoiceAllocator(boo::IAudioVoiceEngine& booEngine)
    : BooBackendVoiceAllocator(booEngine) {}
};

/** Actual plugin implementation class */
class VSTBackend : public AudioEffectX
{
    std::unique_ptr<boo::IAudioVoiceEngine> m_booBackend;
    std::experimental::optional<amuse::VSTBackendVoiceAllocator> m_voxAlloc;
    std::experimental::optional<amuse::Engine> m_engine;
    size_t m_curFrame = 0;
    std::wstring m_userDir;
    AudioGroupFilePresenter m_filePresenter;
    VSTEditor m_editor;
public:
    VSTBackend(audioMasterCallback cb);
    ~VSTBackend();
    AEffEditor* getEditor();
    VstInt32 processEvents(VstEvents* events);
    void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
    VstInt32 canDo(char* text);
    VstPlugCategory getPlugCategory();
    bool getEffectName(char* text);
    bool getProductString(char* text);
    bool getVendorString(char* text);
    bool getProgramNameIndexed(VstInt32 category, VstInt32 index, char* text);
    bool getOutputProperties(VstInt32 index, VstPinProperties* properties);
    VstInt32 getNumMidiInputChannels();
    void setSampleRate(float sampleRate);
    void setBlockSize(VstInt32 blockSize);

    amuse::Engine& getAmuseEngine() {return *m_engine;}
    const std::wstring& getUserDir() const {return m_userDir;}
    AudioGroupFilePresenter& getFilePresenter() {return m_filePresenter;}
};

}

#endif // __AMUSE_VSTBACKEND_HPP__

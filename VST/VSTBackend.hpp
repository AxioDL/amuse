#ifndef __AMUSE_VSTBACKEND_HPP__
#define __AMUSE_VSTBACKEND_HPP__

#include <vst36/audioeffectx.h>
#include "VSTEditor.hpp"
#include <memory>
#include "optional.hpp"

#include "amuse/BooBackend.hpp"
#include "amuse/Engine.hpp"
#include "amuse/IBackendVoice.hpp"
#include "amuse/IBackendSubmix.hpp"
#include "amuse/IBackendVoiceAllocator.hpp"

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
    VSTEditor m_editor;
public:
    VSTBackend(audioMasterCallback cb);
    AEffEditor* getEditor();
    VstInt32 processEvents(VstEvents* events);
    void processReplacing(float** inputs, float** outputs, VstInt32 sampleFrames);
    VstInt32 canDo(char* text);
    VstPlugCategory getPlugCategory();
    bool getProductString(char* text);
    bool getVendorString(char* text);
    bool getOutputProperties(VstInt32 index, VstPinProperties* properties);
    VstInt32 getNumMidiInputChannels();
    void setSampleRate(float sampleRate);
    void setBlockSize(VstInt32 blockSize);
};

}

#endif // __AMUSE_VSTBACKEND_HPP__

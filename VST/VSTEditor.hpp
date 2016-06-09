#ifndef __AMUSE_VSTEDITOR_HPP__
#define __AMUSE_VSTEDITOR_HPP__

#include <vst36/aeffeditor.h>

namespace amuse
{
class VSTBackend;

/** Editor UI class */
class VSTEditor : public AEffEditor
{
    VSTBackend& m_backend;
    ERect m_windowRect = {10, 10, 410, 520};
public:
    VSTEditor(VSTBackend& backend);

    bool getRect(ERect** rect);
    bool open(void* ptr);
};

}

#endif // __AMUSE_VSTEDITOR_HPP__

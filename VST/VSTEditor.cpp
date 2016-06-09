#include "VSTEditor.hpp"
#include "VSTBackend.hpp"

namespace amuse
{

VSTEditor::VSTEditor(VSTBackend& backend)
: AEffEditor(&backend), m_backend(backend)
{
}

bool VSTEditor::getRect(ERect** rect)
{
    *rect = &m_windowRect;
    return true;
}

bool VSTEditor::open(void* ptr)
{
    AEffEditor::open(ptr);
#if _WIN32
#elif __APPLE__
#else
#endif
    return true;
}

}

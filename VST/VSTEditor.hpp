#ifndef __AMUSE_VSTEDITOR_HPP__
#define __AMUSE_VSTEDITOR_HPP__

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>

#include "aeffeditor.h"

namespace amuse
{
class VSTBackend;

/** Editor UI class */
class VSTEditor : public AEffEditor
{
    VSTBackend& m_backend;
    ERect m_windowRect = {0, 0, 520, 800};

    HWND m_rootView;
    HWND m_collectionListView;
    HWND m_groupFileListView;
    HWND m_groupListView;
    HWND m_pageListView;

    static LRESULT CALLBACK WindowProc(
      _In_ HWND   hwnd,
      _In_ UINT   uMsg,
      _In_ WPARAM wParam,
      _In_ LPARAM lParam
    );
    static LRESULT CALLBACK ColHeaderWindowProc(
      _In_ HWND   hwnd,
      _In_ UINT   uMsg,
      _In_ WPARAM wParam,
      _In_ LPARAM lParam
    );
public:
    VSTEditor(VSTBackend& backend);

    bool getRect(ERect** rect);
    bool open(void* ptr);
    void close();
    void update();

    void selectCollection(int idx);
    void selectGroupFile(int idx);
    void selectGroup(int idx);
    void selectPage(int idx);
};

}

#endif // __AMUSE_VSTEDITOR_HPP__
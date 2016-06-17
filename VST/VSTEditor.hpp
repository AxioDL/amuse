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
    friend class VSTBackend;
    friend class AudioGroupFilePresenter;
    friend struct AudioGroupCollection;

    VSTBackend& m_backend;
    ERect m_windowRect = {0, 0, 420, 600};

    HWND m_rootView = 0;
    HWND m_collectionTree = 0;
    HWND m_collectionAdd = 0;
    HWND m_collectionRemove = 0;
    HWND m_groupListView = 0;
    HWND m_pageListView = 0;

    int m_selCollectionIdx = -1;
    int m_selFileIdx = -1;
    int m_selGroupIdx = -1;
    int m_selPageIdx = -1;
    int m_lastLParam = -1;

    HTREEITEM m_deferredCollectionSel = 0;

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

    void _reselectColumns();
public:
    VSTEditor(VSTBackend& backend);

    bool getRect(ERect** rect);
    bool open(void* ptr);
    void close();
    void update();

    void addAction();
    void removeAction();

    void selectCollection(LPARAM idx);
    void selectGroup(int idx);
    void selectPage(int idx);
    void reselectPage();
    void selectNormalPage(int idx);
    void selectDrumPage(int idx);
};

}

#endif // __AMUSE_VSTEDITOR_HPP__

#include "VSTEditor.hpp"
#include "VSTBackend.hpp"
#include "FileOpenDialog.hpp"
#include <Windowsx.h>
#include <shellapi.h>
#include <algorithm>
#include <Shlwapi.h>

#undef min
#undef max

extern void* hInstance;
static WNDPROC OriginalListViewProc = 0;
static HBRUSH gGreyBorderBrush;

namespace amuse
{

VSTEditor::VSTEditor(VSTBackend& backend) : AEffEditor(&backend), m_backend(backend) {}

bool VSTEditor::getRect(ERect** rect)
{
    *rect = &m_windowRect;
    return true;
}

LRESULT CALLBACK VSTEditor::WindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    VSTEditor& editor = *reinterpret_cast<VSTEditor*>(GetWindowLongPtrW(hwnd, 0));
    switch (uMsg)
    {
    case WM_NOTIFY:
    {
        NMHDR& itemAct = *reinterpret_cast<LPNMHDR>(lParam);
        switch (itemAct.code)
        {
        case HDN_BEGINTRACK:
            return TRUE;
        case NM_CLICK:
        {
            NMITEMACTIVATE& itemAct = *reinterpret_cast<LPNMITEMACTIVATE>(lParam);
            if (itemAct.hdr.hwndFrom == editor.m_groupListView)
                editor.selectGroup(itemAct.iItem);
            else if (itemAct.hdr.hwndFrom == editor.m_pageListView)
                editor.selectPage(itemAct.iItem);
            return 0;
        }
        case TVN_SELCHANGED:
        {
            if (editor.m_deferredCollectionSel)
                return 0;
            NMTREEVIEW& itemAct = *reinterpret_cast<LPNMTREEVIEW>(lParam);
            if (itemAct.hdr.hwndFrom == editor.m_collectionTree)
                editor.selectCollection(itemAct.itemNew.lParam);
            return 0;
        }
        case TVN_GETDISPINFO:
        {
            NMTVDISPINFO& treeDispInfo = *reinterpret_cast<LPNMTVDISPINFO>(lParam);
            if (treeDispInfo.item.mask & TVIF_CHILDREN)
            {
            }
            return 0;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }
    case WM_COMMAND:
    {
        switch (HIWORD(wParam))
        {
        case BN_CLICKED:
        {
            HWND button = HWND(lParam);
            if (button == editor.m_collectionAdd)
                editor.addAction();
            else if (button == editor.m_collectionRemove)
                editor.removeAction();
            return 0;
        }
        default:
            return DefWindowProc(hwnd, uMsg, wParam, lParam);
        }
    }
    case WM_ERASEBKGND:
    {
        RECT rect;
        GetClientRect(hwnd, &rect);
        FillRect(HDC(wParam), &rect, gGreyBorderBrush);
        return 1;
    }
    default:
        return DefWindowProc(hwnd, uMsg, wParam, lParam);
    }
}

LRESULT CALLBACK VSTEditor::ColHeaderWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
    switch (uMsg)
    {
    case WM_SETCURSOR:
        return TRUE;
    case WM_LBUTTONDBLCLK:
        return 0;
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC dc = BeginPaint(hwnd, &ps);
        RECT rect;
        GetClientRect(hwnd, &rect);

        TRIVERTEX verts[] = {{rect.left, rect.top, 0x6000, 0x6000, 0x7000, 0xff00},
                             {rect.right, rect.bottom, 0x2000, 0x2000, 0x2800, 0xff00}};
        GRADIENT_RECT grect = {0, 1};
        GradientFill(dc, verts, 2, &grect, 1, GRADIENT_FILL_RECT_V);

        SetTextColor(dc, RGB(255, 255, 255));
        SetBkMode(dc, TRANSPARENT);
        SelectObject(dc, GetStockObject(ANSI_VAR_FONT));
        rect.left += 6;

        LPWSTR str = LPWSTR(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
        DrawText(dc, str, -1, &rect, DT_SINGLELINE | DT_VCENTER | DT_END_ELLIPSIS);

        EndPaint(hwnd, &ps);
        return 0;
    }
    }

    return CallWindowProc(OriginalListViewProc, hwnd, uMsg, wParam, lParam);
}

void VSTEditor::_reselectColumns()
{
    if (m_deferredCollectionSel)
    {
        TreeView_SelectItem(m_collectionTree, m_deferredCollectionSel);
        m_deferredCollectionSel = 0;
    }
    if (m_selGroupIdx != -1)
        ListView_SetItemState(m_groupListView, m_selGroupIdx, LVIS_FOCUSED | LVIS_SELECTED, 0xf);
    if (m_selPageIdx != -1)
        ListView_SetItemState(m_pageListView, m_selPageIdx, LVIS_FOCUSED | LVIS_SELECTED, 0xf);
}

bool VSTEditor::open(void* ptr)
{
    AEffEditor::open(ptr);
    HWND hostView = HWND(ptr);
    gGreyBorderBrush = CreateSolidBrush(RGB(100, 100, 100));

    WNDCLASSW notifyCls = {CS_HREDRAW | CS_VREDRAW,
                           WindowProc,
                           0,
                           8,
                           HINSTANCE(hInstance),
                           nullptr,
                           nullptr,
                           nullptr,
                           nullptr,
                           L"VSTNotify"};
    RegisterClassW(&notifyCls);

    m_rootView = CreateWindowW(L"VSTNotify", L"", WS_CHILD, 0, 0, m_windowRect.right, m_windowRect.bottom, hostView,
                               nullptr, HINSTANCE(hInstance), nullptr);
    SetWindowLongPtrW(m_rootView, 0, LONG_PTR(this));
    ShowWindow(m_rootView, SW_SHOW);

    TVINSERTSTRUCT treeItem = {};
    treeItem.hParent = TVI_ROOT;
    treeItem.hInsertAfter = TVI_LAST;

    treeItem.item.mask = TVIF_CHILDREN | TVIF_TEXT;
    treeItem.item.cChildren = 1;
    treeItem.item.pszText = L"Root A";

    LVCOLUMN column = {};
    column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH;
    column.fmt = LVCFMT_LEFT | LVCFMT_FIXED_WIDTH;
    column.cx = 199;

    m_collectionTree =
        CreateWindowW(WC_TREEVIEW, L"",
                      WS_CHILD | WS_CLIPSIBLINGS | TVS_SHOWSELALWAYS | TVS_HASLINES | TVS_LINESATROOT | TVS_HASBUTTONS,
                      1, 25, 199, m_windowRect.bottom - m_windowRect.top - 26, m_rootView, nullptr, nullptr, nullptr);
    TreeView_SetBkColor(m_collectionTree, RGB(64, 64, 64));
    TreeView_SetTextColor(m_collectionTree, RGB(255, 255, 255));
    HTREEITEM rootItemA = TreeView_InsertItem(m_collectionTree, &treeItem);
    treeItem.item.pszText = L"Root B";
    HTREEITEM rootItemB = TreeView_InsertItem(m_collectionTree, &treeItem);
    treeItem.hParent = rootItemA;
    treeItem.item.cChildren = 0;
    treeItem.item.pszText = L"Child A";
    TreeView_InsertItem(m_collectionTree, &treeItem);
    treeItem.item.pszText = L"Child B";
    TreeView_InsertItem(m_collectionTree, &treeItem);
    treeItem.hParent = rootItemB;
    treeItem.item.pszText = L"Child A";
    TreeView_InsertItem(m_collectionTree, &treeItem);
    treeItem.item.pszText = L"Child B";
    TreeView_InsertItem(m_collectionTree, &treeItem);
    ShowWindow(m_collectionTree, SW_SHOW);

    HWND cHeader = CreateWindowW(WC_HEADER, L"", WS_CHILD, 1, 1, 199, 24, m_rootView, nullptr, nullptr, nullptr);
    SetWindowLongPtrW(cHeader, GWLP_USERDATA, LONG_PTR(L"Collection"));
    OriginalListViewProc = WNDPROC(SetWindowLongPtr(cHeader, GWLP_WNDPROC, LONG_PTR(ColHeaderWindowProc)));
    ShowWindow(cHeader, SW_SHOW);

    HWND gHeader = CreateWindowW(WC_HEADER, L"", WS_CHILD, 201, 1, 199, 24, m_rootView, nullptr, nullptr, nullptr);
    SetWindowLongPtrW(gHeader, GWLP_USERDATA, LONG_PTR(L"Group"));
    OriginalListViewProc = WNDPROC(SetWindowLongPtr(gHeader, GWLP_WNDPROC, LONG_PTR(ColHeaderWindowProc)));
    ShowWindow(gHeader, SW_SHOW);

    HWND pHeader = CreateWindowW(WC_HEADER, L"", WS_CHILD, 401, 1, 198, 24, m_rootView, nullptr, nullptr, nullptr);
    SetWindowLongPtrW(pHeader, GWLP_USERDATA, LONG_PTR(L"Page"));
    OriginalListViewProc = WNDPROC(SetWindowLongPtr(pHeader, GWLP_WNDPROC, LONG_PTR(ColHeaderWindowProc)));
    ShowWindow(pHeader, SW_SHOW);

    m_collectionAdd =
        CreateWindowW(WC_BUTTON, L"+", WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON, 1,
                      m_windowRect.bottom - m_windowRect.top - 25, 25, 24, m_rootView, nullptr, nullptr, nullptr);
    SetWindowFont(m_collectionAdd, GetStockObject(ANSI_FIXED_FONT), FALSE);
    Button_Enable(m_collectionAdd, TRUE);
    SetWindowPos(m_collectionAdd, HWND_TOP, 1, m_windowRect.bottom - m_windowRect.top - 25, 25, 24, SWP_SHOWWINDOW);

    m_collectionRemove =
        CreateWindowW(WC_BUTTON, L"-", WS_CHILD | WS_CLIPSIBLINGS | BS_PUSHBUTTON, 26,
                      m_windowRect.bottom - m_windowRect.top - 25, 25, 24, m_rootView, nullptr, nullptr, nullptr);
    SetWindowFont(m_collectionRemove, GetStockObject(ANSI_FIXED_FONT), FALSE);
    Button_Enable(m_collectionRemove, FALSE);
    SetWindowPos(m_collectionRemove, HWND_TOP, 26, m_windowRect.bottom - m_windowRect.top - 25, 25, 24, SWP_SHOWWINDOW);

    m_groupListView =
        CreateWindowW(WC_LISTVIEW, L"",
                      WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | LVS_NOSORTHEADER,
                      201, 25, 199, m_windowRect.bottom - m_windowRect.top - 26, m_rootView, nullptr, nullptr, nullptr);
    column.pszText = L"Group";
    HWND header = ListView_GetHeader(m_groupListView);
    SetWindowLongPtrW(header, GWLP_USERDATA, LONG_PTR(column.pszText));
    SetWindowLongPtr(header, GWLP_WNDPROC, LONG_PTR(ColHeaderWindowProc));
    ListView_SetBkColor(m_groupListView, RGB(64, 64, 64));
    ListView_SetTextBkColor(m_groupListView, CLR_NONE);
    ListView_SetTextColor(m_groupListView, RGB(255, 255, 255));
    ListView_InsertColumn(m_groupListView, 0, &column);
    ShowWindow(m_groupListView, SW_SHOW);

    m_pageListView =
        CreateWindowW(WC_LISTVIEW, L"",
                      WS_CHILD | LVS_REPORT | LVS_SINGLESEL | LVS_SHOWSELALWAYS | LVS_NOCOLUMNHEADER | LVS_NOSORTHEADER,
                      401, 25, 198, m_windowRect.bottom - m_windowRect.top - 26, m_rootView, nullptr, nullptr, nullptr);
    column.pszText = L"Page";
    column.cx = 198 - GetSystemMetrics(SM_CXVSCROLL);
    header = ListView_GetHeader(m_pageListView);
    SetWindowLongPtrW(header, GWLP_USERDATA, LONG_PTR(column.pszText));
    SetWindowLongPtr(header, GWLP_WNDPROC, LONG_PTR(ColHeaderWindowProc));
    ListView_SetBkColor(m_pageListView, RGB(64, 64, 64));
    ListView_SetTextBkColor(m_pageListView, CLR_NONE);
    ListView_SetTextColor(m_pageListView, RGB(255, 255, 255));
    ListView_InsertColumn(m_pageListView, 0, &column);
    ShowWindow(m_pageListView, SW_SHOW);

    update();
    return true;
}

void VSTEditor::close()
{
    AEffEditor::close();
    UnregisterClassW(L"VSTNotify", HINSTANCE(hInstance));
}

void VSTEditor::update()
{
    m_backend.getFilePresenter().populateCollectionColumn(*this);
    m_backend.loadGroupFile(m_selCollectionIdx, m_selFileIdx);
    m_backend.getFilePresenter().populateGroupColumn(*this, m_selCollectionIdx, m_selFileIdx);
    m_backend.setGroup(m_selGroupIdx, true);
    m_backend.getFilePresenter().populatePageColumn(*this, m_selCollectionIdx, m_selFileIdx, m_selGroupIdx);
    selectPage(m_selPageIdx);
    _reselectColumns();
}

void VSTEditor::addAction()
{
    std::wstring path = openDB();
    if (path.size())
    {
        amuse::ContainerRegistry::Type containerType;
        std::vector<std::pair<std::wstring, amuse::IntrusiveAudioGroupData>> data =
            amuse::ContainerRegistry::LoadContainer(path.c_str(), containerType);
        if (data.empty())
        {
            wchar_t msg[512];
            SNPrintf(msg, 512, L"Unable to load Audio Groups from %s", path.c_str());
            MessageBoxW(nullptr, msg, L"Invalid Data File", MB_OK | MB_ICONERROR);
            return;
        }

        SystemString name(amuse::ContainerRegistry::TypeToName(containerType));
        if (containerType == amuse::ContainerRegistry::Type::Raw4)
        {
            size_t dotpos = path.rfind(L'.');
            if (dotpos != std::string::npos)
                name.assign(path.cbegin(), path.cbegin() + dotpos);
            size_t slashpos = name.rfind(L'\\');
            size_t fslashpos = name.rfind(L'/');
            if (slashpos == std::string::npos)
                slashpos = fslashpos;
            else if (fslashpos != std::string::npos)
                slashpos = std::max(slashpos, fslashpos);
            if (slashpos != std::string::npos)
                name.assign(name.cbegin() + slashpos + 1, name.cend());
        }

        m_backend.getFilePresenter().addCollection(name, std::move(data));
        update();
    }
}

void VSTEditor::removeAction()
{
    if (m_selCollectionIdx == -1)
        return;
    m_backend.getFilePresenter().removeCollection(m_selCollectionIdx);
    m_backend.getFilePresenter().populateCollectionColumn(*this);
    m_selCollectionIdx = -1;
    m_selFileIdx = -1;
    m_selGroupIdx = -1;
    m_backend.getFilePresenter().populateGroupColumn(*this, m_selCollectionIdx, m_selFileIdx);
    m_backend.getFilePresenter().populatePageColumn(*this, m_selCollectionIdx, m_selFileIdx, m_selGroupIdx);
    Button_Enable(m_collectionRemove, FALSE);
}

void VSTEditor::selectCollection(LPARAM idx)
{
    if (0x80000000 & idx)
    {
        /* Sub-item */
        int rootIdx = (idx >> 16) & 0x7fff;
        int subIdx = idx & 0xffff;
        Button_Enable(m_collectionRemove, FALSE);
        m_selCollectionIdx = rootIdx;
        m_selFileIdx = subIdx;
        m_backend.loadGroupFile(m_selCollectionIdx, m_selFileIdx);
        m_backend.getFilePresenter().populateGroupColumn(*this, rootIdx, subIdx);
    }
    else
    {
        /* Root-item */
        int rootIdx = (idx >> 16) & 0x7fff;
        m_selCollectionIdx = rootIdx;
        Button_Enable(m_collectionRemove, TRUE);
    }
}

void VSTEditor::selectGroup(int idx)
{
    m_selGroupIdx = idx;
    m_backend.setGroup(m_selGroupIdx, false);
    m_backend.getFilePresenter().populatePageColumn(*this, m_selCollectionIdx, m_selFileIdx, m_selGroupIdx);
    m_lastLParam = -1;
}

void VSTEditor::selectPage(int idx)
{
    m_selPageIdx = idx;
    LV_ITEM item = {};
    item.mask = LVIF_PARAM;
    item.iItem = idx;
    ListView_GetItem(m_pageListView, &item);
    m_lastLParam = item.lParam;
    if (item.lParam & 0x80000000)
        selectDrumPage(item.lParam & 0x7fffffff);
    else
        selectNormalPage(item.lParam & 0x7fffffff);
}

void VSTEditor::reselectPage()
{
    if (m_lastLParam != -1)
    {
        if (m_lastLParam & 0x80000000)
            m_backend._setDrumProgram(m_lastLParam & 0x7fffffff);
        else
            m_backend._setNormalProgram(m_lastLParam & 0x7fffffff);
    }
}

void VSTEditor::selectNormalPage(int idx) { m_backend.setNormalProgram(idx); }

void VSTEditor::selectDrumPage(int idx) { m_backend.setDrumProgram(idx); }
}

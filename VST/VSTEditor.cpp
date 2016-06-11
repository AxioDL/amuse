#include "VSTEditor.hpp"
#include "VSTBackend.hpp"

extern void* hInstance;
static WNDPROC OriginalListViewProc = 0;
static HBRUSH gGreyBorderBrush;

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

LRESULT CALLBACK VSTEditor::WindowProc(HWND hwnd,
                                       UINT uMsg,
                                       WPARAM wParam,
                                       LPARAM lParam)
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
            if (itemAct.hdr.hwndFrom == editor.m_collectionTree)
                editor.selectCollection(itemAct.iItem);
            else if (itemAct.hdr.hwndFrom == editor.m_groupListView)
                editor.selectGroup(itemAct.iItem);
            else if (itemAct.hdr.hwndFrom == editor.m_pageListView)
                editor.selectPage(itemAct.iItem);
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

LRESULT CALLBACK VSTEditor::ColHeaderWindowProc(HWND hwnd,
                                                UINT uMsg,
                                                WPARAM wParam,
                                                LPARAM lParam)
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

        TRIVERTEX verts[] =
        {
            {rect.left, rect.top, 0x6000, 0x6000, 0x7000, 0xff00},
            {rect.right, rect.bottom, 0x2000, 0x2000, 0x2800, 0xff00}
        };
        GRADIENT_RECT grect = {0, 1};
        GradientFill(dc, verts, 2, &grect, 1, GRADIENT_FILL_RECT_V);

        SetTextColor(dc, RGB(255,255,255));
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

bool VSTEditor::open(void* ptr)
{
    AEffEditor::open(ptr);
    HWND hostView = HWND(ptr);
    gGreyBorderBrush = CreateSolidBrush(RGB(100,100,100));

    WNDCLASSW notifyCls =
    {
        CS_HREDRAW | CS_VREDRAW,
        WindowProc,
        0,
        8,
        HINSTANCE(hInstance),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        L"VSTNotify"
    };
    RegisterClassW(&notifyCls);

    m_rootView = CreateWindowW(L"VSTNotify",
                               L"",
                               WS_CHILD,
                               0, 0,
                               m_windowRect.right,
                               m_windowRect.bottom,
                               hostView,
                               nullptr,
                               HINSTANCE(hInstance),
                               nullptr);
    SetWindowLongPtrW(m_rootView, 0, LONG_PTR(this));
    ShowWindow(m_rootView, SW_SHOW);

    LVCOLUMN column = {};
    column.mask = LVCF_FMT | LVCF_TEXT | LVCF_WIDTH | LVCF_SUBITEM;
    column.fmt = LVCFMT_LEFT | LVCFMT_FIXED_WIDTH;
    column.cx = 200;

    LVITEM item = {};
    item.mask = LVIF_TEXT | LVIF_GROUPID;
    item.pszText = L"Test";
    item.iGroupId = 1;

    m_collectionTree = CreateWindowW(WC_TREEVIEW,
                                     L"",
                                     WS_CHILD | WS_BORDER | TVS_HASLINES,
                                     0, 24,
                                     201,
                                     m_windowRect.bottom - m_windowRect.top - 24,
                                     m_rootView,
                                     nullptr,
                                     nullptr,
                                     nullptr);
    TreeView_SetBkColor(m_collectionTree, RGB(64,64,64));
    ShowWindow(m_collectionTree, SW_SHOW);

    m_collectionHeader = CreateWindowW(WC_HEADER,
                                       L"",
                                       WS_CHILD,
                                       1, 1,
                                       200,
                                       24,
                                       m_rootView,
                                       nullptr,
                                       nullptr,
                                       nullptr);
    SetWindowLongPtrW(m_collectionHeader, GWLP_USERDATA, LONG_PTR(L"Collection"));
    OriginalListViewProc = WNDPROC(SetWindowLongPtr(m_collectionHeader, GWLP_WNDPROC, LONG_PTR(ColHeaderWindowProc)));
    ShowWindow(m_collectionHeader, SW_SHOW);

    m_groupListView = CreateWindowW(WC_LISTVIEW,
                                    L"",
                                    WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
                                    200, 0,
                                    201,
                                    m_windowRect.bottom - m_windowRect.top,
                                    m_rootView,
                                    nullptr,
                                    nullptr,
                                    nullptr);
    column.pszText = L"Group";
    HWND header = ListView_GetHeader(m_groupListView);
    SetWindowLongPtrW(header, GWLP_USERDATA, LONG_PTR(column.pszText));
    SetWindowLongPtr(header, GWLP_WNDPROC, LONG_PTR(ColHeaderWindowProc));
    ListView_SetBkColor(m_groupListView, RGB(64,64,64));
    ListView_InsertColumn(m_groupListView, 0, &column);
    ShowWindow(m_groupListView, SW_SHOW);

    m_pageListView = CreateWindowW(WC_LISTVIEW,
                                   L"",
                                   WS_CHILD | WS_BORDER | LVS_REPORT | LVS_SINGLESEL | LVS_NOSORTHEADER,
                                   400, 0,
                                   200,
                                   m_windowRect.bottom - m_windowRect.top,
                                   m_rootView,
                                   nullptr,
                                   nullptr,
                                   nullptr);
    column.pszText = L"Page";
    header = ListView_GetHeader(m_pageListView);
    SetWindowLongPtrW(header, GWLP_USERDATA, LONG_PTR(column.pszText));
    SetWindowLongPtr(header, GWLP_WNDPROC, LONG_PTR(ColHeaderWindowProc));
    ListView_SetBkColor(m_pageListView, RGB(64,64,64));
    ListView_InsertColumn(m_pageListView, 0, &column);
    ShowWindow(m_pageListView, SW_SHOW);

    return true;
}

void VSTEditor::close()
{
    AEffEditor::close();
    UnregisterClassW(L"VSTNotify", HINSTANCE(hInstance));
}

void VSTEditor::update()
{

}

void VSTEditor::selectCollection(int idx)
{

}

void VSTEditor::selectGroup(int idx)
{

}

void VSTEditor::selectPage(int idx)
{

}

}

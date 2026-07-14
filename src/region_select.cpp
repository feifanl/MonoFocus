#include "region_select.h"

#include <windowsx.h>
#include <algorithm>
#include <cstdio>

#include "constants.h"

namespace RegionSelect {
namespace {

HWND                       g_hwnd = nullptr;
std::function<void(RECT)>  g_onRect;
std::vector<RECT>          g_outlines;      // virtual-screen px (existing + this session)
bool                       g_dragging = false;
POINT                      g_anchor = {};   // client px
POINT                      g_cur    = {};   // client px
int                        g_vx = 0, g_vy = 0, g_vw = 0, g_vh = 0;   // virtual screen

constexpr BYTE kVeilAlpha = 64;             // ~25% black veil

// Client rect (anchor..cur), normalized so left<right, top<bottom.
RECT CurrentDragClient() {
    RECT r;
    r.left   = std::min(g_anchor.x, g_cur.x);
    r.top    = std::min(g_anchor.y, g_cur.y);
    r.right  = std::max(g_anchor.x, g_cur.x);
    r.bottom = std::max(g_anchor.y, g_cur.y);
    return r;
}

void Paint(HWND hwnd) {
    PAINTSTRUCT ps;
    HDC hdc = BeginPaint(hwnd, &ps);
    RECT rc;
    GetClientRect(hwnd, &rc);

    // Double-buffer to avoid flicker while dragging.
    HDC     mem = CreateCompatibleDC(hdc);
    HBITMAP bmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
    HGDIOBJ oldBmp = SelectObject(mem, bmp);

    FillRect(mem, &rc, static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH)));

    HPEN    pen    = CreatePen(PS_DASH, 1, RGB(255, 255, 255));
    HGDIOBJ oldPen = SelectObject(mem, pen);
    HGDIOBJ oldBr  = SelectObject(mem, GetStockObject(NULL_BRUSH));   // hollow

    // Committed regions (convert virtual -> client).
    for (const RECT& v : g_outlines)
        Rectangle(mem, v.left - g_vx, v.top - g_vy, v.right - g_vx, v.bottom - g_vy);

    // Live rubber band.
    if (g_dragging) {
        RECT d = CurrentDragClient();
        Rectangle(mem, d.left, d.top, d.right, d.bottom);
    }

    BitBlt(hdc, 0, 0, rc.right, rc.bottom, mem, 0, 0, SRCCOPY);

    SelectObject(mem, oldBr);
    SelectObject(mem, oldPen);
    DeleteObject(pen);
    SelectObject(mem, oldBmp);
    DeleteObject(bmp);
    DeleteDC(mem);
    EndPaint(hwnd, &ps);
}

void CommitDrag() {
    RECT d = CurrentDragClient();
    if (d.right - d.left >= kMinRegionPx && d.bottom - d.top >= kMinRegionPx) {
        RECT v = { d.left + g_vx, d.top + g_vy, d.right + g_vx, d.bottom + g_vy };
        g_outlines.push_back(v);

        wchar_t dbg[128];
        swprintf_s(dbg, L"[MonoFocus] region committed: %d,%d,%d,%d\n",
                   v.left, v.top, v.right, v.bottom);
        OutputDebugStringW(dbg);

        if (g_onRect) g_onRect(v);
    }
}

LRESULT CALLBACK SelectWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_LBUTTONDOWN:
            g_dragging = true;
            g_anchor = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
            g_cur = g_anchor;
            SetCapture(hwnd);
            return 0;

        case WM_MOUSEMOVE:
            if (g_dragging) {
                g_cur = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_LBUTTONUP:
            if (g_dragging) {
                g_dragging = false;
                g_cur = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };
                ReleaseCapture();
                CommitDrag();                       // stays active for more drags
                InvalidateRect(hwnd, nullptr, FALSE);
            }
            return 0;

        case WM_RBUTTONUP:
            DestroyWindow(hwnd);
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE || wParam == VK_RETURN)
                DestroyWindow(hwnd);
            return 0;

        case WM_PAINT:
            Paint(hwnd);
            return 0;

        case WM_ERASEBKGND:
            return 1;                               // handled in WM_PAINT

        case WM_DESTROY:
            if (GetCapture() == hwnd) ReleaseCapture();
            g_hwnd = nullptr;
            g_dragging = false;
            g_onRect = nullptr;
            g_outlines.clear();
            return 0;

        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void EnsureClass(HINSTANCE hInst) {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = SelectWndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_CROSS);
    wc.lpszClassName = kSelectClass;
    RegisterClassExW(&wc);
    registered = true;
}

}  // namespace

void Begin(HWND owner, const std::vector<RECT>& existing,
           std::function<void(RECT)> onRect) {
    if (g_hwnd) return;                              // already active

    HINSTANCE hInst = reinterpret_cast<HINSTANCE>(GetModuleHandleW(nullptr));
    EnsureClass(hInst);

    g_vx = GetSystemMetrics(SM_XVIRTUALSCREEN);
    g_vy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    g_vw = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    g_vh = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    g_onRect   = std::move(onRect);
    g_outlines = existing;                           // outline pre-existing regions
    g_dragging = false;

    g_hwnd = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TOOLWINDOW,
        kSelectClass, L"", WS_POPUP,
        g_vx, g_vy, g_vw, g_vh, owner, nullptr, hInst, nullptr);
    if (!g_hwnd) {
        g_onRect = nullptr;
        g_outlines.clear();
        return;
    }

    SetLayeredWindowAttributes(g_hwnd, 0, kVeilAlpha, LWA_ALPHA);
    ShowWindow(g_hwnd, SW_SHOW);
    SetForegroundWindow(g_hwnd);
    SetFocus(g_hwnd);
    InvalidateRect(g_hwnd, nullptr, FALSE);
}

bool IsActive() { return g_hwnd != nullptr; }

HWND Hwnd() { return g_hwnd; }

}  // namespace RegionSelect

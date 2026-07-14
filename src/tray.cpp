#include "tray.h"

#include <shellapi.h>
#include <cstdint>
#include <cmath>
#include <cstdio>

#include "app.h"
#include "constants.h"
#include "settings.h"

namespace Tray {
namespace {

constexpr UINT kIconId = 1;

HWND    g_owner   = nullptr;
HICON   g_iconOn  = nullptr;
HICON   g_iconOff = nullptr;
bool    g_added   = false;

// --- runtime-drawn icons -------------------------------------------------

// Draws a 32x32 anti-aliased filled disc into a 32-bit straight-alpha bitmap
// and wraps it in an HICON. on -> green (#2ea043), off -> gray (#8b949e).
HICON MakeIcon(bool on) {
    constexpr int S = 32;

    BITMAPV5HEADER bi = {};
    bi.bV5Size        = sizeof(bi);
    bi.bV5Width       = S;
    bi.bV5Height      = -S;                 // top-down
    bi.bV5Planes      = 1;
    bi.bV5BitCount    = 32;
    bi.bV5Compression = BI_BITFIELDS;
    bi.bV5RedMask     = 0x00FF0000;
    bi.bV5GreenMask   = 0x0000FF00;
    bi.bV5BlueMask    = 0x000000FF;
    bi.bV5AlphaMask   = 0xFF000000;

    void* bits = nullptr;
    HDC hdc = GetDC(nullptr);
    HBITMAP color = CreateDIBSection(hdc, reinterpret_cast<BITMAPINFO*>(&bi),
                                     DIB_RGB_COLORS, &bits, nullptr, 0);
    ReleaseDC(nullptr, hdc);
    if (!color) return nullptr;

    const uint32_t r = on ? 0x2e : 0x8b;
    const uint32_t g = on ? 0xa0 : 0x94;
    const uint32_t b = on ? 0x43 : 0x9e;

    auto* px = static_cast<uint32_t*>(bits);
    const double cx = (S - 1) / 2.0;
    const double cy = (S - 1) / 2.0;
    const double radius = 14.0;

    for (int y = 0; y < S; ++y) {
        for (int x = 0; x < S; ++x) {
            const double dist = std::sqrt((x - cx) * (x - cx) + (y - cy) * (y - cy));
            // 1px anti-aliased edge.
            double cov = radius - dist + 0.5;
            if (cov < 0.0) cov = 0.0;
            if (cov > 1.0) cov = 1.0;
            const uint32_t a = static_cast<uint32_t>(cov * 255.0 + 0.5);
            px[y * S + x] = (a << 24) | (r << 16) | (g << 8) | b;
        }
    }

    HBITMAP mask = CreateBitmap(S, S, 1, 1, nullptr);   // unused (alpha carries it)
    ICONINFO ii = {};
    ii.fIcon    = TRUE;
    ii.hbmColor = color;
    ii.hbmMask  = mask;
    HICON icon = CreateIconIndirect(&ii);

    DeleteObject(color);
    DeleteObject(mask);
    return icon;
}

void EnsureIcons() {
    if (!g_iconOn)  g_iconOn  = MakeIcon(true);
    if (!g_iconOff) g_iconOff = MakeIcon(false);
}

NOTIFYICONDATAW BaseData() {
    NOTIFYICONDATAW nid = {};
    nid.cbSize = sizeof(nid);
    nid.hWnd   = g_owner;
    nid.uID    = kIconId;
    return nid;
}

// --- context menu --------------------------------------------------------

// Builds and tracks the tray menu. State args drive checkmarks/graying;
// commit 2 passes defaults, commit 3 passes live App state.
void ShowMenu(HWND hwnd, bool monoOn, bool hasRegions, float saturation,
              bool restoreState, bool autostart) {
    HMENU menu = CreatePopupMenu();

    AppendMenuW(menu, MF_STRING | (monoOn ? MF_CHECKED : 0),
                IDM_TOGGLE, L"Toggle monochrome\tCtrl+Alt+M");
    AppendMenuW(menu, MF_STRING, IDM_SELECT, L"Select region…\tCtrl+Alt+R");
    AppendMenuW(menu, MF_STRING | (hasRegions ? 0 : MF_GRAYED),
                IDM_CLEAR, L"Clear regions\tCtrl+Alt+X");

    HMENU sat = CreatePopupMenu();
    AppendMenuW(sat, MF_STRING, IDM_SAT_0,  L"0% (grayscale)");
    AppendMenuW(sat, MF_STRING, IDM_SAT_25, L"25%");
    AppendMenuW(sat, MF_STRING, IDM_SAT_50, L"50%");
    AppendMenuW(sat, MF_STRING, IDM_SAT_75, L"75%");
    // Check the preset nearest to the current saturation.
    const int presets[] = { 0, 25, 50, 75 };
    const UINT presetIds[] = { IDM_SAT_0, IDM_SAT_25, IDM_SAT_50, IDM_SAT_75 };
    int satPct = static_cast<int>(saturation * 100.0f + 0.5f);
    int best = 0;
    for (int i = 1; i < 4; ++i)
        if (std::abs(satPct - presets[i]) < std::abs(satPct - presets[best])) best = i;
    CheckMenuRadioItem(sat, IDM_SAT_0, IDM_SAT_75, presetIds[best], MF_BYCOMMAND);
    AppendMenuW(menu, MF_POPUP, reinterpret_cast<UINT_PTR>(sat), L"Saturation");

    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING | (restoreState ? MF_CHECKED : 0),
                IDM_RESTORE_STATE, L"Restore state at launch");
    AppendMenuW(menu, MF_STRING | (autostart ? MF_CHECKED : 0),
                IDM_AUTOSTART, L"Start with Windows");
    AppendMenuW(menu, MF_STRING, IDM_OPEN_SETTINGS,   L"Open settings file");
    AppendMenuW(menu, MF_STRING, IDM_RELOAD_SETTINGS, L"Reload settings");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, IDM_QUIT, L"Quit");

    POINT pt;
    GetCursorPos(&pt);
    SetForegroundWindow(hwnd);      // so the menu dismisses on focus loss
    TrackPopupMenuEx(menu, TPM_RIGHTBUTTON, pt.x, pt.y, hwnd, nullptr);
    PostMessageW(hwnd, WM_NULL, 0, 0);
    DestroyMenu(menu);
}

}  // namespace

void Add(HWND hwnd) {
    g_owner = hwnd;
    EnsureIcons();
    NOTIFYICONDATAW nid = BaseData();
    nid.uFlags           = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    nid.uCallbackMessage = WM_TRAY;
    nid.hIcon            = g_iconOff;
    wcscpy_s(nid.szTip, kAppName);
    g_added = Shell_NotifyIconW(NIM_ADD, &nid) ? true : false;
}

void ReAdd() {
    if (g_owner) {
        g_added = false;
        Add(g_owner);
    }
}

void Remove() {
    if (g_added) {
        NOTIFYICONDATAW nid = BaseData();
        Shell_NotifyIconW(NIM_DELETE, &nid);
        g_added = false;
    }
    if (g_iconOn)  { DestroyIcon(g_iconOn);  g_iconOn  = nullptr; }
    if (g_iconOff) { DestroyIcon(g_iconOff); g_iconOff = nullptr; }
}

void SyncState(const App& app) {
    if (!g_added) return;
    EnsureIcons();
    const AppState& s = app.State();

    NOTIFYICONDATAW nid = BaseData();
    nid.uFlags = NIF_ICON | NIF_TIP;
    nid.hIcon  = s.mono ? g_iconOn : g_iconOff;
    if (s.mono) {
        const int grayPct = static_cast<int>((1.0f - s.saturation) * 100.0f + 0.5f);
        swprintf_s(nid.szTip, L"%s — on (gray %d%%)", kAppName, grayPct);
    } else {
        swprintf_s(nid.szTip, L"%s — off", kAppName);
    }
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

void OnCallback(HWND hwnd, LPARAM lParam, App& app) {
    switch (LOWORD(lParam)) {
        case WM_LBUTTONUP:
            app.Toggle();
            break;
        case WM_RBUTTONUP:
        case WM_CONTEXTMENU: {
            const AppState& s   = app.State();
            const Settings& cfg = app.CurrentSettings();
            ShowMenu(hwnd, s.mono, !s.regions.empty(), s.saturation,
                     cfg.restoreState, cfg.autostart);
            break;
        }
        default:
            break;
    }
}

void Balloon(const std::wstring& title, const std::wstring& text) {
    if (!g_added) return;
    NOTIFYICONDATAW nid = BaseData();
    nid.uFlags = NIF_INFO;
    wcscpy_s(nid.szInfoTitle, title.c_str());
    wcscpy_s(nid.szInfo, text.c_str());
    nid.dwInfoFlags = NIIF_INFO;
    Shell_NotifyIconW(NIM_MODIFY, &nid);
}

}  // namespace Tray

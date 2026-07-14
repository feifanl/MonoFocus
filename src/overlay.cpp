#include "overlay.h"

#include <magnification.h>

#include "color_matrix.h"
#include "constants.h"
#include "region_select.h"

namespace {

LRESULT CALLBACK HostWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == WM_CREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA,
                          reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
        return 0;
    }

    auto* self = reinterpret_cast<Overlay*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    switch (msg) {
        case WM_TIMER:
            if (self && wParam == kTimerRefresh) self->Tick();
            return 0;
        case WM_DESTROY:
            KillTimer(hwnd, kTimerRefresh);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

void EnsureHostClass(HINSTANCE hInst) {
    static bool registered = false;
    if (registered) return;
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = HostWndProc;
    wc.hInstance     = hInst;
    wc.hbrBackground = static_cast<HBRUSH>(GetStockObject(BLACK_BRUSH));
    wc.lpszClassName = kHostClass;
    RegisterClassExW(&wc);
    registered = true;
}

}  // namespace

bool Overlay::Show(float saturation, const std::vector<RECT>& regions) {
    if (host_) {                                   // already up: update in place
        UpdateSaturation(saturation);
        UpdateRegions(regions);
        return true;
    }

    saturation_ = saturation;
    regions_    = regions;
    vx_ = GetSystemMetrics(SM_XVIRTUALSCREEN);
    vy_ = GetSystemMetrics(SM_YVIRTUALSCREEN);
    vw_ = GetSystemMetrics(SM_CXVIRTUALSCREEN);
    vh_ = GetSystemMetrics(SM_CYVIRTUALSCREEN);

    HINSTANCE hInst = reinterpret_cast<HINSTANCE>(GetModuleHandleW(nullptr));
    EnsureHostClass(hInst);

    host_ = CreateWindowExW(
        WS_EX_TOPMOST | WS_EX_LAYERED | WS_EX_TRANSPARENT |
            WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE,
        kHostClass, L"", WS_POPUP,
        vx_, vy_, vw_, vh_, nullptr, nullptr, hInst, this);
    if (!host_) return false;

    SetLayeredWindowAttributes(host_, 0, 255, LWA_ALPHA);   // fully opaque

    mag_ = CreateWindowW(WC_MAGNIFIER, L"", WS_CHILD | WS_VISIBLE,
                         0, 0, vw_, vh_, host_, nullptr, hInst, nullptr);
    if (!mag_) {
        DestroyWindow(host_);
        host_ = nullptr;
        return false;
    }

    MAGTRANSFORM xform = {};                                 // identity = 1.0x
    xform.v[0][0] = xform.v[1][1] = xform.v[2][2] = 1.0f;
    MagSetWindowTransform(mag_, &xform);

    MAGCOLOREFFECT eff = SaturationMatrix(saturation_);
    MagSetColorEffect(mag_, &eff);

    SetFilterList();                                         // no self-capture echo

    RECT src = { vx_, vy_, vx_ + vw_, vy_ + vh_ };
    MagSetWindowSource(mag_, src);

    ApplyHoles();
    ShowWindow(host_, SW_SHOWNOACTIVATE);
    SetTimer(host_, kTimerRefresh, 16, nullptr);
    return true;
}

void Overlay::SetFilterList() {
    if (!mag_) return;
    // Exclude our own host (mandatory: prevents a 1.0x feedback echo) and the
    // region-select veil if it is up (so it isn't magnified into the gray area).
    HWND filtered[2];
    int  n = 0;
    filtered[n++] = host_;
    if (RegionSelect::IsActive() && RegionSelect::Hwnd())
        filtered[n++] = RegionSelect::Hwnd();
    MagSetWindowFilterList(mag_, MW_FILTERMODE_EXCLUDE, n, filtered);
}

void Overlay::RefreshFilterList() {
    SetFilterList();
}

void Overlay::ApplyHoles() {
    if (!host_) return;
    HRGN rgn = CreateRectRgn(0, 0, vw_, vh_);
    for (const RECT& r : regions_) {                        // virtual-screen px
        HRGN hole = CreateRectRgn(r.left - vx_, r.top - vy_,
                                  r.right - vx_, r.bottom - vy_);
        CombineRgn(rgn, rgn, hole, RGN_DIFF);
        DeleteObject(hole);
    }
    SetWindowRgn(host_, rgn, TRUE);                         // system owns rgn now
}

void Overlay::UpdateRegions(const std::vector<RECT>& regions) {
    regions_ = regions;
    ApplyHoles();
}

void Overlay::UpdateSaturation(float s) {
    saturation_ = s;
    if (mag_) {
        MAGCOLOREFFECT eff = SaturationMatrix(s);
        MagSetColorEffect(mag_, &eff);
    }
}

void Overlay::Tick() {
    if (!mag_) return;
    RECT src = { vx_, vy_, vx_ + vw_, vy_ + vh_ };
    MagSetWindowSource(mag_, src);
    InvalidateRect(mag_, nullptr, TRUE);
}

void Overlay::Hide() {
    if (host_) {
        KillTimer(host_, kTimerRefresh);
        DestroyWindow(host_);                               // child magnifier dies with it
        host_ = nullptr;
        mag_  = nullptr;
    }
}

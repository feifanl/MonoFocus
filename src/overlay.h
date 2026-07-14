#pragma once
#include <windows.h>
#include <vector>

// Mode B: a topmost, click-through host window spanning the virtual screen with
// a WC_MAGNIFIER child at 1.0x rendering a desaturated copy of the desktop.
// Each color region is a hole punched in the host via SetWindowRgn(RGN_DIFF),
// so inside a region the user sees the real desktop — true color, live, zero
// added latency, clicks reach the real app (PLAN §2, §5).

class Overlay {
public:
    // Creates host + magnifier and shows it. If already shown, updates in place.
    bool Show(float saturation, const std::vector<RECT>& regions);

    void UpdateRegions(const std::vector<RECT>& regions);   // re-punch holes only
    void UpdateSaturation(float s);                         // MagSetColorEffect only
    void RefreshFilterList();                               // re-exclude self + veil
    void Hide();
    bool IsShown() const { return host_ != nullptr; }

    // Internal: invoked by the host WndProc on the refresh timer. Not for callers.
    void Tick();

private:
    void ApplyHoles();
    void SetFilterList();

    HWND  host_ = nullptr;
    HWND  mag_  = nullptr;
    std::vector<RECT> regions_;
    int   vx_ = 0, vy_ = 0, vw_ = 0, vh_ = 0;
    float saturation_ = 0.0f;
};

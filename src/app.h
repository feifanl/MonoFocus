#pragma once
#include <windows.h>
#include <vector>

// Single owner of all mutable state. Every change funnels through Apply(),
// the state machine (PLAN §5 app). Commit 3 implements Off <-> Mode A only;
// Mode B (overlay), region select, settings, and environment handling arrive
// in later commits.

enum class Mode { Off, FullscreenEffect, Overlay };   // Off | A | B

struct AppState {
    bool  mono = false;
    float saturation = 0.0f;          // 0 = gray, 1 = no effect
    std::vector<RECT> regions;        // virtual-screen physical px
};

class App {
public:
    explicit App(HWND mainWnd);

    void Toggle();                    // mono = !mono -> Apply
    void SetSaturation(float s);      // clamp [0,1] -> Apply
    void StepSaturation(int dir);     // +/- kSatStep -> Apply
    void BeginRegionSelect();         // enable mono if off, launch region_select
    void AddRegion(RECT r);           // region_select callback -> Apply
    void ClearRegions();              // regions.clear() -> Apply
    void Shutdown();                  // clear effect / destroy overlay

    Mode CurrentMode() const { return current_; }
    const AppState& State() const { return state_; }

private:
    void Apply();                     // THE state machine

    HWND     mainWnd_;
    AppState state_;
    Mode     current_ = Mode::Off;
};

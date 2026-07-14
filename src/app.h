#pragma once
#include <windows.h>
#include <vector>

#include "overlay.h"

struct Settings;

// Single owner of all mutable state. Every change funnels through Apply(),
// the state machine (PLAN §5 app). Environment handling arrives in a later
// commit.

enum class Mode { Off, FullscreenEffect, Overlay };   // Off | A | B

struct AppState {
    bool  mono = false;
    float saturation = 0.0f;          // 0 = gray, 1 = no effect
    std::vector<RECT> regions;        // virtual-screen physical px
};

class App {
public:
    App(HWND mainWnd, Settings& settings);

    void Toggle();                    // mono = !mono -> Apply
    void SetSaturation(float s);      // clamp [0,1] -> Apply
    void StepSaturation(int dir);     // +/- kSatStep -> Apply
    void BeginRegionSelect();         // enable mono if off, launch region_select
    void AddRegion(RECT r);           // region_select callback -> Apply
    void ClearRegions();              // regions.clear() -> Apply
    void Shutdown();                  // clear effect / destroy overlay + save state

    void RestoreFromSettings();       // restore mono/saturation/regions at launch
    void OnSettingsReloaded();        // re-apply after a settings reload
    void FinishTransition();          // deferred flash-free drop (WM_APP_FINISH_TRANSITION)

    Mode CurrentMode() const { return current_; }
    const AppState& State() const { return state_; }
    const Settings& CurrentSettings() const { return settings_; }

private:
    void Apply();                     // THE state machine

    HWND      mainWnd_;
    Settings& settings_;
    AppState  state_;
    Mode      current_ = Mode::Off;
    Overlay   overlay_;
    bool      pendingClearFullscreen_ = false;
};

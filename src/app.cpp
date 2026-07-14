#include "app.h"

#include <algorithm>

#include "constants.h"
#include "effect_fullscreen.h"
#include "tray.h"

App::App(HWND mainWnd) : mainWnd_(mainWnd) {}

void App::Toggle() {
    state_.mono = !state_.mono;
    Apply();
}

void App::SetSaturation(float s) {
    state_.saturation = std::clamp(s, 0.0f, 1.0f);
    Apply();
}

void App::StepSaturation(int dir) {
    SetSaturation(state_.saturation + dir * kSatStep);
}

void App::ClearRegions() {
    state_.regions.clear();
    Apply();
}

void App::Shutdown() {
    // Commit 3: only Mode A exists. Later commits also destroy the overlay here.
    FullscreenEffect::Clear();
    current_ = Mode::Off;
}

// The state machine. Desired mode: !mono -> Off; no regions -> A; else -> B.
// Mode B is not implemented until commit 6, so it currently falls back to A.
void App::Apply() {
    Mode desired = !state_.mono         ? Mode::Off
                 : state_.regions.empty() ? Mode::FullscreenEffect
                                          : Mode::Overlay;
    if (desired == Mode::Overlay)
        desired = Mode::FullscreenEffect;   // TODO(commit 6): real Mode B

    switch (desired) {
        case Mode::Off:
            if (current_ != Mode::Off)
                FullscreenEffect::Clear();
            break;
        case Mode::FullscreenEffect:
            FullscreenEffect::Set(state_.saturation);
            break;
        case Mode::Overlay:
            break;   // unreachable in commit 3
    }
    current_ = desired;

    Tray::SyncState(*this);
}

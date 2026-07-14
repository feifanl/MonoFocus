#include "app.h"

#include <algorithm>

#include "constants.h"
#include "effect_fullscreen.h"
#include "region_select.h"
#include "settings.h"
#include "tray.h"

App::App(HWND mainWnd, Settings& settings)
    : mainWnd_(mainWnd), settings_(settings) {
    // Saturation is a preference: honor it even when state isn't restored.
    state_.saturation = std::clamp(settings_.saturation, 0.0f, 1.0f);
}

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

void App::BeginRegionSelect() {
    // Selecting a color region only makes sense against gray: enable mono first.
    if (!state_.mono) {
        state_.mono = true;
        Apply();
    }
    if (RegionSelect::IsActive()) return;
    RegionSelect::Begin(mainWnd_, state_.regions,
                        [this](RECT r) { this->AddRegion(r); });
    // If Mode B is already up, exclude the freshly created veil from magnification.
    if (overlay_.IsShown())
        overlay_.RefreshFilterList();
}

void App::AddRegion(RECT r) {
    state_.regions.push_back(r);
    Apply();
}

void App::ClearRegions() {
    state_.regions.clear();
    Apply();
}

void App::Shutdown() {
    settings_.SaveState(state_);
    overlay_.Hide();
    FullscreenEffect::Clear();
    current_ = Mode::Off;
}

void App::RestoreFromSettings() {
    state_.saturation = std::clamp(settings_.saturation, 0.0f, 1.0f);
    state_.mono       = settings_.lastMono;
    state_.regions    = settings_.lastRegions;
    Apply();
}

void App::OnSettingsReloaded() {
    // Pick up an edited saturation; hotkeys are re-registered by the caller.
    state_.saturation = std::clamp(settings_.saturation, 0.0f, 1.0f);
    Apply();
}

// The state machine. Desired mode: !mono -> Off; no regions -> A (fullscreen
// effect); else -> B (overlay). Transitions are ordered flash-free (PLAN §2):
// bring the new surface up, then drop the old one (deferred a message pass for
// A->B so the overlay has rendered before the fullscreen gray is removed).
void App::Apply() {
    const Mode desired = !state_.mono          ? Mode::Off
                       : state_.regions.empty() ? Mode::FullscreenEffect
                                                : Mode::Overlay;

    if (desired == current_) {
        // Same mode: push saturation/region changes in place.
        switch (current_) {
            case Mode::FullscreenEffect:
                FullscreenEffect::Set(state_.saturation);
                break;
            case Mode::Overlay:
                overlay_.UpdateSaturation(state_.saturation);
                overlay_.UpdateRegions(state_.regions);
                break;
            case Mode::Off:
                break;
        }
    } else {
        switch (desired) {
            case Mode::Off:
                // Reveal color: fully tear down both surfaces (both idempotent).
                FullscreenEffect::Clear();
                overlay_.Hide();
                break;

            case Mode::FullscreenEffect:
                // Fullscreen gray covers everything, so the overlay (if any) can
                // drop immediately without a color flash.
                FullscreenEffect::Set(state_.saturation);
                overlay_.Hide();
                break;

            case Mode::Overlay:
                // Bring the overlay up first. If a fullscreen effect is active
                // (A->B), it keeps the screen gray until the overlay has painted;
                // defer clearing it one message pass to stay flash-free.
                overlay_.Show(state_.saturation, state_.regions);
                if (current_ == Mode::FullscreenEffect) {
                    pendingClearFullscreen_ = true;
                    PostMessageW(mainWnd_, WM_APP_FINISH_TRANSITION, 0, 0);
                }
                break;
        }
    }

    current_ = desired;
    Tray::SyncState(*this);
    settings_.SaveState(state_);
}

void App::FinishTransition() {
    if (!pendingClearFullscreen_) return;
    pendingClearFullscreen_ = false;
    // Only clear if we actually settled in Mode B; a rapid follow-up transition
    // may already own the fullscreen effect (see Off/A paths, which manage it).
    if (current_ == Mode::Overlay)
        FullscreenEffect::Clear();
}

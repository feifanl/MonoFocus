#pragma once
#include <windows.h>
#include <functional>
#include <vector>

// Modal drag-to-select overlay (PLAN §5 region_select). A fullscreen layered
// window with a subtle dark veil and crosshair cursor; each left-drag commits
// one rectangle (>= kMinRegionPx square) via onRect and stays active so several
// can be drawn per session. Esc/Enter/right-click exits.
//
// Deviation from PLAN §5 signature: Begin also takes the current region list so
// pre-existing regions can be outlined during selection (SPEC §2.2).

namespace RegionSelect {

void Begin(HWND owner, const std::vector<RECT>& existing,
           std::function<void(RECT)> onRect);
bool IsActive();
HWND Hwnd();

}  // namespace RegionSelect

#pragma once
#include <windows.h>
#include <string>

// System tray icon, context menu, and balloon notifications (PLAN §5 tray).
//
// Commit 2 scope: Add/ReAdd/Remove, runtime-drawn on/off icons, the full menu
// skeleton (most items stubbed, Quit live), and balloons. Menu command routing
// happens via WM_COMMAND to the owner window. The App-aware SyncState and
// state-driven menu checkmarks arrive in commit 3 when App exists.

namespace Tray {

// Adds the tray icon; remembers the owner HWND for ReAdd/menu routing.
void Add(HWND hwnd);

// Re-adds the icon after an Explorer restart (TaskbarCreated broadcast).
void ReAdd();

// Removes the tray icon and frees cached icon handles. Idempotent.
void Remove();

// Handles the WM_TRAY callback: left-click toggles, right-click shows the menu.
void OnCallback(HWND hwnd, LPARAM lParam);

// Shows a balloon notification (NIM_MODIFY + NIF_INFO).
void Balloon(const std::wstring& title, const std::wstring& text);

}  // namespace Tray

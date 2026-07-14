#pragma once
#include <windows.h>
#include <string>

class App;

// System tray icon, context menu, and balloon notifications (PLAN §5 tray).

namespace Tray {

// Adds the tray icon; remembers the owner HWND for ReAdd/menu routing.
void Add(HWND hwnd);

// Re-adds the icon after an Explorer restart (TaskbarCreated broadcast).
void ReAdd();

// Removes the tray icon and frees cached icon handles. Idempotent.
void Remove();

// Swaps the icon variant + tooltip to match the current App state.
void SyncState(const App& app);

// Handles the WM_TRAY callback: left-click toggles, right-click shows the menu.
void OnCallback(HWND hwnd, LPARAM lParam, App& app);

// Shows a balloon notification (NIM_MODIFY + NIF_INFO).
void Balloon(const std::wstring& title, const std::wstring& text);

}  // namespace Tray

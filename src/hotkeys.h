#pragma once
#include <windows.h>

class App;

// Global hotkeys via RegisterHotKey on the hidden message window (PLAN §5).
// Commit 3 hardcodes the defaults and registers Ctrl+Alt+M only; the INI-driven
// parse grammar and the rest of the bindings arrive in later commits.

namespace Hotkeys {

void RegisterAll(HWND hwnd);
void UnregisterAll(HWND hwnd);
void Dispatch(App& app, WPARAM hotkeyId);

}  // namespace Hotkeys

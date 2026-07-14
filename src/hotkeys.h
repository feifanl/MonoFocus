#pragma once
#include <windows.h>
#include <string>

class App;
struct Settings;

// Global hotkeys via RegisterHotKey on the hidden message window (PLAN §5).
// Bindings come from Settings (INI-driven); specs are parsed by the grammar in
// Parse(). All registrations use MOD_NOREPEAT.

namespace Hotkeys {

// Parses "Ctrl+Alt+M" -> mods|vk. Case-insensitive, '+'-separated: >=1 modifier
// (Ctrl|Alt|Shift|Win) then exactly one key. Returns false on anything invalid.
bool Parse(const std::wstring& spec, UINT& mods, UINT& vk);

void RegisterAll(HWND hwnd, const Settings& s);   // balloon per failed combo
void UnregisterAll(HWND hwnd);
void Dispatch(App& app, WPARAM hotkeyId);

}  // namespace Hotkeys

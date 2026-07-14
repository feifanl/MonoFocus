#pragma once
#include <windows.h>
#include <map>
#include <string>
#include <vector>

#include "constants.h"   // HotkeyId

struct AppState;         // app.h

// Settings store: %APPDATA%\MonoFocus\settings.ini via the private-profile API,
// plus the HKCU Run key for autostart (registry is the source of truth; the INI
// mirrors it). Load validates every field: bad float -> default, bad rect ->
// dropped, bad hotkey spec -> default; malformed fields are collected in
// loadWarnings for a single tray balloon (PLAN §4, §5).

struct Settings {
    float saturation   = 0.0f;
    bool  restoreState  = true;
    bool  autostart     = false;
    std::map<HotkeyId, std::wstring> hotkeySpecs;   // defaults per §1
    bool  lastMono      = false;
    std::vector<RECT> lastRegions;

    std::vector<std::wstring> loadWarnings;         // malformed fields this load

    static Settings Load();                          // creates default file if absent
    void SaveState(const AppState& state);           // [state] + [general] saturation
    void SaveAll() const;                            // whole file
    static std::wstring Path();                      // %APPDATA%\MonoFocus\settings.ini
    void SetAutostart(bool on);                      // HKCU Run write/delete + field
};

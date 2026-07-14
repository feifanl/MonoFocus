#pragma once
#include <windows.h>

// Every ID, class name, message, and tunable constant lives here (PLAN §4).

constexpr wchar_t kMutexName[]     = L"Local\\MonoFocusSingleInstance";
constexpr wchar_t kMainClass[]     = L"MonoFocusMain";      // hidden message window
constexpr wchar_t kHostClass[]     = L"MonoFocusOverlayHost";
constexpr wchar_t kSelectClass[]   = L"MonoFocusRegionSelect";
constexpr wchar_t kAppName[]       = L"MonoFocus";          // tray tip, %APPDATA% dir, Run value

constexpr UINT WM_TRAY             = WM_APP + 1;            // tray callback
constexpr UINT kTimerRefresh       = 1;                     // overlay, 16 ms

enum HotkeyId { HK_TOGGLE = 1, HK_SELECT, HK_CLEAR, HK_SAT_UP, HK_SAT_DOWN };

enum MenuId {                                               // tray menu commands
  IDM_TOGGLE = 100, IDM_SELECT, IDM_CLEAR,
  IDM_SAT_0, IDM_SAT_25, IDM_SAT_50, IDM_SAT_75,
  IDM_RESTORE_STATE, IDM_AUTOSTART, IDM_OPEN_SETTINGS, IDM_RELOAD_SETTINGS,
  IDM_QUIT,
};

constexpr int   kMinRegionPx   = 8;      // drags smaller than 8x8 ignored
constexpr float kSatStep       = 0.1f;   // hotkey step

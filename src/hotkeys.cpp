#include "hotkeys.h"

#include "app.h"
#include "constants.h"

namespace Hotkeys {

void RegisterAll(HWND hwnd) {
    // Hardcoded defaults (INI-driven config arrives in commit 8).
    // MOD_NOREPEAT so held keys don't autofire.
    const UINT ca = MOD_CONTROL | MOD_ALT | MOD_NOREPEAT;
    RegisterHotKey(hwnd, HK_TOGGLE,   ca, 'M');
    RegisterHotKey(hwnd, HK_SAT_UP,   ca, VK_UP);
    RegisterHotKey(hwnd, HK_SAT_DOWN, ca, VK_DOWN);
}

void UnregisterAll(HWND hwnd) {
    UnregisterHotKey(hwnd, HK_TOGGLE);
    UnregisterHotKey(hwnd, HK_SAT_UP);
    UnregisterHotKey(hwnd, HK_SAT_DOWN);
}

void Dispatch(App& app, WPARAM hotkeyId) {
    switch (hotkeyId) {
        case HK_TOGGLE:
            app.Toggle();
            break;
        case HK_SAT_UP:
            app.StepSaturation(+1);
            break;
        case HK_SAT_DOWN:
            app.StepSaturation(-1);
            break;
        default:
            break;
    }
}

}  // namespace Hotkeys

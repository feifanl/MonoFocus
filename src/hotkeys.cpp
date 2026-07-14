#include "hotkeys.h"

#include "app.h"
#include "constants.h"

namespace Hotkeys {

void RegisterAll(HWND hwnd) {
    // Commit 3: toggle only. MOD_NOREPEAT so held keys don't autofire.
    RegisterHotKey(hwnd, HK_TOGGLE, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 'M');
}

void UnregisterAll(HWND hwnd) {
    UnregisterHotKey(hwnd, HK_TOGGLE);
}

void Dispatch(App& app, WPARAM hotkeyId) {
    switch (hotkeyId) {
        case HK_TOGGLE:
            app.Toggle();
            break;
        default:
            break;
    }
}

}  // namespace Hotkeys

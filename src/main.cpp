// MonoFocus - main.cpp
// Process lifecycle + message routing (PLAN §5).
//
// Commit 1 scope: single-instance mutex, MagInitialize, a hidden top-level
// message window, and the message loop. Tray, hotkeys, and the App state
// machine are wired in later commits.
//
// Note on the window: PLAN §5 mentions an HWND_MESSAGE parent, but later
// commits depend on broadcast messages (the registered "TaskbarCreated"
// message, WM_DISPLAYCHANGE, WM_POWERBROADCAST) which message-only windows
// never receive. We therefore use a hidden *top-level* window (created but
// never shown). It is invisible all the same and does get the broadcasts.

#include <windows.h>
#include <magnification.h>

#include "constants.h"

namespace {

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;
        default:
            return DefWindowProcW(hwnd, msg, wParam, lParam);
    }
}

// Registers kMainClass and creates the hidden top-level message window.
HWND CreateMessageWindow(HINSTANCE hInst) {
    WNDCLASSEXW wc = { sizeof(wc) };
    wc.lpfnWndProc   = MainWndProc;
    wc.hInstance     = hInst;
    wc.lpszClassName = kMainClass;
    if (!RegisterClassExW(&wc))
        return nullptr;

    // WS_POPUP, never shown -> invisible, but still a top-level window so it
    // receives display/power/taskbar broadcasts in later commits.
    return CreateWindowExW(
        0, kMainClass, kAppName, WS_POPUP,
        0, 0, 0, 0, nullptr, nullptr, hInst, nullptr);
}

}  // namespace

int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE, PWSTR, int) {
    // 1. Single instance: second launch exits silently.
    HANDLE mutex = CreateMutexW(nullptr, TRUE, kMutexName);
    if (mutex && GetLastError() == ERROR_ALREADY_EXISTS) {
        CloseHandle(mutex);
        return 0;
    }

    // 2. Magnification runtime. The only fatal error in the app.
    if (!MagInitialize()) {
        MessageBoxW(nullptr, L"Failed to initialize the Magnification API.",
                    kAppName, MB_ICONERROR | MB_OK);
        if (mutex) CloseHandle(mutex);
        return 1;
    }

    // 3. Hidden top-level message window.
    HWND hwnd = CreateMessageWindow(hInstance);
    if (!hwnd) {
        MagUninitialize();
        if (mutex) CloseHandle(mutex);
        return 1;
    }

    // 4. Message loop.
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    // 5. Teardown.
    MagUninitialize();
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return 0;
}

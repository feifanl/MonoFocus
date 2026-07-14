// MonoFocus - main.cpp
// Process lifecycle + message routing (PLAN §5).
//
// Note on the window: PLAN §5 mentions an HWND_MESSAGE parent, but later
// commits depend on broadcast messages (the registered "TaskbarCreated"
// message, WM_DISPLAYCHANGE, WM_POWERBROADCAST) which message-only windows
// never receive. We therefore use a hidden *top-level* window (created but
// never shown). It is invisible all the same and does get the broadcasts.

#include <windows.h>
#include <commctrl.h>
#include <magnification.h>
#include <shellapi.h>
#include <wtsapi32.h>
#include <string>

#include "app.h"
#include "constants.h"
#include "hotkeys.h"
#include "settings.h"
#include "tray.h"

namespace {

// Registered broadcast sent by Explorer when the taskbar is (re)created.
UINT g_taskbarCreatedMsg = 0;

// The single App instance (owned by wWinMain; alive for the whole message loop).
App*      g_app      = nullptr;
Settings* g_settings = nullptr;

// Shows one balloon summarizing any fields that were reset during load.
void ShowLoadWarnings(const Settings& s) {
    if (s.loadWarnings.empty()) return;
    std::wstring msg = L"Some settings were invalid and reset to defaults:\n";
    for (const auto& w : s.loadWarnings)
        msg += L"\x2022 " + w + L"\n";
    Tray::Balloon(kAppName, msg);
}

LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (msg == g_taskbarCreatedMsg && g_taskbarCreatedMsg != 0) {
        Tray::ReAdd();
        if (g_app) Tray::SyncState(*g_app);
        return 0;
    }

    switch (msg) {
        case WM_HOTKEY:
            if (g_app) Hotkeys::Dispatch(*g_app, wParam);
            return 0;

        case WM_APP_FINISH_TRANSITION:
            if (g_app) g_app->FinishTransition();
            return 0;

        case WM_DISPLAYCHANGE:
        case WM_DPICHANGED:
            if (g_app) g_app->OnEnvironmentChange();
            return 0;

        case WM_POWERBROADCAST:
            if (g_app && wParam == PBT_APMRESUMEAUTOMATIC)
                g_app->OnEnvironmentChange();
            return TRUE;

        case WM_WTSSESSION_CHANGE:
            if (g_app && wParam == WTS_SESSION_UNLOCK)
                g_app->OnEnvironmentChange();
            return 0;

        case WM_TRAY:
            if (g_app) Tray::OnCallback(hwnd, lParam, *g_app);
            return 0;

        case WM_COMMAND:
            switch (LOWORD(wParam)) {
                case IDM_TOGGLE:
                    if (g_app) g_app->Toggle();
                    return 0;
                case IDM_SELECT:
                    if (g_app) g_app->BeginRegionSelect();
                    return 0;
                case IDM_CLEAR:
                    if (g_app) g_app->ClearRegions();
                    return 0;
                case IDM_SAT_0:
                    if (g_app) g_app->SetSaturation(0.00f);
                    return 0;
                case IDM_SAT_25:
                    if (g_app) g_app->SetSaturation(0.25f);
                    return 0;
                case IDM_SAT_50:
                    if (g_app) g_app->SetSaturation(0.50f);
                    return 0;
                case IDM_SAT_75:
                    if (g_app) g_app->SetSaturation(0.75f);
                    return 0;
                case IDM_RESTORE_STATE:
                    if (g_settings) {
                        g_settings->restoreState = !g_settings->restoreState;
                        WritePrivateProfileStringW(
                            L"general", L"restore_state",
                            g_settings->restoreState ? L"1" : L"0",
                            Settings::Path().c_str());
                    }
                    return 0;
                case IDM_AUTOSTART:
                    if (g_settings) g_settings->SetAutostart(!g_settings->autostart);
                    return 0;
                case IDM_OPEN_SETTINGS:
                    ShellExecuteW(nullptr, L"open", Settings::Path().c_str(),
                                  nullptr, nullptr, SW_SHOWNORMAL);
                    return 0;
                case IDM_RELOAD_SETTINGS:
                    if (g_settings && g_app) {
                        Hotkeys::UnregisterAll(hwnd);
                        *g_settings = Settings::Load();
                        Hotkeys::RegisterAll(hwnd, *g_settings);
                        g_app->OnSettingsReloaded();
                        ShowLoadWarnings(*g_settings);
                        Tray::SyncState(*g_app);
                    }
                    return 0;
                case IDM_QUIT:
                    DestroyWindow(hwnd);
                    return 0;
                default:
                    return 0;
            }

        case WM_DESTROY:
            Tray::Remove();
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

    // comctl32 v6 for themed menus (manifest declares the dependency).
    INITCOMMONCONTROLSEX icc = { sizeof(icc), ICC_STANDARD_CLASSES };
    InitCommonControlsEx(&icc);

    g_taskbarCreatedMsg = RegisterWindowMessageW(L"TaskbarCreated");

    // 3. Hidden top-level message window.
    HWND hwnd = CreateMessageWindow(hInstance);
    if (!hwnd) {
        MagUninitialize();
        if (mutex) CloseHandle(mutex);
        return 1;
    }
    // Session lock/unlock notifications (defensive re-apply on unlock).
    WTSRegisterSessionNotification(hwnd, NOTIFY_FOR_THIS_SESSION);

    // 4. Settings, App state machine, tray icon, hotkeys.
    Settings settings = Settings::Load();
    g_settings = &settings;
    App app(hwnd, settings);
    g_app = &app;
    Tray::Add(hwnd);                 // before RegisterAll so failure balloons show
    Tray::SyncState(app);
    Hotkeys::RegisterAll(hwnd, settings);
    ShowLoadWarnings(settings);
    if (settings.restoreState)
        app.RestoreFromSettings();

    // 5. Message loop.
    MSG m;
    while (GetMessageW(&m, nullptr, 0, 0) > 0) {
        TranslateMessage(&m);
        DispatchMessageW(&m);
    }

    // 6. Teardown (Tray::Remove already ran in WM_DESTROY; idempotent).
    g_app = nullptr;
    g_settings = nullptr;
    app.Shutdown();
    Hotkeys::UnregisterAll(hwnd);
    WTSUnRegisterSessionNotification(hwnd);
    Tray::Remove();
    MagUninitialize();
    if (mutex) {
        ReleaseMutex(mutex);
        CloseHandle(mutex);
    }
    return 0;
}

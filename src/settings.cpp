#include "settings.h"

#include <shlobj.h>
#include <shlwapi.h>
#include <algorithm>
#include <cwchar>

#include "app.h"        // AppState
#include "hotkeys.h"    // Hotkeys::Parse for validation

namespace {

constexpr wchar_t kRunKey[] =
    L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";

// Ordered table of the five bindings: id, INI key, default spec.
struct HotkeyDef { HotkeyId id; const wchar_t* key; const wchar_t* def; };
constexpr HotkeyDef kHotkeyDefs[] = {
    { HK_TOGGLE,   L"toggle",         L"Ctrl+Alt+M"    },
    { HK_SELECT,   L"select_region",  L"Ctrl+Alt+R"    },
    { HK_CLEAR,    L"clear_regions",  L"Ctrl+Alt+X"    },
    { HK_SAT_UP,   L"saturation_up",  L"Ctrl+Alt+Up"   },
    { HK_SAT_DOWN, L"saturation_down",L"Ctrl+Alt+Down" },
};

std::wstring ReadStr(const wchar_t* section, const wchar_t* key,
                     const wchar_t* def, const std::wstring& path) {
    wchar_t buf[512];
    GetPrivateProfileStringW(section, key, def, buf,
                             static_cast<DWORD>(std::size(buf)), path.c_str());
    return buf;
}

bool AutostartInRegistry() {
    HKEY k;
    if (RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, KEY_QUERY_VALUE, &k) != ERROR_SUCCESS)
        return false;
    LONG q = RegQueryValueExW(k, kAppName, nullptr, nullptr, nullptr, nullptr);
    RegCloseKey(k);
    return q == ERROR_SUCCESS;
}

}  // namespace

std::wstring Settings::Path() {
    PWSTR appdata = nullptr;
    std::wstring dir;
    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &appdata))) {
        dir = appdata;
        CoTaskMemFree(appdata);
    }
    dir += L"\\";
    dir += kAppName;
    CreateDirectoryW(dir.c_str(), nullptr);
    return dir + L"\\settings.ini";
}

Settings Settings::Load() {
    Settings s;
    // Seed defaults for all five bindings first.
    for (const auto& d : kHotkeyDefs)
        s.hotkeySpecs[d.id] = d.def;

    const std::wstring path = Path();

    if (!PathFileExistsW(path.c_str())) {
        s.SaveAll();                              // materialize a default file
        s.autostart = AutostartInRegistry();
        return s;
    }

    // [general] saturation (float 0-1, clamp; malformed -> default + warning).
    {
        std::wstring raw = ReadStr(L"general", L"saturation", L"0.0", path);
        wchar_t* end = nullptr;
        double v = std::wcstod(raw.c_str(), &end);
        if (end == raw.c_str() || *end != L'\0') {
            s.loadWarnings.push_back(L"saturation (using default)");
        } else {
            s.saturation = static_cast<float>(std::clamp(v, 0.0, 1.0));
        }
    }

    s.restoreState = GetPrivateProfileIntW(L"general", L"restore_state", 1, path.c_str()) != 0;

    // Autostart: registry is source of truth; INI value is only a mirror.
    s.autostart = AutostartInRegistry();

    // [hotkeys] each spec validated via the parse grammar.
    for (const auto& d : kHotkeyDefs) {
        std::wstring spec = ReadStr(L"hotkeys", d.key, d.def, path);
        UINT mods = 0, vk = 0;
        if (Hotkeys::Parse(spec, mods, vk)) {
            s.hotkeySpecs[d.id] = spec;
        } else {
            s.hotkeySpecs[d.id] = d.def;
            s.loadWarnings.push_back(std::wstring(d.key) + L" (invalid hotkey, using default)");
        }
    }

    // [state] mono + regions.
    s.lastMono = GetPrivateProfileIntW(L"state", L"mono", 0, path.c_str()) != 0;
    int count = GetPrivateProfileIntW(L"state", L"region_count", 0, path.c_str());
    for (int i = 0; i < count; ++i) {
        wchar_t key[32];
        swprintf_s(key, L"region%d", i);
        std::wstring raw = ReadStr(L"state", key, L"", path);
        RECT r{};
        if (swscanf_s(raw.c_str(), L"%d,%d,%d,%d", &r.left, &r.top, &r.right, &r.bottom) == 4
            && r.right > r.left && r.bottom > r.top) {
            s.lastRegions.push_back(r);
        }
        // bad rect -> dropped (PLAN §4)
    }

    return s;
}

void Settings::SaveState(const AppState& state) {
    const std::wstring path = Path();

    wchar_t sat[32];
    swprintf_s(sat, L"%.4g", state.saturation);
    WritePrivateProfileStringW(L"general", L"saturation", sat, path.c_str());

    WritePrivateProfileStringW(L"state", L"mono", state.mono ? L"1" : L"0", path.c_str());

    // Clear any stale regionN keys from a previous larger set.
    int oldCount = GetPrivateProfileIntW(L"state", L"region_count", 0, path.c_str());
    int newCount = static_cast<int>(state.regions.size());
    for (int i = newCount; i < oldCount; ++i) {
        wchar_t key[32];
        swprintf_s(key, L"region%d", i);
        WritePrivateProfileStringW(L"state", key, nullptr, path.c_str());   // delete
    }

    wchar_t cnt[16];
    swprintf_s(cnt, L"%d", newCount);
    WritePrivateProfileStringW(L"state", L"region_count", cnt, path.c_str());

    for (int i = 0; i < newCount; ++i) {
        const RECT& r = state.regions[i];
        wchar_t key[32], val[64];
        swprintf_s(key, L"region%d", i);
        swprintf_s(val, L"%d,%d,%d,%d", r.left, r.top, r.right, r.bottom);
        WritePrivateProfileStringW(L"state", key, val, path.c_str());
    }
}

void Settings::SaveAll() const {
    const std::wstring path = Path();

    wchar_t sat[32];
    swprintf_s(sat, L"%.4g", saturation);
    WritePrivateProfileStringW(L"general", L"saturation", sat, path.c_str());
    WritePrivateProfileStringW(L"general", L"restore_state", restoreState ? L"1" : L"0", path.c_str());
    WritePrivateProfileStringW(L"general", L"autostart", autostart ? L"1" : L"0", path.c_str());

    for (const auto& d : kHotkeyDefs) {
        auto it = hotkeySpecs.find(d.id);
        const std::wstring& spec = (it != hotkeySpecs.end()) ? it->second : std::wstring(d.def);
        WritePrivateProfileStringW(L"hotkeys", d.key, spec.c_str(), path.c_str());
    }

    WritePrivateProfileStringW(L"state", L"mono", lastMono ? L"1" : L"0", path.c_str());
    wchar_t cnt[16];
    swprintf_s(cnt, L"%d", static_cast<int>(lastRegions.size()));
    WritePrivateProfileStringW(L"state", L"region_count", cnt, path.c_str());
    for (size_t i = 0; i < lastRegions.size(); ++i) {
        const RECT& r = lastRegions[i];
        wchar_t key[32], val[64];
        swprintf_s(key, L"region%zu", i);
        swprintf_s(val, L"%d,%d,%d,%d", r.left, r.top, r.right, r.bottom);
        WritePrivateProfileStringW(L"state", key, val, path.c_str());
    }
}

void Settings::SetAutostart(bool on) {
    HKEY k;
    if (RegCreateKeyExW(HKEY_CURRENT_USER, kRunKey, 0, nullptr, 0,
                        KEY_SET_VALUE, nullptr, &k, nullptr) == ERROR_SUCCESS) {
        if (on) {
            wchar_t exe[MAX_PATH];
            GetModuleFileNameW(nullptr, exe, MAX_PATH);
            std::wstring quoted = L"\"" + std::wstring(exe) + L"\"";
            RegSetValueExW(k, kAppName, 0, REG_SZ,
                           reinterpret_cast<const BYTE*>(quoted.c_str()),
                           static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t)));
        } else {
            RegDeleteValueW(k, kAppName);
        }
        RegCloseKey(k);
    }
    autostart = on;
    WritePrivateProfileStringW(L"general", L"autostart", on ? L"1" : L"0", Path().c_str());
}

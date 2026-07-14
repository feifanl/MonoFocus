#include "hotkeys.h"

#include <algorithm>
#include <cwctype>
#include <vector>

#include "app.h"
#include "constants.h"
#include "settings.h"
#include "tray.h"

namespace Hotkeys {
namespace {

std::wstring Lower(std::wstring s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](wchar_t c) { return static_cast<wchar_t>(std::towlower(c)); });
    return s;
}

std::wstring Trim(const std::wstring& s) {
    size_t a = s.find_first_not_of(L" \t");
    size_t b = s.find_last_not_of(L" \t");
    if (a == std::wstring::npos) return L"";
    return s.substr(a, b - a + 1);
}

// Maps a single key token (already lowercased) to a virtual-key code.
bool KeyToVk(const std::wstring& k, UINT& vk) {
    if (k.size() == 1) {
        wchar_t c = k[0];
        if (c >= L'a' && c <= L'z') { vk = static_cast<UINT>(L'A' + (c - L'a')); return true; }
        if (c >= L'0' && c <= L'9') { vk = static_cast<UINT>(c); return true; }
    }
    if (k.size() >= 2 && k[0] == L'f') {                 // F1..F24
        wchar_t* end = nullptr;
        long n = std::wcstol(k.c_str() + 1, &end, 10);
        if (end && *end == L'\0' && n >= 1 && n <= 24) { vk = VK_F1 + (n - 1); return true; }
    }
    struct Named { const wchar_t* name; UINT vk; };
    static const Named named[] = {
        { L"up", VK_UP }, { L"down", VK_DOWN }, { L"left", VK_LEFT }, { L"right", VK_RIGHT },
        { L"space", VK_SPACE }, { L"tab", VK_TAB }, { L"home", VK_HOME }, { L"end", VK_END },
        { L"pgup", VK_PRIOR }, { L"pgdn", VK_NEXT }, { L"ins", VK_INSERT }, { L"del", VK_DELETE },
    };
    for (const auto& n : named)
        if (k == n.name) { vk = n.vk; return true; }
    return false;
}

const wchar_t* ActionName(HotkeyId id) {
    switch (id) {
        case HK_TOGGLE:   return L"Toggle monochrome";
        case HK_SELECT:   return L"Select region";
        case HK_CLEAR:    return L"Clear regions";
        case HK_SAT_UP:   return L"Saturation up";
        case HK_SAT_DOWN: return L"Saturation down";
    }
    return L"Hotkey";
}

}  // namespace

bool Parse(const std::wstring& spec, UINT& mods, UINT& vk) {
    mods = 0;
    vk = 0;
    bool haveKey = false;

    size_t start = 0;
    while (start <= spec.size()) {
        size_t plus = spec.find(L'+', start);
        std::wstring tok = Lower(Trim(spec.substr(start,
            plus == std::wstring::npos ? std::wstring::npos : plus - start)));
        if (!tok.empty()) {
            if (tok == L"ctrl" || tok == L"control")   mods |= MOD_CONTROL;
            else if (tok == L"alt")                    mods |= MOD_ALT;
            else if (tok == L"shift")                  mods |= MOD_SHIFT;
            else if (tok == L"win")                    mods |= MOD_WIN;
            else {
                UINT k = 0;
                if (haveKey || !KeyToVk(tok, k)) return false;   // 2nd key or bad key
                vk = k;
                haveKey = true;
            }
        }
        if (plus == std::wstring::npos) break;
        start = plus + 1;
    }

    return mods != 0 && haveKey;                          // >=1 modifier + exactly one key
}

void RegisterAll(HWND hwnd, const Settings& s) {
    for (const auto& [id, spec] : s.hotkeySpecs) {
        UINT mods = 0, vk = 0;
        if (!Parse(spec, mods, vk)) continue;            // should not happen (validated on load)
        if (!RegisterHotKey(hwnd, id, mods | MOD_NOREPEAT, vk)) {
            Tray::Balloon(kAppName,
                std::wstring(ActionName(id)) + L" hotkey (" + spec + L") is unavailable.");
        }
    }
}

void UnregisterAll(HWND hwnd) {
    UnregisterHotKey(hwnd, HK_TOGGLE);
    UnregisterHotKey(hwnd, HK_SELECT);
    UnregisterHotKey(hwnd, HK_CLEAR);
    UnregisterHotKey(hwnd, HK_SAT_UP);
    UnregisterHotKey(hwnd, HK_SAT_DOWN);
}

void Dispatch(App& app, WPARAM hotkeyId) {
    switch (hotkeyId) {
        case HK_TOGGLE:   app.Toggle();            break;
        case HK_SELECT:   app.BeginRegionSelect(); break;
        case HK_CLEAR:    app.ClearRegions();      break;
        case HK_SAT_UP:   app.StepSaturation(+1);  break;
        case HK_SAT_DOWN: app.StepSaturation(-1);  break;
        default: break;
    }
}

}  // namespace Hotkeys

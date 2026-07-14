# MonoFocus

A lightweight Windows 11 tray utility that desaturates/turns monochrome the entire screen
and lets you carve out rectangular regions that stay in color.

Simple -- five hotkeys, three features.

- **Global monochrome toggle** - one hotkey desaturates every monitor (all apps, the
  taskbar, everything), regardless of which window has focus.
- **Adjustable saturation** - from 0% (full grayscale, the default) to 100% (no effect),
  in 10% steps or via tray presets.
- **True-color regions** - while desaturated, create rectangles that remain in-color.
- **No install, no admin, no dependencies** - a single ~200 KB `monofocus.exe`.

## Contents

- [How it works](#how-it-works)
- [Default hotkeys](#default-hotkeys) - [Region select mode](#region-select-mode)
- [Tray menu](#tray-menu)
- [Getting started](#getting-started) - [Build from source](#build-from-source)
- [Install / uninstall](#install--uninstall)
- [Settings](#settings)
- [Known limitations](#known-limitations)
- [Troubleshooting](#troubleshooting)

## How it works

Two rendering modes, both built on the Windows Magnification API:

- **Mode A (no regions)** - `MagSetFullscreenColorEffect` applies a GPU color matrix to
  the final output of all monitors. This is the same mechanism as Windows' built-in
  color filters, so it's instant and zero latency.
- **Mode B (regions)** - a host window spanning the virtual screen hosts a magnifier control
  that re-renders the desktop desaturated, with each region **as a hole** (`SetWindowRgn`).
  Holes expose the actual desktop below.

## Default hotkeys

| Action | Default |
|---|---|
| Toggle monochrome on/off | `Ctrl+Alt+M` |
| Enter region select mode | `Ctrl+Alt+R` |
| Clear all regions | `Ctrl+Alt+X` |
| Saturation up (+10%) | `Ctrl+Alt+Up` |
| Saturation down (-10%) | `Ctrl+Alt+Down` |

All are configurable -- see [Settings](#settings).

### Region select mode

Press `Ctrl+Alt+R` (or tray -> *Select region…*). If monochrome is off, it turns on first.
Drag to draw a rectangle region; releasing saves it and it lights up in color immediately.
Draw as many as you like. Drags smaller than 8x8 px are ignored.

Press `Esc`, `Enter`, or right-click to finish.

## Tray menu

Left-click the tray icon to toggle monochrome. Right-click for the full menu: toggle,
select region, clear regions, saturation presets (0/25/50/75%), restore state at launch,
start with Windows, open/reload the settings file, and quit.

The icon is green when off (normal color) and gray when on (monochrome active).

## Getting started

**Already have `monofocus.exe`?** Copy it anywhere, double-click, and press
`Ctrl+Alt+M`. That's it. To launch it automatically at login, right-click the
tray icon and check **Start with Windows**.

### Build from source

You need **CMake ≥ 3.21** and the **Visual Studio 2022 Build Tools** (MSVC x64).

Both install with no admin:

1. **Install the toolchain** (one time):

   ```powershell
   winget install Kitware.CMake Microsoft.VisualStudio.2022.BuildTools
   ```

2. **Open a new PowerShell window.**
3. **Get the code:**

   ```powershell
   git clone https://github.com/feifanl/MonoFocus monofocus
   cd monofocus
   ```

4. **Configure and build (Release):**

   ```powershell
   cmake -B build -G "Visual Studio 17 2022" -A x64
   cmake --build build --config Release
   ```

5. **Run it:**

   ```powershell
   .\build\Release\monofocus.exe
   ```

The result is `build\Release\monofocus.exe` (~220 KB). Copy it anywhere - there is no install.

**Build not working?**

- `cmake : ... not recognized` - open a **new** terminal (step 2), or sign out and back in.
- `No CMAKE_CXX_COMPILER could be found` - the C++ workload is missing. Rerun the Build
  Tools installer and add the **Desktop development with C++** workload.
- `Could not find ... Visual Studio 17 2022` - you have a different VS version; change the
  `-G` value to match (e.g. `"Visual Studio 16 2019"`).

## Install / uninstall

There is no installer. Copy `monofocus.exe` anywhere and run it - it lives in the tray.
To start it with Windows, check **Start with Windows** in the tray menu.

To uninstall: quit, delete the exe, uncheck **Start with Windows** (or delete the
`MonoFocus` value under `HKCU\Software\Microsoft\Windows\CurrentVersion\Run`), and delete
`%APPDATA%\MonoFocus`.

## Settings

Settings live in `%APPDATA%\MonoFocus\settings.ini` (tray -> *Open settings file*). Edit it
and choose *Reload settings* to apply changes without restarting.

```ini
[general]
saturation=0.0         ; 0.0 = grayscale, 1.0 = no effect
restore_state=1        ; restore mono + regions at launch
autostart=0            ; mirrors the HKCU Run value (registry is source of truth)

[hotkeys]
toggle=Ctrl+Alt+M
select_region=Ctrl+Alt+R
clear_regions=Ctrl+Alt+X
saturation_up=Ctrl+Alt+Up
saturation_down=Ctrl+Alt+Down

[state]                ; written automatically; virtual-screen physical pixels
mono=0
region_count=0
```

**Hotkey grammar** (case-insensitive, `+`-separated): one or more modifiers
(`Ctrl`, `Alt`, `Shift`, `Win`) followed by exactly one key - a letter `A–Z`, digit
`0–9`, `F1`–`F24`, `Up`/`Down`/`Left`/`Right`, or
`Space`/`Tab`/`Home`/`End`/`PgUp`/`PgDn`/`Ins`/`Del`. Invalid entries fall back to the
default and are noted in a tray balloon on load.

## Known limitations

- **Secure desktop** (UAC prompts, Ctrl+Alt+Del, lock screen) is never affected - Windows
  composits it separately. Color returns there and the monochrome effect resumes afterward.
- **Exclusive-fullscreen games** bypass DWM composition; neither mode applies. Borderless-
  windowed content is fine.
- **DRM-protected video** may render black inside the Mode B gray area. Inside a color
  region, it shows fine, since it's the real desktop being exposed.
- In Mode B, system surfaces above topmost (Start menu, Alt-Tab, notification toasts)
  appear in full color over the gray overlay. Mode A desaturates them correctly. Cosmetic.
- The Mode B gray area refreshes on a ~60 Hz timer and can trail live content by a frame.
  Color regions have no such lag (they are the real desktop).
- Only one program can own the fullscreen color effect. Running Windows Magnifier or
  another color-filter app at the same time conflicts (last writer wins).

## Troubleshooting

- **A hotkey doesn't work / a balloon says a binding is unavailable.** Another app already
  owns that combination. Rebind it in `settings.ini` under `[hotkeys]` and choose *Reload
  settings*.
- **The screen won't desaturate, or the effect flickers with another app.** Windows
  Magnifier or another color-filter tool is contending for the fullscreen color effect.
  Close the other tool; the effect is re-applied defensively on resume and unlock.
- **The screen is stuck gray after a crash.** It won't be - Windows resets magnification
  state when the owning process exits. If in doubt, launch and quit MonoFocus once.

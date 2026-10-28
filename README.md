# WT Armour Vision — research edition 0.4

Native C++20 Windows control centre, independent telemetry overlay and offline synthetic armour demonstration. This is an initial implementation, **not the complete production application described in the specification**. No verified War Thunder armour dataset, live weakspot analysis, damage prediction or vehicle recognition is included.

## Download

[Download WTArmourVision.exe](https://github.com/davyduao-hue/War-thundeer-visualiser/raw/refs/heads/main/downloads/WTArmourVision.exe) — standalone Windows x64 research build. Cross-compiled successfully; not yet tested on Windows or in game. No external data files are needed for demo startup.

## Build and run

Windows 10/11 x64, Visual Studio 2022 Desktop development with C++, Windows SDK and CMake 3.20+:

```powershell
cmake -S . -B build -A x64
cmake --build build --config Release --parallel 2
ctest --test-dir build -C Release --output-on-failure
cmake --install build --config Release --prefix package/WTArmourVision
Compress-Archive package/WTArmourVision/* WTArmourVision-Windows-x64.zip
```

Double-click the packaged `WTArmourVision.exe`. The standalone executable embeds its demo database and creates a local cache on first run; the optional ZIP includes the external `data` directory. The MSVC runtime is linked statically; Windows supplies DirectX and WinHTTP. Dependencies are vendored, so configuration needs no downloads. `.github/workflows/windows.yml` builds, tests and packages on Windows when pushed to GitHub; it also publishes the standalone executable to GitHub Releases after a successful main-branch build.

Linux core validation:

```sh
cmake -S . -B build
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

## Operation

Demo mode starts enabled and labels simulated telemetry. Enable the overlay on Dashboard; demo overlay follows the control window. Click **CONNECT TO GAME** to disable demo and enable the overlay. In Game Settings, refresh the window list and select the game if automatic detection fails. Telemetry polls port 8111 independently of window detection. Overlay supports a visible, focused `aces.exe` window in borderless mode. It hides on focus loss, minimisation or game exit. It is a separate layered, nonactivating, click-through Win32 window with GDI text; the control centre uses Dear ImGui and DirectX 11. Exclusive fullscreen is unsupported.

Alt+F8 toggles the overlay when hotkey registration succeeds. Minimise to tray from User Settings; double-click the tray icon to restore and right-click to exit. Settings persist under `%LOCALAPPDATA%/WTArmourVision/settings.json` every two seconds and at normal shutdown. Copy this file to back up settings; there is no GUI import/export yet.

Telemetry is read asynchronously from `/state`, `/indicators`, `/map_info.json` with bounded responses, timeouts and reconnect polling. Endpoint availability and fields in ground battles are **not verified**. A reported `type` is displayed as unverified raw information. Ammunition detection is not implemented: `MANUAL SHELL SELECTION REQUIRED` remains visible. No targets or precise armour maps are derived from live telemetry. Each endpoint is polled independently; one missing endpoint no longer prevents the others from working. Raw fields retain their provided labels/units. **Save compatibility report** writes `compatibility-report.json` under the application settings folder and opens that folder.

## Offline analysis

Select a synthetic plate and shell, adjust distance, and rotate the plate schematic. This is not a vehicle mesh or full 3D scene. The database is versioned JSON; search and reload operate locally. All imported profiles remain unverified. No public data is scraped or redistributed without verified permission.

The core interpolates penetration tables without extrapolation, computes line-of-sight thickness, and provides a conservative estimate for homogeneous plates. AP/APHE apply supplied normalization; HEAT does not; oblique APFSDS, HE, composite/spaced/reactive armour and missing data return uncertainty. This is an educational approximation, not a validated War Thunder ballistic simulator. APHE post-penetration effects and crew/module damage are not modelled. The 15% uncertainty band is an explicit engineering heuristic, not measured confidence. Demo profiles always classify as uncertain.

Legend: red #FF4545 unlikely; green #36E886 likely; yellow #FFD447 uncertain. Blue #39A9FF is reserved for validated disabling-hit estimates and is never currently assigned. No kill probability is claimed.

## Implemented versus outstanding

Implemented: seven control pages, actual overlay toggle/opacity/scale/FPS control, process/window discovery using limited query access, focus tracking, telemetry worker, manual shell/profile selection, local database reload/search, local display name/settings, tray/hotkey, synthetic plate viewer, portable calculation tests, Windows build/package workflow.

Outstanding: verified licensed vehicle data and geometry, online database updates, validated shell-specific ballistics and damage models, full vehicle 3D rendering, optional recorded-video CV/tracking, CPU profiling and RX 580 benchmarks, anchors/colour customization, avatars/localization, Windows autostart, notifications, GUI settings import/export, adaptive video throttling. Performance presets currently alter telemetry polling and render caps only. No game FPS impact has been measured.

## Validation status

Linux: 17 core checks and 19 telemetry parsing/recovery checks passed, including interpolation, classification thresholds, LOS, invalid angle/range, complex armour uncertainty, HE/APFSDS limitations, unverified data, database loading and settings replacement.

Windows launch, DX11 rendering, tray, transparency, click-through, resizing, DPI, shutdown and real game reconnection require Windows execution. Follow `docs/WINDOWS_CHECKLIST.md`. A Windows x64 executable was cross-compiled with LLVM-MinGW 20250613 (Clang 20.1.7) on Linux. It has not been executed on Windows or tested with War Thunder.

See `docs/RESEARCH.md` for research/access limitations and `ATTRIBUTION.md` for data and dependency licensing.

## Compatibility update 0.2

Fixed DirectX swap-chain resizing by unbinding the active render target before releasing it. Added manual game window selection, independent telemetry polling, endpoint-specific errors, bounded response reads, display-DPI overlay sizing, and a compatibility report. The Windows build workflow now runs `WTArmourVision.exe --smoke-test`, which uses software rendering to create the windows, render frames, resize, check overlay style flags, save settings and shut down. This automation does not verify visual transparency, input routing or actual War Thunder compatibility. Live validation on a Windows PC remains required.

## Bug-fix update 0.3

Corrupt settings no longer apply partially or get silently overwritten. Reset settings explicitly replaces a rejected file; otherwise the original is preserved. Windows settings replacement is atomic. Corrupt database loading now falls back to bundled synthetic profiles while retaining the source file. Unicode telemetry and names preserve complete characters, and report export failures remain in the UI instead of terminating the application. Duplicate launches restore the existing window. Missing tray support falls back to the taskbar, and Explorer restarts re-register the tray icon.

Regression coverage includes failed settings loads/replacements, UTF-8 boundaries, oversized JSON, negative penetration-table distances and database load rollback. Windows CI additionally tests recovery from corrupt settings and database files through application launch, rendering and shutdown. Real-game validation remains outstanding.

## Interface redesign 0.4

The native control centre now uses Segoe UI typography, a persistent sidebar, clear status cards, separate session controls, a telemetry inspector, selectable performance presets, and an armour studio with a grid-backed schematic and analysis panel. Overlay settings include a position preview; the profile page has reset confirmation; the data library has a searchable table. Existing connection, overlay, settings and database controls remain functional.

Telemetry mode changes clear old data immediately, pending results from older modes are rejected, and data older than four seconds is marked stale. Each endpoint publishes independently so an unavailable map endpoint does not delay state display. The Windows smoke test renders every page. The release includes a captured UI preview.

## Vehicle identification 0.4.1

The user-supplied Charioteer indicators response is included as a regression fixture. Its reported model ID is mapped to a display label; this is not a verified armour profile. Vehicle identity is read from the complete indicators JSON before limiting inspector rows. `/state` failure does not prevent identifying the vehicle. No selected ammunition identifier was present in the supplied response; `first_stage_ammo: -1` is not interpreted as a shell type.

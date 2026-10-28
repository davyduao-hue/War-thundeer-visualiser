Bug-fix update 0.3:
- Settings load transactionally: malformed files cannot partially change the running configuration.
- Settings replacement is atomic on Windows; corrupt settings are preserved until Reset settings is selected.
- Unicode names and telemetry are truncated at character boundaries; report export handles errors without terminating the app.
- A corrupt database falls back to the embedded, clearly labelled synthetic profiles without overwriting the damaged file.
- Launching the application twice restores the existing instance; unavailable tray support minimises to the taskbar, and Explorer restart re-registers the tray icon.
- Shutdown cleans up even if saving settings fails.

Download WTArmourVision.exe for Windows x64. No developer tools or additional demo data files are needed. Click CONNECT TO GAME; use borderless mode and manually choose the game window if needed. Real War Thunder compatibility remains unverified. No live armour detection, weakspot targeting, automatic ammunition selection or validated damage prediction is included.

# Required Windows validation — NOT EXECUTED

1. Build Release x64 with VS2022; run CTest; install to package directory.
2. Launch packaged executable from Explorer with no developer tools on PATH.
3. Exercise all seven pages; resize, minimise, restore, change display DPI and monitors.
4. Enable demo overlay, change opacity/scale and disable it. Confirm clicks reach the underlying window and the overlay never takes focus.
5. Test tray restore/exit, Alt+F8 registration conflict, settings persistence and malformed settings recovery.
6. With an allowed ground-battle test session, disable demo; verify each endpoint's status and JSON fields separately. Record game version and field units. Never infer automatic vehicle/ammunition support from generic valid JSON.
7. Test game launch/close/relaunch, minimisation and focus loss; test borderless resolution changes.
8. Stop telemetry mid-session and confirm timeout/reconnect and responsive UI. Confirm orderly exit while a request is pending.
9. Measure CPU, memory and frame time for each preset on target RX 580 hardware. Do not claim performance impact before measurement.
10. Test missing/corrupt database and write-protected settings directories. Check packaged files and third-party licences.

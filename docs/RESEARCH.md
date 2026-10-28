# Research status (2026-10-28)

The supplied specification lists candidate localhost endpoints `/state`, `/indicators` and `/map_info.json` on port 8111. Their implementation here is exploratory; no running game is available to establish ground-battle support or field semantics. A successful JSON response alone does not verify vehicle or shell identification.

Official wiki: https://wiki.warthunder.com/ — attempted access from the development environment returned proxy HTTP 403. No wiki contents or license terms were retrieved. Official terms reference for future review: https://legal.gaijin.net/termsofservice — not retrieved, so current third-party application permission is unresolved. These links are research leads, not evidence of approval. Review the current rules before enabling an overlay in online play.

Dear ImGui v1.91.9b and nlohmann/json v3.11.3 were obtained from their upstream GitHub projects over TLS, with upstream MIT license files retained. All bundled armour/shell numbers and telemetry are original synthetic test fixtures, not real vehicle statistics. No third-party vehicle model or trained recognition model is bundled.

The program uses ordinary window enumeration, limited process image-name queries and localhost HTTP. It does not inject code, read game memory, bypass anti-cheat or implement automated live target/weakspot selection. Offline calculations are approximate and incomplete; verified data and validated models remain prerequisites for useful vehicle analysis.

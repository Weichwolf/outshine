# FlightBox — Simulation

**JSBSim-Frontend.** Ein DCS-World-artiger Flugsimulator: die Physik ist JSBSim, direkt ins Command
Center kompiliert. Physik + Regelung + Rendering laufen als **ein** Prozess (WASM im Browser, native
im CLI). Server-seitig läuft **nur** der Kachel-Server `fb-tiles`.

```
  fb-tiles (Server)                    Command Center (Client, ein Prozess)
  weltweit DEM/OSM/Luftbild  ──HTTP──▶  libJSBSim  ─▶ FBW ─▶ Loiter-AP
  auf :8081                             │                        │
                                        └────── ECEF-Renderer + MIL-STD-1787-HUD ◀── liest JSBSim-Zustand direkt
```

## Komponenten
- `aircraft/fdm/jsbsim_adapter.*` — C-aufrufbarer Adapter um libJSBSim (Load/Step/Trim, Controls,
  Ground-Clearance aus der Fahrwerks-Geometrie). Die einzige Naht zwischen unserem Code und JSBSim.
- `aircraft/models/<name>/` — Flugzeug-Plugins: JSBSim-Modell (XML + engine/), `profile.env` (Vs/Vc,
  Spawn). F-16 bis EPP-Nurflügel.
- Steuerung: schlanker **Fly-by-Wire** (Raten-/Attitude-PID auf `fcs/*-cmd-norm`) + kleiner
  **Autopilot** (`manual`, `loiter(lat,lon,alt,radius)`). Kein iNav.
- `command_center/` — der Client: `cc.c` (Emscripten/SDL2) + der WGS84-ECEF-Renderer (`world3d.h`,
  `render.h`, Reversed-Z-Depth), osmmesh-Terrain in per-Tile-ECEF, HUD (`hud.h`, MAX7456-Font).
- `tiles/` — der `fb-tiles`-Server: weltweit DEM/OSM/Luftbild on-demand.
- `geo/osmmesh/` — unsere Terrain-Vermaschung (nicht vendored).

## Bauen & Starten
```bash
make tiles            # fb-tiles-Image bauen
make up-tiles         # fb-tiles starten -> :8081
make controlcenter    # WASM-CC (JSBSim + Renderer) -> flightbox/web/  (nutzt ~/Git/emsdk)
# CC im Browser öffnen; per lat/lon/alt/radius weltweit loitern und rendern
```
Der native CLI-Build (dieselbe Physik + Renderer, headless) fährt die weltweiten Screenshot-Läufe zum
Härten des Renderers.

## Status / Pivot (2026-07-21)
Früher: zwei Container (aircraft + flightbox) mit iNav in der Schleife, UDP/MSP-„Funk". **Retired.**
Jetzt: JSBSim direkt im Client, nur `fb-tiles` server-seitig. iNav (`inav-src`), `flightbox/`-Hub,
`msp_bridge`, das TS-Missions-/Validator-Testsystem und `test/` sind **superseded** und werden entfernt,
sobald der JSBSim-in-CC-Pfad steht. Renderer + Aircraft-Plugins + `jsbsim_adapter` bleiben.

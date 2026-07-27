# FlightBox

A **JSBSim-backed F-16 simulator at DCS-World fidelity** — a **WASM app** with a **worldwide tileserver
backend** that maps the whole Earth. The flight physics is **JSBSim**; FlightBox is the world around it:
global real-world terrain, a WebGPU WGS84-ECEF renderer, a MIL-STD-1787 HUD, and the controls to fly the
**F-16 anywhere on Earth**. Two quality axes define the project: **correct rendering** (bar:
X-Plane/MSFS/DCS) and **faithful F-16 flight** — the reference is the vanilla JSBSim F-16 model itself.
Aircraft are runtime **modules** (DCS-style): each carries its own systems — displays, flight control,
sensors, weapons — behind shared interfaces; the F-16 is the product, further airframes plug in.

**JSBSim compiles directly into the Command Center.** Physics, control and rendering run in **one
process** — WASM in the browser, native on the CLI. The camera reads the JSBSim state directly: no
telemetry wire, no separate world process, no firmware in the loop. A thin **fly-by-wire** layer
stabilizes the aircraft; on top, **FBPilot** flies a `.fbm` mission as a phase machine (preflight,
takeoff, climb, route, ...), commanding `FBAutopilot::Direct` point-to-point guidance each phase — the
default boot mode anywhere on the planet. `manual` stays available as a direct-stick sandbox.

**Only one thing runs server-side: the tile server (`fb-tiles`).** It serves worldwide Copernicus DEM,
OSM vector, and aerial imagery on demand — so **any point on Earth is a valid start**. Everything else
is the client.

Aircraft are **plugins**: a directory with a JSBSim model (aero / mass / propulsion + engine data),
nothing hand-written. **JSBSim is a pinned, read-only git submodule**, built from source, so the
physics is bit-identical to the pinned commit and update-safe.

## Build & run

Two self-contained projects: **`sim/`** (the `fb-sim` Command Center — WASM app, native oracle, and web
host) and **`tiles/`** (the `fb-tiles` world-data server). Each carries its own Makefile, Dockerfile and
source.

```bash
make -C tiles image        # build the fb-tiles server (worldwide DEM / OSM / imagery)
make -C tiles run          # run it -> :8081
make -C sim wasm           # build the WASM Command Center + tile worker -> sim/web/
make -C sim up             # run the fb-sim web host -> http://localhost:8080
# open the Command Center in a browser; FBPilot flies the default mission anywhere on Earth
```

The native oracle (`make -C sim native` → `sim/build/gpu_native`, same JSBSim + renderer, headless)
drives worldwide screenshot runs and in-process `--mission FILE --interval S` flight to harden the
renderer.

The architecture — JSBSim-in-the-client, the fly-by-wire + FBPilot mission autopilot, aircraft plugins,
and the ECEF renderer — is in [`CLAUDE.md`](CLAUDE.md).

## Lizenz

FlightBox steht unter der **GNU General Public License, Version 2 oder später**
(`GPL-2.0-or-later`) — Lizenztext in [`LICENSE`](LICENSE).

## Datenquellen & Lizenzen

Der Simulator lädt reale Welt-Daten on-demand. Attributionspflicht der Quellen:

- **Kartendaten:** © OpenStreetMap contributors — lizenziert unter der
  [Open Database License (ODbL)](https://www.openstreetmap.org/copyright) (Shortbread-Vektorkacheln).
- **Luftbilder:** Esri World Imagery — © Esri und seine Datenlieferanten.
- **Geländehöhe:** Copernicus DEM (Terrarium-kodiert).
- **Flugphysik:** [JSBSim](https://github.com/JSBSim-Team/jsbsim) — LGPL 2.1.
- **WebGPU-Backend:** Dawn / Tint (Google) — BSD-3-Clause.
- **HUD-Schrift:** B612 Mono (Airbus) — SIL OFL 1.1.
- **Sternkatalog:** [HYG Database](https://github.com/astronexus/HYG-Database) (Hipparcos-abgeleitet)
  — lizenziert unter CC-BY-SA 4.0.
- **Mondtextur:** NASA/GSFC Scientific Visualization Studio, CGI Moon Kit (LROC WAC Albedo,
  `sim/web/moon.jpg`) — public domain.

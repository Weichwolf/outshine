# FlightBox

A **JSBSim-backed F-16 simulator at DCS-World fidelity** — a **WASM app** with a **worldwide tileserver
backend** that maps the whole Earth. The flight physics is **JSBSim**; FlightBox is the world around it:
global real-world terrain, a WGS84-ECEF renderer, a MIL-STD-1787 HUD, and the controls to fly the
**F-16 anywhere on Earth**. Two quality axes define the project: **correct rendering** and **realistic
F-16 flight behavior** — measured against the real jet, DCS World, and X-Plane. (Aircraft are still
plugins — a JSBSim model directory — but the F-16 is the product.)

**JSBSim compiles directly into the Command Center.** Physics, control and rendering run in **one
process** — WASM in the browser, native on the CLI. The camera reads the JSBSim state directly: no
telemetry wire, no separate world process, no firmware in the loop. A thin **fly-by-wire** layer
stabilizes the aircraft and a small **loiter autopilot** (center / altitude / radius) flies it as a
camera platform over any location on the planet.

**Only one thing runs server-side: the tile server (`fb-tiles`).** It serves worldwide Copernicus DEM,
OSM vector, and aerial imagery on demand — so **any point on Earth is a valid start**. Everything else
is the client.

Aircraft are **plugins**: a directory with a JSBSim model (aero / mass / propulsion + engine data),
nothing hand-written. **JSBSim is a pinned, read-only git submodule**, built from source, so the
physics is bit-identical to the pinned commit and update-safe.

## Build & run

From `sim/`:

```bash
make tiles                # build the fb-tiles server image (worldwide DEM / OSM / imagery)
make controlcenter        # build the WASM command center (JSBSim + renderer) -> flightbox/web/
make up-tiles             # run the ONE server-side container -> :8081
# open the command center in a browser; loiter anywhere on Earth by lat/lon/alt/radius
```

The native CLI build (same JSBSim + renderer, headless) drives worldwide screenshot runs to harden the
renderer.

The architecture — JSBSim-in-the-client, the fly-by-wire + loiter autopilot, aircraft plugins, and the
ECEF renderer — is in [`CLAUDE.md`](CLAUDE.md).

> **Note (2026-07-21 pivot):** FlightBox was previously an *iNav-in-the-loop* rig (a validation harness
> for real RC firmware). That premise is retired — the project is now a JSBSim frontend. iNav, MSP, the
> flightbox hub and the mission/validator test system are superseded and being removed.

## Datenquellen & Lizenzen

Der Simulator lädt reale Welt-Daten on-demand. Attributionspflicht der Quellen:

- **Kartendaten:** © OpenStreetMap contributors — lizenziert unter der
  [Open Database License (ODbL)](https://www.openstreetmap.org/copyright) (Shortbread-Vektorkacheln).
- **Luftbilder:** Esri World Imagery — © Esri und seine Datenlieferanten.
- **Geländehöhe:** Copernicus DEM (Terrarium-kodiert).
- **Flugphysik:** [JSBSim](https://github.com/JSBSim-Team/jsbsim) — LGPL 2.1.
- **Sternkatalog:** [HYG Database](https://github.com/astronexus/HYG-Database) (Hipparcos-abgeleitet)
  — lizenziert unter CC-BY-SA 4.0.
- **Mondtextur:** NASA/GSFC Scientific Visualization Studio, CGI Moon Kit (LROC WAC Albedo,
  `flightbox/web/moon.jpg`) — public domain.

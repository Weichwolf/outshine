# FlightBox

A **global flight simulator** built on **X-Plane / MSFS / DCS-class technology**, to a **MIL-SPEC**
engineering bar. It simulates **arbitrary aircraft** — an F-16, a Cessna 172, down to a foam motor-glider —
anywhere on real-world global terrain, with **real iNav firmware in the loop**.

One system, two uses:

1. A full flight simulator.
2. A control / validation rig for real flight models — the same iNav SITL that flies the sim flies
   the physical model, so control laws, failsafe, autopilot and handling are **proven in simulation**
   before they fly.

The command center and flight bridge are C; the flight physics is **JSBSim** (industry-standard,
behind a C-callable adapter), so aircraft are **plugins** — an iNav config plus a JSBSim model,
nothing hand-written. **iNav and JSBSim are pinned, read-only git submodules — never patched** (the
firmware stays bit-vanilla and update-safe). The flight bridge speaks iNav's built-in `--sim=xp`
X-Plane protocol with `--useimu`, so **stock** iNav SITL fuses attitude itself and connects with zero
modification; a WASM browser **command center** renders global 3D terrain (real OSM + Copernicus DEM),
live weather, and a MIL-STD-1787 HUD. Nothing is preloaded — every tile is fetched on demand, so **any
point on Earth is a valid start**.

## Build & run

```bash
sim/build-wasm.sh         # renderer -> flightbox/web/
sim/run-podman.sh         # builds 3 images + starts the stack -> localhost:8080
sim/test/verify.sh        # all gates: unit+coverage, builds, render, physics
```

Set `ORIGIN_LAT` / `ORIGIN_LON` to start anywhere on Earth.

The architecture — the three systems, the two API boundaries (sensor via `--sim=xp`/`--useimu`, radio
via MSP), aircraft plugins, and CC-commands-iNav-flies-native — is in [`CLAUDE.md`](CLAUDE.md).

## Datenquellen & Lizenzen

Der Simulator lädt reale Welt-Daten on-demand. Attributionspflicht der Quellen:

- **Kartendaten:** © OpenStreetMap contributors — lizenziert unter der
  [Open Database License (ODbL)](https://www.openstreetmap.org/copyright) (Shortbread-Vektorkacheln).
- **Luftbilder:** Esri World Imagery — © Esri und seine Datenlieferanten.
- **Geländehöhe:** Copernicus DEM (Terrarium-kodiert).
- **Sternkatalog:** [HYG Database](https://github.com/astronexus/HYG-Database) (Hipparcos-abgeleitet)
  — lizenziert unter CC-BY-SA 4.0.

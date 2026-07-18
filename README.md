# FlightBox

A **global flight simulator** built on **X-Plane / MSFS / DCS-class technology**, to a **MIL-SPEC**
engineering bar. It simulates **arbitrary aircraft** — an F-16 down to an EPP-foam flying-wing —
anywhere on real-world global terrain, with **real iNav firmware in the loop**.

One system, two uses:

1. A full flight simulator.
2. A control / validation rig for real flight models — the same iNav SITL that flies the sim flies
   the physical model, so control laws, failsafe, autopilot and handling are **proven in simulation**
   before they fly.

All C. The flight model speaks the X-Plane UDP protocol so real iNav SITL connects to it; a WASM
browser **command center** renders global 3D terrain (real OSM + Copernicus DEM), live weather, and a
MIL-STD-1787 HUD. Nothing is preloaded — every tile is fetched on demand, so **any point on Earth is
a valid start**.

## Build & run

```bash
sim/build-wasm.sh         # renderer -> flightbox/web/
sim/run-podman.sh         # builds 3 images + starts the stack -> localhost:8080
sim/test/verify.sh        # all gates: unit+coverage, builds, render, physics
```

Set `ORIGIN_LAT` / `ORIGIN_LON` to start anywhere on Earth.

Architecture, standards and the specialist-agent map are in [`CLAUDE.md`](CLAUDE.md). The reference
flying-wing hardware target is in [`doc/flying-wing.md`](doc/flying-wing.md).

## Datenquellen & Lizenzen

Der Simulator lädt reale Welt-Daten on-demand. Attributionspflicht der Quellen:

- **Kartendaten:** © OpenStreetMap contributors — lizenziert unter der
  [Open Database License (ODbL)](https://www.openstreetmap.org/copyright) (Shortbread-Vektorkacheln).
- **Luftbilder:** Esri World Imagery — © Esri und seine Datenlieferanten.
- **Geländehöhe:** Copernicus DEM (Terrarium-kodiert).
- **Sternkatalog:** [HYG Database](https://github.com/astronexus/HYG-Database) (Hipparcos-abgeleitet)
  — lizenziert unter CC-BY-SA 4.0.

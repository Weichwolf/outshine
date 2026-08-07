# Roadmap

The stages, thin on purpose. **What** each stage must achieve lives in the Spec section of its topic
file; this page only orders them and says where the work is described. How a stage is built and what
counts as done: [`goal.md`](goal.md), which is binding until revoked. What already landed:
[`journal.md`](journal.md).

## The order, and why it is this one

Geometry → LOD → lighting → colour, and the layers in the order **terrain + buildings → trees →
perennials → turbine → grass (last, or never)** ([`goal.md`](goal.md)). Terrain and buildings first
because from 1.70 m a town is its buildings; trees before perennials because the loudest defect in the
frame is *a forest without trees*; grass last because it held everything up and the aggregate machinery
it needs belongs to trees, where there is a measured precedent.

Each layer runs **build → critics accept → optimise**, and optimisation happens inside the layer.

## Stages

| # | Stage | Spec / gaps live in | State |
|---|---|---|---|
| **L0** | **The pedestrian's frame.** A camera at eye height over the declared scene, the terrain streamer, one PNG — the bench everything else is measured on | [`clients/clients.md`](clients/clients.md), [`render/visual-target.md`](render/visual-target.md) | **built and measured, 2026-08-07.** `build/gpu_walk` links warning-free under `-Wall -Wextra -Wpedantic -Werror`. `mods/demo/scene.json` (52.10602 N, 9.43453 E, eye 1.70 m, yaw 280, 2026-08-06T17:40:00Z): ground **100.596 m** against `/elev?block=1` = 100.60, sun el 11.202 / az 282.601, 7 render passes. Frame: `sim/walk-demo.png` |
| **L1** | **Terrain and the ground material.** Tile stream, mesh, the classification chain from albedo + position + OSM to class weights, and the ground as a material rather than a colour | [`world/terrain.md`](world/terrain.md), [`render/classification.md`](render/classification.md), [`render/stages/terrain.md`](render/stages/terrain.md) | **built.** The class is resolved on the CPU to one byte per texel and the material is drawn from it — `albedoVramMB` 130 → 0 against `classVramMB` 43.33 |
| **L2** | **Buildings out of OSM.** The vector tile carries the footprints. Extrusion rule, height source and fallback, LOD, and what happens where OSM gives no height | [`render/stages/buildings.md`](render/stages/buildings.md), [`world/terrain.md`](world/terrain.md) | **built.** `world/OsmVector` + `world/BuildingField` extrude the z14 footprints, `BuildingsStage` draws them, and they cast into the shadow cascade: **16 025** building triangles from 48 075 vertices in `sim/walk-demo.png`. Height source, fallback and LOD are not written down anywhere |
| **L3** | **Trees.** The layer with the measured precedent for its far stage, and the loudest hole in the frame today — 98 % of ground pixels within 50 m classify as `laubmischwald` and no tree stands in them | [`render/vegetation.md`](render/vegetation.md), [`render/lod.md`](render/lod.md) | **nothing built.** The next stage |
| **L4** | **Perennials.** The 0–40 m layer stack between the sward and the canopy | [`render/vegetation.md`](render/vegetation.md) | nothing built |
| **L5** | **The turbine.** The one thing in the scene that moves, and therefore the one that needs a published anchor — tip speed ratio, measured in the simulation state and in the rendered image | [`body-format.md`](body-format.md) | nothing built |
| **L6** | **Grass.** Stopped by decision, at the end of the order or never. What remains is the stand as an aggregate term of the ground fragment; the blade geometry is deleted | [`render/stages/terrain.md`](render/stages/terrain.md), [`render/vegetation.md`](render/vegetation.md) | **stopped, 2026-08-07.** The geometry cost 61 % of the GPU frame and is gone; nothing below tree size moves |
| **E1** | **Epoch and decay.** Three epochs × three decay steps, a selection and not a blend. What the dial reaches (materials, vegetation density, building state, road surface) and what it must NOT reach (geometry and identity — the same dataset has to stay the same dataset) | [`mods.md`](mods.md), [`render/classification.md`](render/classification.md) | **nothing built.** `kEpoch`/`kDecay` exist as the stand's own declaration (`render/Sward.h`), both 0, and **nothing reads them**; every other place a material sits still owes the same pair |
| **D1** | **Declarations in JSON.** `mods/demo/scene.json` is the only declaration surface that exists. A manifest, a body and an entity each need a JSON shape with a schema | [`mods.md`](mods.md) | `scene.json` and `mod.json` are the whole of it |
| **B1** | **The body format — and list A comes before the solver.** Five declarations (segments, joints, contacts, force sources, medium) plus model, materials and brain. **The gating work is writing list A**: the written-down list of places a knowledgeable person checks, each with a band and a source | [`body-format.md`](body-format.md) | **spec only, nothing built.** The oracle the old §3 leaned on no longer exists; list A must be written from sources |

## Landed and out of the order

| # | Stage | Where | State |
|---|---|---|---|
| **W1** | **Weather, server half:** `/wx` on fb-tiles — NOAA GFS 0.25°, one compact global raster per variable (1440×721); wind at 10 m and 850/700/500/250 hPa, cover per étage, cloud base, visibility. The format **is** the interface | [`world/weather.md`](world/weather.md) | **built.** FBWX frozen at version 1, GRIB2 decoder, byte-identical across machines and compilers |
| **W2** | **Weather, client half:** a weather provider beside the elevation hook — calm, constant wind, the fixed FBWX blob, the live fetch as configuration | [`world/weather.md`](world/weather.md), [`core.md`](core.md) | **built** (`43b82b5`) |
| **W3** | **Cloud rebuild** — one stage over one shared density function, C++ and WGSL from the same constants, haze shared with the terrain | [`render/clouds.md`](render/clouds.md) | **built**, with its numeric and frame gates |
| **X1** | **The tree links again** | [`architecture.md`](architecture.md), [`core.md`](core.md) | **done, 2026-08-07, by deletion.** The simulation layer that named the deleted `Fdm` class was deleted with its subject rather than ported. `sim/src/` is `clients/ core/ render/ render/stages/ units/ world/ world/terrain/`; both clients build, `verify-layers` is green. What that cost is booked in [`core.md`](core.md) `## Gaps` and [`conventions.md`](conventions.md) |

## Parked

Work that has no home file yet, kept here so it cannot be lost:

| Thing | Waits for |
|---|---|
| `units/` has **no topic file**, and only `Unit.h` + `UnitRegistry.h` survive in it | `world/World.cpp`'s effect path is the only caller. It also pins ten combat value types in `core/`, which is what `verify-types` still counts |
| `sim/test/` does **not exist**, and no document describes what it should be | a subject that can assert about itself. `verify-trees` names it in 5 of its 9 orphans and that is the gate working |
| **`UnitsStage`/`SpritesStage` have no topic file.** `render/units-visual.md` was retired on 2026-08-07 with its combat effect catalogue; the entity-drawing half survives in code | a body to point it at. Then `render/entities.md` |
| **The `/wx` half of the tile server has no consumer.** Format, decoder and determinism are built and measured on `tiles/`; nothing in `sim/src/` parses FBWX | a new reader in `core/`, a mirror that cannot drift from `tiles/src/wxfmt.h`, and a declaration surface ([`world/weather.md`](world/weather.md) `## Gaps`) |

## How a stage runs

The working rule is in [`conventions.md`](conventions.md) and is binding: change the **Spec** of the
topic file first, build until **State** meets it, then update State and Gaps and add one line to
[`journal.md`](journal.md). Rejected approaches stay in Gaps with their measurements.

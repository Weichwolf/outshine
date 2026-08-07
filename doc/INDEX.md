# Outshine — knowledge base

What Outshine is, what it must become, and what is actually built. This collection is the
**authority**; `CLAUDE.md` in the repository root is a session-start card and points here. If the two
disagree, this wins and `CLAUDE.md` is to be corrected.

Loaded task-wise through the skill **`outshine`** (`.claude/skills/outshine/SKILL.md`).

> **Outshine is an OSM-based GTA 5, and the epoch parameter drives the look from Witcher 3 to
> Fallout 4.**

## Read this first — the state of the tree, 2026-08-07

Two hard cuts happened. What they removed is not "deprecated", it is **gone**:

| Gone | Consequence for this collection |
|---|---|
| JSBSim, the F-16, the MiG-29, their reference bases and modules | no file here may cite an aircraft model, a JSBSim property or an envelope anchor |
| the four NovaLogic titles | every mod example is hypothetical until one is written |
| the four line formats `.fbm` · `.fbc` · `.fba` · `.fbh` | **declarations are JSON.** A hand-written parser for a bespoke line format is not written again |
| the combat tooling, the mission/campaign gyms, the ten campaign specs | no measured combat number in this tree has a subject any more |
| **the whole simulation layer** — `sensors/ weapons/ pilot/ modules/ missions/ systems/`, the avionics group in `render/`, `core-lib`, `fb-gym`, `verify-guards`, `verify-tests`, `fb_test.py` and every `test-*` target | there is no runner, no judge, no health register, no sensor and no headless client. **`sim/test/` does not exist** |

**`sim/src/` is exactly five directories: `clients` · `core` · `render` · `units` · `world`.**
Both clients build and `verify-layers` is green.

**205 files and ~32 000 lines left `sim/src/`, and `doc/` followed.** Retired on 2026-08-07 because
their whole subject was deleted: `testing.md`, `client-server.md`, `persistent-world.md`,
`actor-scale.md`, `player-layer.md`, `assets.md`. Git holds them; nothing here points at them.

## The mirror

The directory layout is not a taxonomy of its own — it is `sim/src/`, one file or one directory per
source **directory** (not per file).

| `sim/src/` | `doc/` |
|---|---|
| `core/` | [core.md](core.md) |
| `render/`, `render/stages/` | [render/](render/renderer.md) — nine files plus `render/stages/`, one document per pass |
| `world/`, `world/terrain/` | [world/](world/terrain.md) — two files |
| `clients/` | [clients/clients.md](clients/clients.md) |
| `units/` | **no topic file today.** Only `Unit.h` + `UnitRegistry.h` survive, kept alive by `world/World.cpp`'s effect path. A named hole, not an oversight |
| `test/` | **does not exist, and no document describes what it should be.** `verify-trees` names it in 5 of its 9 orphans, and that is the gate working |
| *(no source dir yet)* | [body-format.md](body-format.md), [mods.md](mods.md) |
| *(meta)* | this index, [goal.md](goal.md), [vision.md](vision.md), [roadmap.md](roadmap.md), [journal.md](journal.md), [conventions.md](conventions.md), [architecture.md](architecture.md), [build-and-ops.md](build-and-ops.md) |

## How to read a file

Every topic file carries the same four sections:

| Section | What it is | Use it when |
|---|---|---|
| `## Spec` | the contract — what it must do, acceptance criteria, measurement anchors. Changes by decision, not by building. | you are about to change behaviour: change this first |
| `## State` | what is built, with commit and measurement. Honest, including "nothing built" | you need to know what exists today |
| `## Gaps` | Spec − State, ordered by value, **including rejected approaches with their measurements** | you are looking for work, or about to retry something |
| `## Knowledge` | derivations, formulas, measured constants | you need the number and where it came from |

The working rule that binds a round to this shape is in [`conventions.md`](conventions.md).

The **meta files** (this index, `goal.md`, `vision.md`, `roadmap.md`, `journal.md`, `conventions.md`)
carry no Spec/State/Gaps — they *are* the direction, the order, the history and the rules.

**Language:** English throughout. `journal.md` is the exception by decision: it is the chronicle, it is
German, and it may carry names of things that no longer exist. **It is never rewritten.**

## Start here

| File | Content |
|---|---|
| **[goal.md](goal.md)** | **the standing goal and it is binding until revoked** — one reference scene, the order of work, the rules, the measurement discipline, who judges what |
| [vision.md](vision.md) | what Outshine is *for*: an engine cut for a machine, the three epoch mods at one place as its acceptance, believability over fidelity, the tiers A/AA/AAA |
| [roadmap.md](roadmap.md) | the layers in order — each one points at the file whose Spec it must satisfy |
| [conventions.md](conventions.md) | naming, structure, the no-printf rule, **every number carries its provenance**, and the spec-first working rule |
| [architecture.md](architecture.md) | process model, the two clients, directory map, the layering pattern |
| [journal.md](journal.md) | the chronicle: one entry per finished round |
| [build-and-ops.md](build-and-ops.md) | make targets, the **gates**, the measurement tools, measurement discipline, host specifics |

## The engine

| File | Content |
|---|---|
| [body-format.md](body-format.md) | the declarative body: five declarations (segments, joints, contacts, force sources, medium) plus model, materials and brain — the one format from furniture to rockets. **Spec only** |
| [mods.md](mods.md) | what a title may declare and what it may not — the epistemic boundary (*does this need knowledge no participant could have?*), why declarations are JSON, and why a mod adds no world |
| [core.md](core.md) | the shared floor: log and telemetry buses, geodesy, units, matrices, calendar and ephemeris, the elevation hook, the weather providers |

## Rendering — `render/`

**One document per render pass**, under `render/stages/`; everything above them is the goal, the
orchestrator, or a mechanism that binds all of them.

| File | Content |
|---|---|
| [render/visual-target.md](render/visual-target.md) | the look bar and the budget: Witcher 3 / Fallout 4 / GTA 5, 2015er technique, procedural first. **The overarching goal, not a pass** |
| [render/renderer.md](render/renderer.md) | WebGPU, ECEF camera-relative, reversed-Z, the **pass topology as a contract**, the stage catalogue, camera and ground truth |
| [render/classification.md](render/classification.md) | the chain **before** the first pass: albedo + position + OSM → class weights, and the same structure for buildings |
| [render/lod.md](render/lod.md) | continuous LOD 1 m…>1000 m: screen-space error and its threshold τ, the Nanite judgement, hashed alpha, FLIP as the popping gate. **Binds every pass** |
| [render/lighting.md](render/lighting.md) | the light budget: what lights what, and where the scale comes from |
| [render/vegetation.md](render/vegetation.md) | 256 templates from albedo, the 0–40 m layer stack, the sixteen Weserbergland species, `wasm-tree` measured |
| [render/clouds.md](render/clouds.md) | the cloud rebuild spec — bounded-volumetric but simple, including the cirrus layer |
| [render/gpu-determinism.md](render/gpu-determinism.md) | what the WebGPU/WGSL specifications actually guarantee, and what an entity identity may therefore be made of |

### The passes — `render/stages/`

| File | Pass(es) | Why grouped that way |
|---|---|---|
| [render/stages/terrain.md](render/stages/terrain.md) | Tiles | **plus the ground material and the stand** — the ground is a material, not a colour, and everything below the size of a tree is a term in that fragment |
| [render/stages/buildings.md](render/stages/buildings.md) | Buildings | |
| [render/stages/shadow.md](render/stages/shadow.md) | Shadow | |
| [render/stages/ao.md](render/stages/ao.md) | Ao | |
| [render/stages/taa.md](render/stages/taa.md) | Taa | **plus the sub-pixel jitter**, which is not a pass but is the half that makes the resolve worth having |
| [render/stages/atmosphere.md](render/stages/atmosphere.md) | Transmittance · MultiScatter · SkyView · Sky · Irradiance | **one LUT chain** — none of them is judgeable alone |
| [render/stages/celestial.md](render/stages/celestial.md) | Sun · Moon · Stars | one ephemeris |
| [render/stages/tonemap.md](render/stages/tonemap.md) | Tonemap · Upscale · Exposure | one exposure scale at the end of the chain — **the anchor comes from the irradiance, never from the picture** |
| [render/stages/tile-lights.md](render/stages/tile-lights.md) | TileLights | |
| [render/clouds.md](render/clouds.md) | CloudLayer | its own rebuild spec, above |

**Two named holes in this table:**

- **`UnitsStage` and `SpritesStage` have no topic file.** `render/units-visual.md` was retired on
  2026-08-07 with its subject — a combat effect catalogue (nozzle plume, motor trail, countermeasures,
  detonation, wreck fire, navigation lights) whose inputs no longer have a writer. The **entity drawing**
  half survives in code and needs a `render/entities.md` as soon as a body exists to point it at.
  Interim: [render/renderer.md](render/renderer.md) §3.
- **`BenchGroundStage`** — the `--rig` bench's floor and neutral card. Its contract is written in
  [clients/clients.md](clients/clients.md), not in `stages/`.

## World — `world/`

| File | Content |
|---|---|
| [world/terrain.md](world/terrain.md) | the world object, tile streaming and the worker, elevation over tiles, the terrain library, `fb-tiles` from the client's side |
| [world/weather.md](world/weather.md) | the `/wx` data kind: the FBWX format, the GFS run determination, the GRIB2 decoder and its independent verification, the operating figures |

## Clients — `clients/`

| File | Content |
|---|---|
| [clients/clients.md](clients/clients.md) | what each client must be — the native frame oracle, its `--rig` subject bench, the browser — contract, state and gaps per client |

## Not part of the mirror

| Place | Subject |
|---|---|
| `doc/webgl-webgpu-report.txt` | the target GPU capability survey — the hardware the 16.67 ms budget is spent on |

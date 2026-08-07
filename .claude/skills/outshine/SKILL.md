---
name: outshine
description: The Outshine knowledge base — an OSM-based open-world game engine cut for a machine to build games with, where an epoch parameter drives the look from Witcher 3 to Fallout 4. Covers the vision and roadmap, the declarative body format, the mod boundary and module contract, the renderer and its stages, terrain/tile streaming and weather, the persistent world, assets, the client split, testing philosophy and coding conventions. Load when working ON Outshine's code or architecture, on a mod's declaration, on the renderer or the world; when judging whether a change fits; or when answering what must be true, what is built, what is missing and what comes next.
---

# Outshine Reference

> **Outshine is an OSM-based GTA 5, and the epoch parameter drives the look from Witcher 3 to
> Fallout 4.**

The knowledge base lives in `<repo>/doc/`. It is the authority on **what Outshine must be and how it is
built**. `CLAUDE.md` is a session-start card and points here; if the two disagree, `doc/` is right and
`CLAUDE.md` needs fixing.

Start at [`doc/INDEX.md`](../../../doc/INDEX.md).

## Read this first, always

| File | Why |
|---|---|
| `CLAUDE.md` (repo root) | the principles and the anti-cheat guarantees, condensed. A change that violates one of these is wrong even if it works |
| `doc/vision.md` | what the thing is *for*, and the staggered scale — it decides how deep a subsystem may go |
| `doc/conventions.md` | naming, structure, the no-printf rule, **every number carries its provenance**, and the spec-first working rule |

## The map: `doc/` mirrors `sim/src/`

The layout is the source tree, not a taxonomy of its own. `doc/` mirrors **directories**, not files.

| `sim/src/` | `doc/` |
|---|---|
| `core/` | `core.md` |
| `render/` | `render/visual-target.md` (the goal), `render/renderer.md` (the orchestrator), `render/classification.md`, `render/lod.md`, `render/vegetation.md`, `render/hud.md`, `render/clouds.md`, `render/gpu-determinism.md`, `render/units-visual.md` |
| `render/stages/` | **one file per pass**: `terrain.md` (Tiles) · `ground-cover.md` (GroundCover + the ground material) · `buildings.md` · `shadow.md` · `ao.md` · `atmosphere.md` (Transmittance · MultiScatter · SkyView · Sky · Irradiance) · `celestial.md` (Sun · Moon · Stars) · `tonemap.md` (Tonemap · Upscale · Exposure) · `nvis.md` · `ground-map.md` · `map-sheet.md` · `tile-lights.md`. CloudLayer is `render/clouds.md`, Units/Sprites `render/units-visual.md`, Hud `render/hud.md` |
| `world/` | `world/terrain.md`, `world/weather.md` |
| `clients/` | `clients/clients.md` |
| *(no source dir yet)* | `body-format.md`, `mods.md`, `module-contract.md`, `persistent-world.md`, `actor-scale.md`, `player-layer.md`, `assets.md`, `client-server.md`, `testing.md` |
| *(meta)* | `INDEX.md`, `vision.md`, `roadmap.md`, `journal.md`, `conventions.md`, `architecture.md`, `build-and-ops.md` |

## The shape of every topic file

Spec-driven. Four sections, and which one you read depends on the question:

| Section | What it holds | Read it when |
|---|---|---|
| `## Spec` | the contract: what it must do, acceptance criteria, measurement anchors | you are about to change behaviour — **change this first** |
| `## State` | what is built, with commit and measurement. Honest, including "nothing" | you need to know what exists today |
| `## Gaps` | Spec − State by value, **including rejected approaches with their measurements** | you are picking work, or about to retry something that already failed |
| `## Knowledge` | derivations, formulas, measured constants | you need a number and where it came from |

Meta files carry no schema — they are the direction, the order, the history and the rules.

## Then read what the task touches

| Task | Read |
|---|---|
| Orientation — where does a file belong | `architecture.md` |
| What comes next, in which order | `roadmap.md`; history in `journal.md` |
| **A body that moves — a person, a vehicle, an aircraft, a crate** | `body-format.md` — five declarations (segments, joints, contacts, force sources, medium) plus model, materials and brain. **SPEC ONLY, nothing built** |
| **Writing or changing a mod** — what a title may declare and what it may not | `mods.md` (the epistemic boundary: does this need knowledge no participant could have? yes → engine, no → mod) |
| What every core module must satisfy | `module-contract.md` |
| Renderer, camera, pass topology — the orchestrator | `render/renderer.md` |
| **One specific render pass** | `render/stages/<pass>.md` — the table above maps pass → file |
| The look bar, the frame budget, what may be spent | `render/visual-target.md` |
| LOD, popping, screen-space error, Nanite, TAA conditions | `render/lod.md` |
| What decides a ground class or a building type | `render/classification.md` |
| Species, templates, the 0–40 m stack, wind | `render/vegetation.md` |
| HUD/overlay backend, geometry buffer, font | `render/hud.md` — the backend only; the declaration surface it reads is **dead and unreplaced** |
| Frame determinism, the PNG oracle | `render/gpu-determinism.md` |
| World, terrain, tile streaming, `fb-tiles`, elevation | `world/terrain.md` |
| Weather data, GFS run, GRIB2 | `world/weather.md` |
| gym / native / wasm: what each client must be | `clients/clients.md` |
| The epoch and decay parameter, world persistence | `persistent-world.md` |
| How many actors, at what fidelity, how far out | `actor-scale.md` |
| The game as a view over the simulation | `player-layer.md` |
| Asset ladder, glTF, LODs, the modeller/critic pair | `assets.md` |
| Why expectations are DATA and not C++ | `testing.md` |
| Build targets, gates, host specifics | `build-and-ops.md` |

## The state of the tree (2026-08-06) — do not be surprised

A hard cut happened on this date and much of `doc/` has not caught up yet:

- **JSBSim, the F-16 and the MiG-29 are deleted.** So are their reference bases, their modules, the four
  NovaLogic titles, the `.fbm`/`.fbc`/`.fba`/`.fbh` formats and the combat tooling.
- **No `FB` prefix** on any file or class; the namespace is `outshine`. Exceptions: `world/terrain/` is
  a lowercase library, and `FBWX` survives as the name of a file format.
- **`core-lib` does not link** — ~23 files in the simulation/combat layer still name a deleted `Fdm`
  class. `render/` and `world/` are clean.
- **Declarations are JSON.** A hand-written parser for a bespoke line format is not written again.
- **`doc/` was swept on 2026-08-06** — `INDEX.md`, `vision.md`, `roadmap.md`, `core.md`,
  `architecture.md`, `mods.md`, `player-layer.md`, `actor-scale.md`, `build-and-ops.md`,
  `conventions.md`, `body-format.md`, `render/units-visual.md`, `render/renderer.md`, `render/hud.md`,
  `world/terrain.md` and `clients/clients.md` are current. `doc/render/clouds.md` still carries
  measurement records naming deleted scenarios; `persistent-world.md`, `testing.md` and `assets.md`
  still carry examples from the deleted era. Trust `CLAUDE.md`, `doc/INDEX.md` and this file over a
  stale passage, and say so rather than working from it.
- The class names in `doc/` are the current ones: **no `FB` prefix**. `FBWX` (a file format) and
  `FBDEM01` (a file magic) are not class names and are unchanged.

## Ground rules when applying this knowledge

- **Read `Spec` for the target and `State` for what exists — never assume a documented system is built.**
  Coverage is uneven on purpose.
- The standard is **believability, not fidelity**. The physics bar is explicitly *"enough for graphical
  representation"*. Believability is judged on three separate axes — **motion, decision, representation**
  (`body-format.md` §0.1).
- The anti-cheat structure is load-bearing for the *game*, not only the engineering: an actor sees only
  through its sensors and acts only through simulated systems. If a task seems to require reading the
  unit registry directly or writing state behind the simulation, the task is wrong, not the structure.
- **Do not invent a number.** Derived (with the formula), measured (with the measurement), or `[SET]`
  and named as such — otherwise it does not go in.
- The working rule is binding: change the **Spec** first, build until **State** meets it, then update
  State and Gaps and add one line to `journal.md`. Rejected approaches stay in Gaps with their
  measurements. There is no second list of open work.

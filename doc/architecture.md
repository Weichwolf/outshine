# Architecture

Who owns what, what links against what, and where a file belongs. **Why the cut is so** — what the tree
currently contains is in the tree, and what it once contained is in `git log`.

```
fb-tiles (worldwide DEM / OSM / imagery / weather / stars)  ──HTTP──▶  Client
                                                             = world + renderer + generators + AI
                                                               ONE process, wasm on Chromium/Edge
```

**Nothing is preloaded.** Every tile on demand, and from that follows the property the whole design is
built to keep: **every point on Earth is a valid start**, and the engine ships no world.

## One program, one entry point

`clients/Outshine` **is** the program. It holds the declared scene, the tables, `Render::Renderer` and
`World::World`, and it is the only object in the tree that turns data into a picture. A client is a
`main()` plus an output medium over it — it constructs the system, hands it a device, and renders.

Its surface is a handful of calls with a parameter object each (Core Guidelines `I.23`), and the
bring-up phases are an enumeration, not a pair of booleans (`ES.9`): bring-up is fallible and
asynchronous, so a constructor cannot finish it (`C.41`).

**A shared source list is not this contract and cannot stand in for it.** It proves that two
translations *compile*, never that they *show* the same thing — and for ten rounds it covered a forest
that existed in one entry point and not in the other. What enforces the contract is `verify-clients`: an
entry point may include nothing of `render/` or `world/`, its `main()` stays under 40 lines (`F.3`), and
the scene-building calls exist in exactly one translation unit.

**A bench is a layer over the system, never a mode inside it.** A bench is a viewer with unusual wishes;
a bracket over the exposure, a turntable, a CSV are properties of the observation, not of the world.
Inside `Outshine` they would put the browser's build under flags it can never set.

## Core and generators

| **Core** — the naked world | **Generators** — content from core data |
|---|---|
| terrain, DEM, streaming, the spatial index | vegetation: trees, undergrowth, grass |
| classification | buildings |
| atmosphere, clouds, stars, sun, moon | infrastructure |
| renderer, light, tone chain | water |
| | later: vehicles, actors |

A generator reads what the core knows — where the ground is, what class a place has, what the tables
declare — and produces content. **It is exchangeable by construction:** same input, different output.
The core never asks a generator a question, so nothing in `core/`, `world/` or `render/` may name one.

The cut exists because the two halves have different lifetimes. The naked world is what every scene
needs and changes rarely; content is what a scene *is*, and it will be rewritten many times. A boundary
that is only a folder name decays — this one is a contract, and it is enforced like the client contract.

**A generator runs continuously, per region, never once.** The goal is a worldwide sandbox in which a
viewer walks anywhere while everything streams, is generated and is placed. A contract that only carries
the initial load is already wrong at design time.

## Directories

| Directory | Responsibility |
|---|---|
| `sim/src/core/` | value types, log, telemetry, geodesy, keyframes, calendar, ephemeris. **Never points into any layer above it** |
| `sim/src/world/` | world, tile streaming, terrain maths, classification |
| `sim/src/generators/` | content from core data — vegetation, buildings, infrastructure, water |
| `sim/src/render/`, `render/stages/` | the WebGPU renderer, one class per shader |
| `sim/src/units/` | world entities and the registry that holds them |
| `sim/src/clients/` | entry point, app lifecycle, sink implementations |
| `tiles/` | fb-tiles, the tile server (its own Makefile, C) |
| `scenarios/` | the declared worlds. A scenario names a place, a clock, a weather and what runs |

Every `#include` points down the stack, and `verify-layers` is a machine-checked matrix, not a habit.

## The decisions that shape everything

**ONE spatial index, and it is derived.** A quadtree over the sphere with a vertical extent per node,
split vertically only where the content demands it. It answers *where* and **owns nothing**. Never a
second index beside it: two indexes are two truths, and that failure class has cost this project
repeatedly — two class paths, two DEM samplings, two class models.

*Why a quadtree with height and not an octree:* the only difference is whether the vertical axis
subdivides or merely bounds. Terrain, buildings and vegetation cling to the surface; an aircraft at 10 km
lies in the column of its cell, and a frustum cuts columns as reliably as cubes. An octree wins only when
dense content sits at many altitudes, fits a sphere badly, and is over 99 % air. The quadtree fits
because the slippy scheme *is* a sphere subdivision, and it fits the source, which speaks `z/x/y`.
Precision was never the tree's job.

**`float64` is the truth, `float32` is camera-relative, and the conversion happens ONCE and LATE.** ECEF
in double resolves about a nanometre at Earth radius, so millimetres are met by a factor of a million and
no cell origin is needed. An intermediate that computes in absolute `float32` has lost the precision
before the conversion could save it — and it is invisible, because the result lands half a metre off and
looks plausible.

**What the SIMULATION needs must exist without a GPU; what only the PICTURE needs may live on the GPU.**
Height at a point, footprint, class at a point and object positions are simulation and answer on the CPU.
The cluster DAG, the meshes and all appearance are picture. **One geometry, one predicate, two
evaluators** — the edge test a fragment runs is the same one a CPU query runs, shared code, agreeing to
within the refinement the GPU adds. That width is a stated number, never assumed zero.

**Three tiers over the OSM vectors, and refinement is ONE-WAY.** *AABB* on the CPU everywhere —
residency, frustum, what to fetch and evict. *Source polygon* on the CPU on demand, for point queries.
*Refinement* on the GPU only — extrusion, tessellation, scattering, cluster LOD. It never travels back,
so there is nothing to synchronise.

**Derived objects are stored by NEITHER side.** Scattering is a function of position: the GPU evaluates
it for every visible cell, the CPU for the one cell a body stands in. Same function, two callers, zero
bytes. The strong form is **the mesh is a FUNCTION, not a buffer** — a tree is species, seed and place,
and LOD becomes "evaluate fewer segments" instead of a second geometry. Per instance, stored vegetation
is measured in gigabytes; it was never an option.

**LOADING IS AN APPLICATION PHASE, NEVER A RENDERER STATE.** `Outshine` owns the phase and decides which
picture each tick wants; the renderer has one call for a frame and one for a progress fraction and no
notion of loading at all. `world/` knows of neither — it publishes progress and residency, and who turns
those into a screen is not its business. The sequence is the established one: declare the scene, fetch
the initial data while a progress frame is **rendered at full rate**, start the loop when it is there,
stream on beside it. The world is never drawn half-arrived, and there is no ceiling and no timeout on
the load — a server that stops answering is a fact about the server.

**The index is fed by the entity store and never authoritative.** Position is a component in `float64`;
the tree may not hold a position the store does not. Terrain is **not** an entity — tile geometry is
streamed, not simulated.

## An entity is a body

A body declaration lives in a scenario's own directory: segments, joints, contacts, force sources,
medium, plus model, materials and brain. The same format carries furniture, human, wolf, tank, aircraft,
and it must suffice for the depiction — not more.

## Server side

Exactly two lean containers: **`fb-tiles`** (:8081) serves DEM, OSM vectors, imagery, weather and the
star catalogue, and nothing else. **`fb-sim`** (:8080) hosts `web/` and collects log and telemetry from
the running client, so a run is fully reconstructible from the server log. No world process, no hub —
everything else runs in the client.

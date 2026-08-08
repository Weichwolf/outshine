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

### The contract

**A generator is a pure function `(Region, Ground) -> Yield`.** It knows no camera, no frame, no device;
`Generate` is `const`, which is what pays for running N regions concurrently without a lock. **The
scheduler knows where the eye is. The generator never does.**

`Region` is derived, not chosen: the tile address the vector source already keys on. Its seed follows
from the key, so determinism is a property of the place and not of the call order. A second grid beside
it would be a second index.

`Ground` is the core's read-only view — ground height, slope, class with its edge distance, the source
feature and its ring, the water level, the declared tables. It has no member that could deliver a camera,
a frustum, a frame index or a clock, so a generator cannot reach for one.

**Three products, three receivers, and the separation is the contract:**

| Product | Receiver | Carries | Never carries |
|---|---|---|---|
| **occupancy** | the engine — index and physics, and it is in the server target | bounds, a substitute body for contact, mass, a contact-material index | no triangles, no material, **no kind** |
| **draw** | the renderer, and it is *not* in the server target | clusters with a model-space error, instances, a material row | no bounds, no mass |
| **point query** | both, headless | what stands at this place, with no buffer existing anywhere | — |

**The engine knows only physics.** A trunk is a cylinder with a radius and a height, and that is all it
learns; a tree, a house and a car are the same thing to it. There is therefore **no taxonomy of content
in the engine** — every purpose a content enumeration might serve is either declared by the generator
itself or answered by the data. What the scheduler needs is a distance and a region size, and both are
statements about *place*, not about kind.

**Actors are not generators.** A generated yield is a pure function of its region: discard it and
regenerate it identically. An actor has state, so regeneration is not idempotent, and promising the same
interface for it would kill the purity that pays for the concurrency. An actor spawner shares the region
key and hands seed to the entity store, which owns the lifetime.

### The core dictates the render pipeline

**As few passes as possible, as many as necessary — and a generator has no say in it.** On tile-based
deferred hardware a full-screen pass costs its base price before it does anything, so cost follows the
*pass*, not the pixels. The pass count is therefore a deliberate decision of the core that one has to
argue against, never a by-product of how much content someone declared.

This is enforced by construction rather than guarded: **the material a generator supplies is a row of
numbers, and it carries no field that could switch a pipeline state** — no blend mode, no cull mode, no
shader body, no attribute layout. A generator supplies no shader source at all, so a region crossing
compiles nothing; it writes into buffers that already exist.

**A generator declares optical properties of a thing in the world** — reflectance, roughness, coverage,
transmission, tint, index of refraction, emission — and the core derives the state from them. Coverage
below one with no transmission is discarded against a threshold; transmission through something thin is
lit from both sides; transmission through something smooth and refractive is blended back to front. The
generator never names a blend mode, and the core never asks what the thing is.

**Transparency costs no pass, and that follows from the same property that forbids a visibility buffer.**
A blend state is a *pipeline* state: a second pipeline inside one render pass costs a state change and a
draw call, not the store-and-load from tile memory. Our scene pass is **forward** — every surface splices
the same lighting and binds the same light data — so there is no G-buffer for transparency to fall out
of. Engines that need a separate forward pass for it need one because their opaque path is deferred.

Three optical protocols therefore live inside the one scene pass: **opaque**; **coverage** for leaf,
needle, fence and grate, discarded in the opaque part and writing depth; and **transmission** for glass,
blended after the opaque part with depth writes off. Foliage needs no blending at all — it is a binary
coverage mask cut in the fragment, which is why it works in a shadow map.

**Water is not a transparency case.** Depth below the surface is the water level minus the ground height,
and the core owns both exactly — so the body colour follows analytically from that depth and the rest is
reflection. No blended fragment, more accurate than blending, and it reuses the numbers physics needs
anyway.

Sorting stays cheap because the *amount* is a picture decision, not a hope: at the resolution where light,
colour and silhouette decide, a distant window is a dark reflecting rectangle, not a view into a room.
Real blending is a near-field case — the windscreen of the vehicle one sits in. Above a declared budget of
blended clusters the **core** changes its resolution method; a generator never can.

**Geometry consolidates before it is drawn.** Every generator delivers into one cluster format with a
model-space error, and one screen-space-error rule selects across all of them. There is one ladder, not
one per kind of content — a second ladder for a second kind of content is the same mistake as a second
index.

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

## The layering is a build target, not a lint

Higher layers depend on lower ones and never the reverse. **The enforcement is that core, world,
generators and physics compile without the renderer** — see the persistent server in
[`vision.md`](vision.md). A breach shows up as a target that no longer builds, not as a message someone
has to read.

A hand-maintained checker is a second truth about the structure, and a second truth decays: the one this
replaced grew a rank table full of layers that no longer existed while letting the single edge that
mattered pass. What a build cannot express is small and keeps a small check: a namespace per layer, and
a forward declaration passed through opaquely.

**One consequence is structural.** The object that owns both world and renderer cannot itself be in the
server target. It splits: a simulation half that all targets share, and a picture half over it.

## An entity is a body

A body declaration lives in a scenario's own directory: segments, joints, contacts, force sources,
medium, plus model, materials and brain. The same format carries furniture, human, wolf, tank, aircraft,
and it must suffice for the depiction — not more.

## Server side

Exactly two lean containers: **`fb-tiles`** (:8081) serves DEM, OSM vectors, imagery, weather and the
star catalogue, and nothing else. **`fb-sim`** (:8080) hosts `web/` and collects log and telemetry from
the running client, so a run is fully reconstructible from the server log. No world process, no hub —
everything else runs in the client.

# Architecture

How Outshine is to be built. Decisions and constraints only — the tree holds what exists, `git log` holds
what existed.

```
fb-tiles (DEM · OSM · imagery · weather · stars)  ──HTTP──▶  ONE client process, wasm on Chromium/Edge
```

Nothing is preloaded. Every tile on demand. **Every point on Earth is a valid start.**

## Targets

| Target | Contains | Purpose |
|---|---|---|
| `wasm` | everything | the product |
| `world` | everything except `render/` | the persistent server, and the enforcement of the layering |
| `walk` | everything | native oracle — falls once `wasm` carries every declared run |

## The machine

**wasm32 plus WebGPU is a virtual console, and that is the limit.** wasm32 gives a hard 4 GB address
space; WebGPU gives one feature set with no vendor extensions; and the heap is **fixed, not growing** —
a declared budget rather than whatever the machine happens to have.

Fixed is affordable because almost everything is procedural and the rest is streamed: what a fixed heap
must hold is the resident working set, never the world. Fixed is also *necessary* once threads are
plural — growing shared memory can invalidate another thread's view, which is the same class of rare,
unreproducible defect the rest of this document is built to avoid.

Every per-thread stack comes out of that same budget. A thread count and a memory budget are one
decision, not two.

**Layering is enforced by the build, never by a checker.** A module compiles with the include set of its
layer and below. An upward include is a compile error; a breach shows as a target that stops building.

## Directories

| Directory | Holds | May include |
|---|---|---|
| `core/` | value types, log, telemetry, geodesy, keyframes, ephemeris, calendar | itself |
| `world/` | tile streaming, terrain, classification, water level | `core` |
| `generators/` | content from core data | `core`, `world` |
| `render/`, `render/stages/` | the WebGPU renderer, one class per shader | `core` |
| `clients/` | entry point, app lifecycle, sinks | all |
| `tiles/` | fb-tiles, C, its own Makefile | — |
| `scenarios/` | declared worlds | — |

`generators/` never includes `render/`. `core/` never points up.

## One program, one entry point

- One object owns world and renderer and is the only thing that builds a scene.
- A client is `main()` plus an output medium. `main()` stays under 40 lines (`F.3`).
- An entry point includes nothing of `render/` or `world/`.
- Scene-building calls exist in **exactly one** translation unit.
- The object owning both world and renderer **splits**: a simulation half every target shares, a picture
  half over it. The server target links only the first.
- A bench is a layer over the system, never a mode inside it.
- Bring-up phases are an enumeration, not booleans (`ES.9`); a constructor cannot finish fallible
  asynchronous bring-up (`C.41`).

**A shared source list proves that two translations compile, never that they show the same thing.**

## Core and generators

| Core — the naked world | Generators — content |
|---|---|
| terrain, DEM, streaming, spatial index | vegetation: trees, undergrowth |
| classification | buildings |
| **where water is and its level** | **how water looks** |
| atmosphere, clouds, stars, sun, moon | infrastructure |
| renderer, light, tone chain | later: vehicles |

Grass stays a function in the ground fragment — already the end state, not a generator.

### The contract

**A generator is a pure function `(Region, Ground) -> Yield`.** `Generate` is `const`; that is what pays
for N concurrent regions without a lock.

| | |
|---|---|
| `Region` | the tile address the vector source already keys on. Seed derived from the key, so determinism is a property of place, not of call order. **No second grid.** |
| `Ground` | ground height, slope, class + edge distance + runner-up, source feature and ring, water level, declared tables. Read-only, headless, no raw pointer, no callback |
| **not in `Ground`** | camera, frustum, view direction, frame index, clock, LOD level, view distance, renderer, device, sun position, weather |

| Product | Receiver | Carries | Never carries |
|---|---|---|---|
| **occupancy** | engine — index, physics. **In the server target** | bounds, substitute contact body, mass, contact material | triangles, material, **kind** |
| **draw** | renderer. **Not in the server target** | clusters with model-space error, instances, material row | bounds, mass |
| **point query** | both, headless | what stands here, with no buffer existing | — |

**The engine knows only physics.** A trunk is a cylinder with a radius and a height. A tree, a house and
a car are the same thing to it. **No content taxonomy exists in the engine** — the scheduler needs a
distance and a region size, both statements about place.

**Actors are not generators.** State makes regeneration non-idempotent and kills the purity. An actor
spawner shares the region key and hands seed to the entity store.

**A generator runs continuously, per region.** A contract carrying only the initial load is wrong at
design time.

### Geometry

| | |
|---|---|
| Vertex layout | one, core-defined: `pos3 + nrm3 + uv2` |
| `uv` | **metres**, never 0..1 |
| Positions | ECEF offsets from a declared anchor |
| Unit | prototype + instances, never geometry per instance |
| LOD | **one** ladder: `Sse(c) ≤ τ ∧ Sse(parent) > τ`. Never one per kind of content |
| Crack-freedom | within a generator's own soup only |

## The core dictates the render pipeline

**As few passes as possible, as many as necessary — a generator has no say.** A full-screen pass costs
its base price before doing anything: cost follows the pass, not the pixels. A further pass must beat
that price before it exists.

**Enforced by construction, not guarded:** a generator's material is a row of numbers with **no field
that can switch a pipeline state** — no blend mode, no cull mode, no shader body, no attribute layout.
A generator supplies no shader source, so a region crossing compiles nothing.

| Generator declares | Core derives |
|---|---|
| coverage < 1, no transmission | discard against threshold, opaque part, writes depth |
| transmission through something thin | lit both sides, no back-face cull, transmission term |
| transmission, smooth, refractive | blended after the opaque part, depth writes off, back to front |
| emission | contribution to the light list |

**Transparency costs no pass.** A blend state is a *pipeline* state — a second pipeline inside one render
pass costs a state change and a draw call, not the store-and-load from tile memory. The scene pass is
**forward**, so there is no G-buffer for transparency to fall out of; engines needing a separate pass
need one because their opaque path is deferred.

**Water is not a transparency case.** Depth = water level − ground height, both owned exactly by the
core. Body colour follows analytically; no blended fragment, and it reuses what physics needs anyway.

Sorting stays cheap because the amount is a picture decision: at comparison resolution a distant window
is a dark reflecting rectangle, not a view into a room. Above a declared budget of blended clusters the
**core** changes its resolution method.

**No pipeline creation while playing.** A mod's own-entity shader compiles during loading. A violation is
an error, not a latency someone has to hunt.

## Loading

**Loading is an application phase, never a renderer state.** Initial load holds the world back and shows
progress; the renderer runs at full rate throughout and has one call for a frame and one for a progress
fraction. `world/` knows of neither — it publishes progress and residency.

**Streaming during play never stalls the pipeline.** Fetch and decode run beside the render thread,
upload per frame is a budget, a tile becomes visible when complete — never half. **A hitch on stream-in
is a defect.**

No ceiling and no timeout on the load: a server that stops answering is a fact about the server.

## The decisions that shape everything

**ONE spatial index, derived.** A quadtree over the sphere with a vertical extent per node, split
vertically only where content demands it. It answers *where* and **owns nothing**. Never a second index:
two indexes are two truths.

*Not an octree:* content clings to the surface, an aircraft at altitude lies in the column of its cell, a
frustum cuts columns as reliably as cubes. An octree fits a sphere badly and is over 99 % air. The slippy
scheme *is* a sphere subdivision, and the source speaks it.

**`float64` is the truth, `float32` is camera-relative, one conversion, late.** ECEF in double resolves
about a nanometre at Earth radius. An intermediate computing in absolute `float32` has lost the precision
before conversion could save it — invisibly, because the result lands half a metre off and looks
plausible.

**What the simulation needs exists without a GPU; what only the picture needs may live on the GPU.**
Height, class, footprint, water level and occupancy answer on the CPU with no device. **Appearance is
never a precondition for physics.**

**One geometry, one predicate, two evaluators.** The edge test a fragment runs is the edge test a CPU
query runs — shared code, agreeing to within the refinement the GPU adds, and that width is a stated
number.

**Three tiers over the OSM vectors, refinement ONE-WAY.** AABB on the CPU everywhere; source polygon on
the CPU for point queries; refinement on the GPU only. It never travels back, so nothing synchronises.

**Derived objects are stored by neither side.** Scattering is a function of position: the GPU evaluates
it per visible cell, the CPU for the one cell a body stands in. Same function, two callers, zero bytes.
**The mesh is a function, not a buffer.**

**The index is fed by the entity store and never authoritative.** Terrain is not an entity.

## An entity is a body

Segments, joints, contacts, force sources, medium, plus model, materials, brain. One format for
furniture, human, wolf, tank, aircraft. It must suffice for the depiction — not more.

## Server side

**`fb-tiles`** (:8081) serves DEM, OSM vectors, imagery, weather, stars — nothing else. **`fb-sim`**
(:8080) hosts `web/` and collects log and telemetry, so a run is reconstructible from the server log.
No world process, no hub.

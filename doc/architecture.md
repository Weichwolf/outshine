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

**wasm32 plus WebGPU is a virtual console, and that is the limit.** wasm32 gives a hard 4 GiB address
space and no bounds checks; WebGPU gives one feature set with no vendor extensions. Optimise against that
machine from the start: no allocation in the hot path, flat arrays in linear memory, few crossings of the
JS boundary. **WebGPU computes, wasm administers.**

**Device memory has no declared ceiling of its own, and on the targets that bind there is no separate
pool at all.** A discrete card is effectively unbounded at our scale, and the graphics API caps single
buffers rather than totals. But the machines the performance budget names — mobile silicon, consoles —
share one physical memory between processor and device. There it is **one budget with two consumers**,
not two budgets: resident geometry measured in hundreds of megabytes comes out of the same allowance as
the heap, and a console browser gets about a gigabyte for the whole process.

**So the device side is accounted like the heap: every resident pool reports its bytes and evicts against
a declared figure.** Not because an API forces it, but because on the binding target it is the same
memory — and the largest resident item today has a visibility rule and no budget.

**The heap is fixed.** Not because growth is unsafe — shared memory cannot be detached, and the generated
glue guards every access — but because **a growing heap is not a declared budget, and without a budget no
consumer has a reason to limit itself.** Fixing it also deletes that per-access guard, including one
instance per copied byte on our own path.

**A fixed heap is a ledger.** Every pool reports its bytes, or it is a leak with a name. A cache capped in
*entries* does not know its own footprint when an entry ranges over three orders of magnitude.

**A failed allocation is a decision, not a footnote.** On an elastic path — caches, work in flight,
generator output, resident geometry — it means evict, retry once, then refuse that piece of world the way
a missing tile is already refused. Everywhere else it is a loud abort naming the item and the bytes,
because a null dereference there would hide a budget error. The default toolchain behaviour is the
opposite of this and must be turned off, or every null check in the tree is dead code that looks like
handling.

**memory64 is not the way.** wasm32 on a 64-bit host reserves its whole address space and thereby
eliminates bounds checks; memory64 cannot and pays for every access. Only worth it above 4 GiB, and the
tightest target is far below.

**Every per-thread stack comes out of the same heap, so a thread count and a memory budget are one
decision.** Stacks are set per purpose: a thread that blocks on the network holds kilobytes, a thread
that meshes holds megabytes. The default applies one size to both.

**The streamer needs a byte budget and evicts against it.** A cache capped in *entries* does not know its
own footprint when an entry ranges from a few hundred bytes to a few hundred kilobytes, and then a failed
allocation is a crash where it should have been an eviction.

**The binding constraint is the weakest target, and it binds on cores as well as memory.** A console
browser is an *app*, not a title, and app budgets there are a fraction of the machine — measured in a
gigabyte for the whole process tree, with a handful of shared cores. A thread budget derived from a
developer machine's core count does not survive it.

## Threads

| Behaviour | Form | Count from |
|---|---|---|
| blocks, no CPU (network, audio) | **dedicated** | the protocol's connection limit per origin, not preference |
| computes (decode, mesh, generation) | **pool** | hardware concurrency, less the threads that must stay free |
| one job in flight by construction | dedicated, one | the structure itself |

**Every long-lived thread is created at bring-up, from the main thread, before the frame loop.** The pool
size is exactly that number, and creating a thread at runtime is a hard failure rather than a silent
one — thread creation proxies synchronously to the main thread, which is where a deadlock would come
from. Blocking I/O never causes one.

**Audio is not one of these threads.** The audio worklet belongs to the browser, gets its stack handed to
it, and may neither block nor allocate in its callback.

**Two levels of timeout, and they do not contradict each other.** A *request* has a limit, because it
bounds how long a thread stays occupied. The *load* has none — a server that stops answering is a fact
about the server, and a client that gives up turns a slow server into permanently missing world.

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

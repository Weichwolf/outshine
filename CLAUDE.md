# Outshine

> **A wasm app at 720p60 in an A18-Pro-class budget: an OSM-based open world, LLM-driven, built purely
> from declarative `scenarios/`. CryEngine is the engine to match, Kingdom Come: Deliverance the world
> and its vegetation, GTA 5 the built world and the verbs — walk, drive, fly.**

The world is **loaded, not modelled**: terrain, land cover, buildings, vegetation, weather and the night
sky come from `fb-tiles`, so every point on Earth is a valid start. **One physics system** carries
walking, driving, flying and swimming. An **epoch and decay dial** dresses the same geometry. The actors
**think**. The setting is post-scarcity — modern infrastructure, lush nature.

**The repository speaks one language: English.** Code, comments, documents, commit messages.

## Where things live

| Place | Content |
|---|---|
| **the code** | what the thing can do. **Only correct work is committed** |
| **`git log`** | what was. No journal |
| [`doc/requirements.md`](doc/requirements.md) | **the scope** — one line per feature with a box, and a ticked line **names the file that implements it**. It must be fully implemented, and it is extended or shortened **only on the owner's request** |
| [`doc/todo.md`](doc/todo.md) | **the current work item**, with a status header. Short |
| `.claude/agents/` | **`engine-developer`** builds and measures · **`engine-architect`** designs and judges, read-only |
| this file | purpose, shape and the rules. At most **200 lines** |

## Stance

**The owner's comments outrank everything.** The bar is CryEngine's level out of tile data alone, and
**the way is the goal** — a round that learned something is a good round.

**The frame is fixed, the code is in flux.** Fixed are **wasm32 and WebGPU** — a virtual console, and its
limits are the limits. Everything else is material: formats, directories, algorithms, interfaces, build,
tools. Nothing here is a possession.

**Something missing is a task, not a limit.** "That number does not exist" ends with "so the tool gets
built". Distinguish **not measurable** from **not yet measured** — the second has a cost, not a boundary.

**Be inventive, build on what is proven.** The established way is the starting point and a deviation
needs its reason beside it.

## Principles

1. **Purely declarative, and the language is JSON.** A scenario brings no `.cpp` and no world.
2. **Appearance is generated, never authored — and textures are normal.** Bark, leaf, façade, ground
   detail: **produced by a generator in this tree**, cached, sampled. Forbidden is a **file somebody
   painted**, because nothing can recompute it; measured raster data (DEM, imagery, stars) is admissible
   for the mirror reason. **The test: can this be recomputed from something we own?** A bake is normal —
   generate, then cache. What a bake does *not* buy is immunity: once a function is a raster it gets mip
   levels and filtering like any other.
3. **The physics is our own and declarative.** Segments, joints, contacts, force sources, medium, plus
   model, materials, brain — the same format carries furniture, human, wolf, tank, aircraft.
4. **Outshine knows everything, a scenario knows only what it knows.** *Does this need knowledge no
   participant could have?* Yes → engine, otherwise scenario. With LLM actors this is load-bearing: a
   brain sees only through sensors, acts only through simulated systems, and a contact carries no
   identity.
5. **Everything runs IN the client.** One process, one address space, WASM like native.
6. **Two server containers only:** `fb-tiles` (`tiles/`, :8081) and `fb-sim` (`sim/`, :8080). The tile
   server delivers DEM, OSM, imagery, weather and stars — nothing else.
7. **The mathematics is deterministic.** If pace decides the result, the coupling is a bug.

## Shape

**Six directories under `sim/src/`.** `core` is value types and the naked world's mathematics ·
`core/io` is log, telemetry and the probes, so `core` is I/O-free **by directory** · `world` is
streaming, terrain and classification · `generators` turn what the core knows into content · `render` is
the WebGPU renderer, one class per shader · `clients` is the entry point.

**Layering is the build, never a checker.** `make world` links no `render/`; a generator translation unit
compiles with `-Isrc/core -Isrc/generators` and nothing else, so `Renderer`, `World`, `Log` and the
streamer **have no name** there. A breach is a compile error. Gates: `verify-clients` ·
`verify-generators` · `verify-world` · `verify-types`.

**One program, two translations, one entry point.** `clients/Outshine` owns world and renderer and is the
only thing that builds a scene; a client is `main()` plus an output medium. Targets: `walk` (native
oracle) · `wasm` (browser) · `world` (headless server) · `image` · `up`.

**A generator is a pure function `(Region, Ground) → Yield`.** `Generate` is `const noexcept`, which pays
for N concurrent regions with no lock. `Ground` carries height, slope, class with edge distance and
runner-up, water level and the declared tables — resolved values, never a callback. Three products:
**occupancy** to the engine (bounds, substitute contact body, mass — no triangles, no material, **no
kind**), **draw** to the picture, **point query** to both. The engine has no content taxonomy: a trunk is
a cylinder. A thing drawn where nothing stands is unspellable, because a draw needs an id only a claim
mints — and grass is not a claim, it is a field evaluated at draw time with no record ever created.

**The core dictates the pipeline.** A generator's material is a row of numbers with **no field that can
switch a pipeline state**; the core derives discard, two-sidedness, transmission, blending and emission
from what is declared. **A further pass must beat its base price**, and the price is **0.35–0.5 ms** —
a 1280×720 RGBA16F ping-pong moves 14.75 MB, which is 1.5–3 % of the frame before one instruction.

**One geometry stage over one cluster cut, one LOD ladder** — never one per kind of content. Vertex
layout `pos3 @0 · uv2 @12 · nrm3 @20`, 32 B, with `pos3 · nrm3` 24 B as a declared second where no uv
exists. `uv` in metres, except a façade, which encodes style and storeys (`core/FacadeUv.h`).

**Initial load and streaming are two different things.** The load holds the world back and shows
progress. **What streams in during play never stalls the pipeline**, and a tile becomes visible when it
is complete — never half.

## Measurement

**Every stage measures itself, continuously, into the telemetry.** There is no measurement mode; a bench
is a declared run. Performance is a **distribution over a moving camera** — p50/p95/p99, never a mean,
never a minimum. Every line carries scenario, scene, wasm hash and browser version.

**Say which kind a number is.** *Decidable* — a geometric invariant, true or false with no reference;
check these first, they are the cheapest evidence and need no argument. *Consistency* — two parts of this
tree agree, which is most of what gets measured here. *Plausibility*. *Correctness* — checked against
something outside. **Another digit of internal agreement is worth less than the first external check.**

**A run-wide average is not a baseline** when the quantity drifts. Randomise order within a block —
ABBA's arm contrast is the quadratic orthogonal-polynomial contrast over four positions, so it aliases
curved drift into the effect and more blocks make a false positive *more* significant. Publish the
instrument's own floor beside the result.

**When performance work happens is a trigger, not a schedule.** At 720p60 nothing is optimised. When
720p30 can no longer be **held** — the floor, p99 under 33 ms, not the mean — it is optimised back.

**The still is the comparison resolution, not the acceptance.** Popping, ghosting, a hitch on stream-in
and a scatter that ends at a radius are only decidable in motion. **Appearance is judged by eye from the
image**; a number decides whether the frame floor holds, never whether it looks right.

## Hard rules

- **No scattered output.** `Log` for events, `TelemetryBus` for state.
- **Every number carries its origin** — derived, measured or `[SET]` — with unit and frame of reference.
- **What is replaced is deleted in the same round.** A fallback is a dead path; diagnostics are not.
- **Every statement has exactly one place.**
- **There is one version.** No quality levels during basic development.
- **Development is strictly serial** — one agent in the tree.
- **After every accepted step there is a commit.**
- **Comments never describe what the code does.** One task remains: the local non-obvious *why*, one
  line. **A name that needs a comment is the wrong name.**
- `core/` never points up. Peers never call each other.
- **The C++ Core Guidelines apply** (below); what follows are the house deviations.
- C++17, **no prefix**, PascalCase, **`namespace outshine`**, one class per file. Exceptions:
  `world/terrain/` (C-ABI, shared with `tiles/`) and `FBWX` (a format name).

## References

**Stroustrup/Sutter, [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines) — BINDING.** They
decide ownership, lifetime, interface and style; a deviation is a defect until its reason stands beside
it. Note `ES.9` is *Avoid ALL_CAPS names*; the enumeration rule is `Enum.2`/`Enum.3`, and the Guidelines
carry **no** dead-code rule — cite this file for that.

| Field | Titles |
|---|---|
| **Engine** | Gregory, *Game Engine Architecture* 3e · Lengyel, *Foundations of Game Engine Development* I–III |
| **Rendering** | Akenine-Möller, *Real-Time Rendering* 4e · Pharr, *Physically Based Rendering* 4e · Lagarde/de Rousiers, *Moving Frostbite to PBR* |
| **Procedural** | Ebert/Musgrave/Perlin/Worley, *Texturing & Modeling* — the canon for "appearance is a function" |
| **C++** | Meyers, *Effective Modern C++* · Pikus, *The Art of Writing Efficient Programs* |
| **Physics** | Ericson, *Real-Time Collision Detection* · Bridson, *Fluid Simulation* |
| **Implementations** | **CryEngine** — the level to match · **Kingdom Come: Deliverance** — the world and its vegetation, on a PS4's budget · **GTA 5** — the built world and the verbs · SpeedTree · OSM viewers · Microsoft Flight Simulator |

## Host

emsdk in `~/Git/emsdk`, `nproc` shim in `~/.local/bin`. Containers: `podman machine start`, then
`tiles/up.sh` (:8081), `sim/up.sh` (:8080). Native builds: `sim/vendor/.compat-headers`; **macOS has no
`timeout(1)`**.

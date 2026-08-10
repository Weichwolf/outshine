# Outshine

> **A game engine that runs at 720p60 within an A18-Pro-class resource budget, with the technology of Days
> Gone and Horizon Forbidden West, in which games like Witcher 3, Fallout 4 and GTA 5 can be reproduced —
> optically, in content and functionally — purely through declarative `scenarios/`. The basis of the
> procedural world is OSM, elevation, weather and star data from the tile server. Through LLM integration
> every entity is intelligent and the game world is dynamic.**

An engine **cut for one machine** — not for developers and artists. Scenario in, playable game out. Four
build decisions: the world is **loaded from OSM** instead of modelled · **one** physics system carries
walking, driving, flying and swimming · an **epoch and decay dial** dresses the same geometry · the actors
**think**. [`doc/vision.md`](doc/vision.md)

**OSM is the world, not one data source among several.** Terrain, land cover, structures, infrastructure
and vegetation distribution come from the same vectors — which is why the drawn thing and the classified
thing are **the same line by construction**.

**The repository speaks one language: English.** Code, comments, documents, commit messages.

## Stance

**The bar is a world sandbox at Unreal level, out of what the tile server delivers alone** — it holds for
everything visible. **The owner's comments outrank everything.** And **the way is the goal**: months, no
acceptance date — a round that learned something is a good round even without a delivery.

**Be inventive, build on what is proven.** The state of the art is written down — see `## References`. The
established way is the starting point, **a deviation needs a reason**, and the reason stands next to it.

**The frame is fixed, the code is in flux.** Fixed are **wasm32 and WebGPU** — a virtual console, and its
limits are the limits. Everything else in the tree is **material**: formats, directories, algorithms,
interfaces, build, tools. We are building something new; nothing here is a possession, and what the vision
requires gets built or changed.

**Something missing is a task, not a limit.** "That number does not exist" ends with "so the tool gets
built", never with "so it cannot be decided". Distinguish **not measurable** (the thing yields no number)
from **not yet measured** (the tool is missing) — the second has a cost, not a boundary. When a design
snags on something that exists, the question is not "how do I work around it" but **"should the existing
thing change"**, together with what that costs.

**No blank cheque:** every *decision* is revisable — the duty to measure, the origin of every number, and
deleting what is superseded in the same round are not. Those are the tools revision is done with.

## Where things live

| Place | Content |
|---|---|
| **the code** | what the thing can do. **Only correct work is committed** — there is no second place where correctness is claimed |
| **`git log`** | what was. No journal, no history in a file |
| [`doc/vision.md`](doc/vision.md) | what for, and where the bar sits |
| [`doc/architecture.md`](doc/architecture.md) | how Outshine is to be built — decisions, not prose |
| [`doc/todo.md`](doc/todo.md) | the next steps, in order |
| `.claude/agents/` | **`engine-developer`** builds and measures · **`engine-architect`** designs and judges, read-only |
| this file | the rules. At most **200 lines** |

`doc/` holds **three** files — purpose, shape, order — and gets no fourth. A document describing what the
code **does** is the same thing in two languages, and the second one can lie. A rejected attempt is not
preserved: the starting position keeps moving, and a conserved measurement misleads a later round.

**Comments almost entirely disappear.** ONE task remains: **the local why at the decision point**, one
line. Never what the code does. **A name that needs a comment is the wrong name.**

## Principles (not negotiable)

1. **Purely declarative, and the language is JSON.** A title brings **no `.cpp` and no world**. JSON is
   schema-checkable, diffable and **generatable**; a bespoke format would be a parser nobody ordered.
   Shaders for a title's own appearance are allowed — appearance is not knowledge.
2. **The engine is texture-free.** Admissible are only the **cache of a computable function** (sky and
   transmittance LUTs) and **measured data that is a raster by nature** (DEM, imagery, stars) — **never
   authored appearance**; there are no artists. Side benefit: mip dependence, zoom pops, sampling grids
   and filter artefacts **cannot occur in a function**.
3. **The physics is our own and declarative.** Five parts — segments, joints, contacts, force sources,
   medium — plus model, materials, brain; the same format carries furniture, human, wolf, tank, aircraft.
   **It must suffice for the depiction, no more.**
4. **Outshine knows everything, a scenario knows only what it knows.** Checkable: *does this need
   knowledge no participant could have?* Yes → engine, otherwise scenario. **With LLM actors this is the
   load-bearing rule**: a brain sees only through sensors, acts only through simulated systems; a contact
   carries no identity.
5. **Everything runs IN the client.** Physics, world and picture are one process, one address space, WASM
   like native.
6. **Server-side only two containers:** `fb-tiles` (`tiles/`, :8081) and `fb-sim` (`sim/`, :8080). The tile
   server delivers DEM, OSM, imagery, weather and the star catalogue — nothing else.
7. **The mathematics is deterministic.** If pace decides the result, the coupling is a bug.

## Architecture and build

`fb-tiles` delivers over HTTP to the client from principle 5. `sim/src/` has six directories: `clients`,
`core`, `generators`, `render`, `units`, `world` — **core** is the naked world (terrain, classification,
atmosphere, clouds, celestial bodies, renderer), **generators** turn that into content (vegetation,
structures, infrastructure, water) and are exchangeable because they read the same input.

**ONE program, two translations, ONE entry point.** `clients/Outshine` owns world and renderer and is the
only thing that builds a scene; a client is `main()` plus an output medium over it — **`gpu_walk`**
(native, frame oracle, bench `WalkBench`) and **wasm** (browser, `Walker`). Both receive two words: which
scenario, which scene. A shared source list alone covered the drift for ten rounds: it proves that both
*compile*. `verify-clients` proves both are entry points over one scene builder — **it does not prove
they show the same thing, and measurably they do not**: the browser punches triangular sky wedges through
a distant tree line the native oracle draws closed. **Only a picture compared across both clients decides
that**, and nothing does it yet.

Building happens only through make targets. `sim/`: `walk` | `wasm` | `worker` | `image` | `up`. Gates:
`verify-layers` | `verify-clients`. **The wasm client builds in EVERY round.** Warnings
are errors (`-Wall -Wextra -Wpedantic`) · a frame proof or a measurement · vendor read-only.

**Every stage measures itself, continuously, and the result goes into the telemetry** — tile fetch,
decode, upload, residency, every pass, the frame. There is no measurement mode: a bench is a declared run,
not a different code path. Analysis happens over the time series; a tile arrival inside a frame is a
**field**, not a reason to discard the run. **Performance is a distribution over a moving camera** —
p50/p95/p99, never a mean, never a minimum. Every line carries scenario, scene, wasm hash and browser
version. **A run-wide average is not a baseline** when the quantity drifts across the run.

**Above all three sits what is decidable.** Geometry has invariants — a normal is unit length or it is
not, an edge of a closed body belongs to exactly two triangles or it does not — and they are true or
false without any reference and without taste. They are the cheapest evidence in the tree and the only
kind that needs no argument. Check them first.

**There is no verified correct yet, and a measurement here mostly says the tree agrees with itself.**
Two evaluators that call one function agree by construction; if that function is wrong they are wrong
together and nothing in the tree can see it. So name which kind a number is — **consistency** (two parts
of this tree agree), **plausibility** (it has the right order and sign for the physics), or
**correctness** (it was checked against something outside: measured data, a published dimension, a
photograph, a survey point). Only the third is evidence about the world, and most of what gets measured
here is the first. Spend accordingly: another digit of internal agreement is worth less than the first
external check of the same quantity, and an argument about the last digit of a quantity nothing outside
has ever confirmed is an argument about nothing.

**Initial load and streaming are two different things.** The initial load holds the world back and shows
progress — that is what the loading screen is for; Outshine does not warm up. **What streams in during
play NEVER stalls the pipeline**: fetch and decode run beside the render thread, upload per frame is a
budget, and a tile becomes visible when it is complete — never half. **A hitch on stream-in is a defect**,
not a law of nature, and exactly the kind a still frame does not show.

**The still is the comparison resolution, not the acceptance.** What is tuned against a photograph must be
fast **and** flawless in motion — and the most expensive defects are exactly the ones a single frame
cannot show: popping at an LOD change, a scatter that ends at a radius, ghosting and smear in the temporal
filter, a hitch on stream-in, shading that jumps at a mesh change. **A still frame does not prove them** —
a moving capture, or it counts as unverified.

## Hard rules in the code

- **No scattered output.** `Log` for events, `TelemetryBus` for state. Core is I/O-free.
- **Every number carries its origin** — derived, measured or `[SET]`. Unit and frame of reference are part
  of that origin.
- **What is replaced is deleted in the same round** — a dead path that can still fire is worse than one
  line too many. Fallbacks are dead paths; diagnostics are not.
- **Every statement has exactly one place.** An argument standing in both a header and `doc/` will drift.
- **There is one version.** No quality levels during basic development.
- **Development is strictly serial** — one agent in the tree. Separating files protects against
  overwriting, not against interference: tree and compiler are shared.
- **After EVERY accepted step there is a commit** — "git will bring it back" holds only once it is in.
- `core/` never points up. Peers never call each other.
- **The C++ Core Guidelines apply** (`## References`); what follows are only the house deviations.
- C++17, **no prefix**, PascalCase, **`namespace outshine`**, one class per file. Exceptions:
  `world/terrain/` (C-ABI library, `tiles/` calls the same DEM decoder) and `FBWX` (a format name).

## References

**Stroustrup/Sutter, [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines) — BINDING.** They
decide ownership, lifetime, interface and style; a deviation is a defect until its reason stands next to
it, and against a house opinion they win. The rest is canon, not law — a starting point rather than an
invention.

| Field | Titles |
|---|---|
| **Engine** | Gregory, *Game Engine Architecture* 3e · Lengyel, *Foundations of Game Engine Development* I–III |
| **Rendering** | Akenine-Möller et al., *Real-Time Rendering* 4e · Pharr et al., *Physically Based Rendering* 4e · Lagarde/de Rousiers, *Moving Frostbite to PBR* |
| **Procedural** | Ebert/Musgrave/Perlin/Worley, *Texturing & Modeling* — the canon for "appearance is a function", including its limits |
| **C++** | Meyers, *Effective Modern C++* · Pikus, *The Art of Writing Efficient Programs* |
| **Physics** | Ericson, *Real-Time Collision Detection* · Bridson, *Fluid Simulation for Computer Graphics* |
| **Implementations** | AAA titles · SpeedTree · OSM viewers (OSM2World, F4map) · Microsoft Flight Simulator |

## Host

emsdk in `~/Git/emsdk`, `nproc` shim in `~/.local/bin`. Containers: `podman machine start`, then
`tiles/up.sh` (:8081), `sim/up.sh` (:8080). Native builds: `sim/vendor/.compat-headers`; **macOS has no
`timeout(1)`**. Tree template: `~/Git/wasm-tree` (16 species as JSON).

# Architecture

## Spec

The floor plan: who owns what, what links against what, and where a file belongs.

| Contract | Acceptance / measurement anchor |
|---|---|
| The two clients link ONE source list | `PEDESTRIAN_SRCS` + `render/` + `world/` in `sim/Makefile`; a PNG out of `gpu_walk` is therefore a statement about the browser |
| WASM is a cross-compile of the same source list, never a second architecture | `make -C sim wasm` |
| Server-side there are exactly two lean containers | `fb-tiles` (:8081, tile API) and `fb-sim` (:8080, web host). No world process, no hub |
| Nothing is preloaded | every tile on demand — **every point on Earth is a valid start** |
| `src/`, `doc/` and `test/` carry the same directory tree | `make -C sim verify-trees`; an orphan is a named hole |
| Every `#include` points DOWN the stack | `make -C sim verify-layers`, a machine-checked matrix |

## State

**The tree is `clients/ core/ render/ render/stages/ units/ world/ world/terrain/` and nothing else.**
The simulation layer that named the deleted `Fdm` class — `sensors/ weapons/ pilot/ modules/ missions/
systems/` and most of `core/` — was deleted on 2026-08-07 rather than repaired. Both clients build and `verify-layers` is green.

| Piece | State |
|---|---|
| Directory stack and the layer gate | green — **measured 2026-08-07: 136 files, 312 internal includes**, no upward include |
| `walk` (the pedestrian frame oracle) | builds; `render/` + `world/` + the `core/` value TUs those two reference |
| `wasm` + `worker` | build |
| `units/` | only `Unit.h` and `UnitRegistry.h` survive, kept alive by `world/World.cpp`'s effect path |

## Gaps

| Finding | Where |
|---|---|
| `units/` has no topic file and `sim/test/` does not exist. **Measured 2026-08-07: `verify-trees` reports 9 orphans** — 7 engine (5 missing `test/` dirs, `units/` missing both, 2 leaf files a directory rule would split) and 2 for `mods/demo`, which has neither `doc/` nor `src/` | [`build-and-ops.md`](build-and-ops.md) |
| `world/World.cpp` pins `units/Unit.h` and through it `core/Store.h`, `Countermeasure.h`, `Emitter.h`, `Flight.h`, `NetReport.h`, `Team.h`, `VisualContact.h`, `WeaponUplink.h` — the last of the combat value types. **Measured 2026-08-07: `verify-types` counts 1 aircraft type in 2 files** (`core/Countermeasure.h` ×3, `world/World.cpp` ×1), plus 68 uncounted ordnance/ground-type mentions | this file, unresolved |
| The entity and effect stages are built and have **nothing to draw** — no mesh ships, and **no topic file describes them** since the combat effect catalogue was retired | [`render/renderer.md`](render/renderer.md) §3 |

**What blocked pruning is gone.** Telemetry registers sources **by position**, so dropping one used to
change the column layout of every trace and therefore every baseline. With the baselines gone that
constraint is gone too, and pruning `core/`'s last ten orphaned value types is now free — see
[`core.md`](core.md) `## Gaps` 3.

## Knowledge

### The process

```
fb-tiles (server: worldwide DEM / OSM / aerial imagery)  ──HTTP──▶  Client
                                                          = physics + world + renderer + overlay + AI
                                                            as ONE process (WASM browser | native CLI)
```

Nothing is preloaded — every tile on demand. From which follows: **every point on Earth is a valid
start**, and the engine ships no world.

### The two clients

They link the SAME source list — `PEDESTRIAN_SRCS` + `render/` + `world/` — which is what makes a PNG
from one a statement about the other. WASM is a different toolchain target (emcc/wasm32) that recompiles
that list: a cross-compile, not a duplication of the architecture.

| Client | Source | Target | Role |
|---|---|---|---|
| **`gpu_walk`** | `clients/AppWalk.cpp` | `make -C sim walk` | **the pedestrian frame oracle.** A camera at eye height over a named coordinate, the terrain streamer, one PNG |
| **wasm** | `clients/AppWasm.cpp` | `make -C sim wasm` | the browser. Builds the tile worker with it, so a boot never hangs on a missing worker |

A third entry point, `clients/SimHost.cpp`, is not an engine client: it is the 118-line static HTTP
server the `fb-sim` container compiles to serve `web/`.

`gpu_walk` flags: `--lat --lon --ground --eye --yaw --pitch --view --albedo osm|photo --utc --warm
--base --size WxH --out`. `--warm N` is the number of streaming passes before the shot: the near cut has
to be resident before a frame means anything.

### Directories

| Directory | Responsibility | Doc |
|---|---|---|
| `sim/src/core/` | value types, log, telemetry, the state blocks, elevation hook, geodesy, units, matrices, calendar and ephemeris. **Never points into any layer above it** | [core.md](core.md) |
| `sim/src/units/` | world entities and the registry that holds them | *(no topic file)* |
| `sim/src/render/`, `render/stages/` | the WebGPU renderer, one class per shader | [render/renderer.md](render/renderer.md) |
| `sim/src/world/`, `world/terrain/` | world, tile streaming, terrain maths (a lowercase library by decision) | [world/terrain.md](world/terrain.md) |
| `sim/src/clients/` | entry points, app lifecycle, sink implementations | [clients/clients.md](clients/clients.md) |
| `sim/test/` | the harnesses and their declarations, mirroring `sim/src/` path for path — **does not exist, and no document describes what it should be** | *(no topic file)* |
| `tiles/` | fb-tiles, the tile server (its own Makefile) | [world/terrain.md](world/terrain.md) |
| `mods/` | the titles. **`demo/scene.json` and nothing else** | [mods.md](mods.md) |

### An entity is a body

A body declaration lives in a mod's own directory, named by `mod.json`: segments, joints, contacts,
force sources, medium, plus model, materials and brain ([`body-format.md`](body-format.md)).

**The engine ships none, and nothing in `sim/src/` reads one today.** The layer that composed and
stepped entities was deleted with the simulation layer on 2026-08-07. A directory without a manifest is
documentation, not a mod, and drops out of every build list ([`mods.md`](mods.md)).

### The snapshot discipline

**It has no subject today and it is recorded because whatever steps bodies next must re-earn it**: per
tick all entities compute first, then a barrier makes the new poses jointly visible. A pose query
therefore always yields the state of the last **completed** tick — tick order cannot influence any
result. That is what makes a parallel run byte-identical to a serial one, and the check is running the
same declaration over several thread counts.

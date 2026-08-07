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
| **ONE spatial index, and it is derived** | a quadtree over the sphere with a **vertical extent per node**, split vertically only where the content demands it. Never a second index beside it: two indexes are two truths, and that failure class has cost this project three rounds (two class paths, two DEM samplings, two class models). It answers *where*; it **owns nothing** |
| **`float64` is the truth, `float32` is camera-relative, and the conversion happens ONCE and LATE** | ECEF in double resolves ~1 nm at Earth radius, so millimetres are met by a factor of a million and no cell origin is needed for them. Any intermediate that computes in absolute `float32` has lost the precision before the conversion could save it — and it is invisible, because the result lands half a metre off and looks plausible. Acceptance: one named conversion site |
| **What the SIMULATION needs must exist without a GPU; what only the PICTURE needs may live on the GPU** | the engine has to run headless. Height at a point, building footprint, class at a point and object positions are simulation and answer on the CPU; the cluster DAG, the triangle meshes of terrain, buildings and trees, and all appearance are picture and need not. **One geometry, one predicate, two evaluators** — the edge test a fragment runs is the same one a CPU query runs, shared code and not two implementations. Acceptance: a sample of world points, CPU answer against GPU answer, and the difference is a number. That check did not exist for the two DEM samplings, and they disagreed with the drawn mesh for 30 hours |
| **Everything visual is created on the GPU; the CPU does administration** | owner, 2026-08-07: *„am besten entsteht alles visuelle in der gpu und die cpu macht nur verwaltung."* Administration is: what is resident, what cuts the frustum, what to fetch, what to evict — bounding volumes and residency, nothing that has vertices or pixels in it. **Every move is measured before it is made**, in the order of the CPU cost it removes |
| **The OSM vectors are the truth; the GPU holds a copy for derivation** | building extrusion from footprints, classification per fragment, vegetation scattered over landcover polygons, road surface from linestrings — all compute or fragment work over one resident vector set. The vectors themselves are fetched and parsed once and stay reachable by the CPU, because the simulation queries them. **Beyond that the CPU keeps only bounding volumes**: what is resident, what cuts the frustum, what to fetch. That is GPU-driven placement, the property the declared reference (Days Gone, Horizon Forbidden West) is named for — and it is the same sentence as "the index owns nothing", carried to its end |
| **Vegetation follows the same rule, and there it is compulsory** | measured: 0.417 ms and 770 kB per tree, so **per species** it is free (16 species = 6.8 ms, 12 MB) and **per instance** it is dead (5000 trees = **3.7 GB**). Either every beech is identical and the forest looks stamped, or growth moves to where per-instance is affordable. The strong form is the texture rule one level up: **the mesh is a FUNCTION, not a buffer** — a tree is `(species declaration, seed, place)`, nothing is stored, and LOD becomes "evaluate fewer segments" rather than a second geometry |
| **The index is fed by the ECS and never authoritative** | position is a component in `float64`; the tree may not hold a position the store does not. Terrain is **not** an entity — tile geometry is streamed, not simulated, and an index may carry both because it needs only place and extent |

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

### Why GPU-driven is not a preference: the CPU is the scarce resource, by orders of magnitude

**~2000 GFLOPS across 640 shader units against 2 fast cores.** Two cores deliver some tens of GFLOPS for
branchy code, so the ratio is one to two orders of magnitude — and it holds on the declared target
hardware: the A18 Pro pairs 2 performance cores with ~2.6 TFLOPS, the PS4 paired 8 slow Jaguar cores with
1.84 TFLOPS. Both times the CPU is what runs out.

This session measured the imbalance without looking for it:

| measured | |
|---|---|
| the whole GPU frame without grass | **6.6 ms** |
| terrain mesh + DAG per tile, CPU | **12.8 ms** |
| two of those per pass while streaming | **24–30 ms per frame** |
| one building-DAG spike | **260–473 ms** |

**The GPU idles at 6.6 ms while the CPU stalls the frame.** Moving the mesh and both DAGs into worker
threads took p99 from 49.68 to 18.01 ms — but worker threads share the same two cores; the pool takes
`hardware_concurrency() − 2` capped at six, which on two fast cores is optimistic because it counts
efficiency cores. **Threads redistribute the CPU; they do not add any.**

That is what makes "vectors on the GPU, bounding volumes on the CPU" the only split that fits the
machine, rather than the tidier one.

### What the vectors on the GPU dissolve, and the one question they open

The numbers are already measured and they point one way: a whole vector tile is **153 kB raw / 89.8 kB
gzip**, the `land` layer alone **3.1 kB**, against **32.5 MB** for the class raster it replaces — four
orders of magnitude. Vectors are also resolution-free, so nothing downstream inherits a texel size.

Dissolved rather than fixed: the CPU extrusion in `world/BuildingField` — and with it the cause of the
260 ms hitch that one round only *mitigated* by decoding one tile per call; the class raster and its
32.5 MB; tile residency as textures rather than as bounds; and vegetation placement, which stops being a
new problem and becomes the same compute pass over the same polygons.

**For vegetation the obstacle is the recursion.** Growth branches and the output size is not known in
advance, but a vertex shader needs to answer, for vertex *i*, which branch segment it belongs to — the
growth has to become **indexable** rather than sequential. That decides whether it works without an
intermediate buffer or whether a compute pass has to materialise the mesh. **What is not lost either
way:** the C++ generator built on 2026-08-07 becomes the ORACLE for the GPU version — it is byte-identical
to its own reference across 80 buffers and 16 species, and the GPU version must pass the same test against
it. Without it, "looks like a tree" would be the only yardstick.

**The cluster DAG — and a correction, because the blocker was named wrong.** Today **neither** half runs
on the GPU: the DAG is BUILT in CPU worker threads (`clients/TileWorkerMain.cpp` in the browser,
`world/TerrainLoader.cpp` natively) and the cut is SELECTED on the CPU every frame
(`render/stages/TilesStage.cpp`, a loop over every cluster of every visible tile).

The 64-bit-atomics wall is **Nanite's software rasteriser** — it packs depth and triangle id into one
`atomicMax` — and it does not apply to cluster selection. Our selection is a **pure per-cluster
predicate**:

```
DagSelect(c) = Sse(c) <= tau  &&  Sse(parent(c)) > tau
```

Two screen-space error tests and **no traversal**, because monotone error along every root-to-leaf path
makes it a single crossing: exactly one cluster per region of the surface answers true. That
parallelises completely — one thread per cluster, no atomic for the selection itself. Only the
compaction into draw ranges needs coordination, and that is a prefix sum plus an indirect draw. Five
compute stages already exist in the tree, all in the atmosphere.

**So the selection can move to the GPU without what WGSL lacks.** The DAG *build* is the harder question
— edge collapse and error bounds — but it already runs off the frame thread. Measure before building.

### Why a quadtree with height and not an octree

The owner proposed an "octree shell, N km thick, divided into sectors". That is the same structure —
the only difference is whether the vertical axis *subdivides* or merely *bounds*.

Terrain, buildings, vegetation and structures all cling to the surface. An aircraft at 10 km lies in the
**column** of its cell, and a frustum cuts columns as reliably as cubes. An octree wins only when dense
content sits at many altitudes, which will not be true for a long time — and it fits a sphere badly
(cubed-sphere, or degenerate cells), with a shell that is over 99 % air and eight children per node of
which nearly all are empty. The quadtree fits the sphere because the slippy scheme *is* a sphere
subdivision, and it fits the source, which speaks `z/x/y` and will keep doing so — **what falls is the
quadtree as the engine's INDEX, not slippy as the source's addressing.**

**Subdivide when it pays, not because the structure provides for it** — the same rule as the LOD ladder,
where the only declared number is the frame budget. A column stays a column while its content lies
together; the one column that spans 30 km with a single object at the top splits vertically, and its
neighbours do not.

**Precision was never the tree's job.** The two problems the shell was aimed at — millimetre placement
and culling — are solved by `float64` plus the camera-relative conversion, and by bounding volumes.

## Gaps

- **GPU-first is a work list, and it is ordered by measured CPU cost.** Nothing here is assumed; each
  line is a move to be benchmarked against what it replaces.

  | move | CPU cost today | note |
  |---|---|---|
  | building DAG | **260–473 ms** per spike | the largest single win; superlinear in the total set before it was made incremental |
  | terrain mesh + DAG | **12.8 ms/tile**, 2 per pass | `ChunkBuildEcef` 0.109 ms, `ClusterDagBuild` 3.685 ms, DEM decode + stitch 4.795 ms |
  | building extrusion | 4.5–6.3 ms/tile | decode + extrude, on the frame thread before it moved to workers |
  | cluster **selection** | a per-frame CPU loop over every cluster of every visible tile | the cheapest to move: a pure per-cluster predicate, no traversal, no atomics — see below |
  | classification | in build | the vectors have to be GPU-resident for it anyway |
  | vegetation growth and placement | not built | per instance it is 3.7 GB on the CPU, i.e. it was never an option |

- **The spatial index is specified and not built.** Today `world/World.cpp` carries a surface quadtree
  whose `kMaxZ = 14` is a **bare constant with no provenance**, set to the weakest of three sources
  (terrain DEM 15, OSM vectors 14, imagery 19) and shared by all three. Its split rule is justified in the
  code as *"equal albedo resolution by distance"* — and the albedo is being deleted, so the justification
  goes with it. `kNodeCeil` and `kGrace` hang off the same structure.
- **The height oracle disagrees with the drawn mesh: 0.383 m RMS, max 1.89 m.** Not the raster — measured
  and ruled out — but `ChunkBuildEcef` decimating 256² to 33² postings, i.e. **46.9 m spacing at z14**. A
  tree placed on that height stands up to 1.89 m wrong, and a better index would only index the error
  precisely. Named fix, unbuilt: **the oracle evaluates the DRAWN surface** — same posting indices, same
  triangle split — instead of interpolating the DEM a second time. It is an interface decision, because
  `fb_stream_ground` is a free C function with no access to the mesh parameters. **This is a precondition
  for placing anything, not a follow-up.**
- **There is no ECS.** `units/` holds 214 lines — `Unit.h` with `UnitPose`, `UnitArticulation`,
  `UnitSignature` and a virtual `Run(dt, units, world)`, constructed by nothing. A remnant of the combat
  layer. Order: the tree first (it already indexes tiles), the ECS on top; an ECS without an index would
  be slow immediately.

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

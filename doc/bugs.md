# Bugs

**What belongs here.** Something that exists and is wrong. `doc/requirements.md` says what must exist
and an unticked line there means *not built*; a line here means *built and broken*. If it has never
worked, it is a requirement. If it worked, or looks like it works, it is a bug.

**A line carries where it is and what decides it** — file and site, the measurement or the picture that
shows it, and what right would look like. A bug without a way to tell it is fixed is a rumour.

**A fixed bug is deleted, not struck through.** `git log` is the record.

---

## Silent success — a call that answers and nobody reads

The most expensive class in this tree, all with one shape. **The rule that closes the class:** a
function whose failure changes what the caller may do next returns that failure as a value the caller
cannot spend without deciding, and the value that says "no" carries no usable payload — which is
`core/GroundSample.h` and `core/WaterDepth.h`, written out by hand twice already. `std::optional` is
**not** that shape: `*opt` and `opt.value()` read the payload without anyone having looked at the
state, and `[[nodiscard]]` on the function is satisfied by an assignment. `Try(T *out)` is.

- **`RoofSurface::Cover` returns `void`, and `EarClip` bails silently** (`generators/draw/RoofSurface.cpp:32`, called at 209). When triangulation fails, `Covering` loops over an empty vector and draws nothing, then `BuildingMesh.cpp:383-387` draws `Gables`, `Eaves` and `Chimney` unconditionally — **a roof drawn as its own trim, floating in the sky with no covering**, visible in the shipped frame at (930,240)–(1190,370) of `after/street.png`. Right: `[[nodiscard]] bool Cover(...)`, and eaves, gable and chimney unspellable without a closed covering.
- **`emscripten_exit_pointerlock()` discards its result** (`clients/AppWasm.cpp:98`).
- **A status that is read into a variable and only logged is still discarded.** `BindInput`
  (`clients/AppWasm.cpp:242-272`) now routes all six registrations through `Bound`, which logs
  `input_unbound` on failure and **returns anyway**; the six results become six fields of one
  `input_bound` line and nothing branches on them. The comment above `Bound` states the class
  correctly — "the picture comes up with no input and no reason" — and then the picture still comes
  up. `[[nodiscard]]` cannot catch this shape: assignment satisfies it. Right: a keyboard that did not
  bind is a **refusal to run**, not a log line, because every verb the client has goes through it.
- **Six `Try` answers thrown away behind a `(void)`, and the invariant that saves them lives in
  another layer.** Each site writes `T x = 0;`, calls a `[[nodiscard]] bool Try*(T *out)` behind a
  `(void)` cast, and reads `x` afterwards. **Checked first for the harmless explanation, and it holds
  at every one of the six — no wrong value is reachable in the tree today**, which is why this is
  filed as a shape and not as a wrong number:
  `generators/GroundPatch.cpp:31` is guarded by its own class — `GroundPatch::Complete` refuses the
  whole patch unless *every* posting is `State::Resolved` (`GroundPatch.cpp:19-20`) and the
  constructor is private, so at line 31 `TryAslM` provably answers true. That one is sound, and it is
  the model: the invariant is three lines away, in the same file, behind the only door.
  `generators/Buildings.cpp:54`, `generators/Water.cpp:20` and `generators/Water.cpp:53` are guarded
  by something much weaker — a single-producer invariant in **another directory**. Every `Structure`
  and `Water` feature that reaches a generator is minted at `clients/Sim.cpp:218` and `:226`, and both
  set `f.Top`. Nothing in `generators/` can see that rule, no type carries it, and
  `generators/gate/SameRegionSamePlacement.cpp:410` already constructs a `Structure` with
  `FeatureLevel::None()` — so the rule is one ingest site away from being false. If it ever is: a
  top-less feature enters the `highest`/`deepest` comparison at 0.0 m ASL, beating every declared top
  below sea level and losing to every one above, and at `Water.cpp:53` becoming a water level whose
  `WaterDepth::Between(0.0, ground)` reports `LevelBelowGround` for every dry-land outline — a missing
  datum counted as a disagreement between two models.
  `clients/WorldMain.cpp:64-65` writes `waterDepthM=0` for a dry place, but the same row already
  carries `water=dry` from `Wetness(p.Water)`, so nothing is lost; it is the row that reads badly, and
  the row beside it (`structureHeightM` + `structure`) shows the deliberate form.
  Right, and **not** by swapping `Try(T *out)` for `std::optional` — this file's own opening argument
  rules that out and it still stands: `*opt` reads the payload with nobody having looked at the state.
  The answer is one rung up, at `C.41`/`C.42`: a `FeatureField::Feature` whose `Kind` is `Structure`
  or `Water` **cannot be constructed without a top**. It is an aggregate today with
  `FeatureLevel Top = None()`, minted by hand at three places, and the rule that keeps it right lives
  in a fourth directory. A named factory that takes the top as an argument deletes
  `Buildings.cpp:54`, `Water.cpp:20` and `Water.cpp:53` outright — no branch, no cast, no zero —
  and costs nothing at runtime. `GroundPatch` already has exactly this shape and that is why it is
  the one of the six with nothing to fix.
- **Past the Mercator limit the world loads tile (0,0) and says nothing.** `osmmesh_geo_to_tile`
  (`world/terrain/geo.cpp:19`) returns `OSMMESH_GEO_ERR_RANGE` and writes neither output for
  |lat| > 85.05112877980659°. Both callers discard the `int` and read the outputs they initialised to
  zero — `world/OsmField.cpp:35` (`uint32_t cx = 0, cy = 0`) and `world/World.cpp:472`
  (`rx = 0, ry = 0`) — so a standpoint in the high Arctic streams the north-west corner tile of the
  map as its own neighbourhood. **Reachable from a declared scenario**: `clients/Scene.cpp:65` accepts
  `lat` in `[-90, 90]` and `World::Open` (`world/World.cpp:79`) has no latitude guard at all, so the
  only thing between a scenario and this is nobody having written one.
  It is **one function at two sites, not a habit** — checked: of the seven `int`-returning C-ABI
  functions in `world/terrain/`, the other six are checked at every call site
  (`osmmesh_create` ×2, `osmmesh_fetch_tile`, `osmmesh_tile_grid`, `osmmesh_enu_init`,
  `osmmesh_terrain_decode_png`, `osmmesh_terrain_build_mesh`). What *is* systemic is that no mechanism
  reaches any of them: `int` is neither `bool` nor an enumeration, so the `[[nodiscard]]` sweep and
  every gate pass over the whole directory.
  Right: the range refusal reaches `World::Open` and is named. And note what it costs to say
  otherwise — Web Mercator ends at ±85.0511° by construction, so *every point on Earth is a valid
  start* is a claim the tile scheme does not hold and a refusal is the honest half of it.

## Bounds, allocation, and what the platform hides

*Measured 2026-08-11 in `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad`,
emsdk 6.0.3 / node 26.7.0 / clang, all at `-O2`: an index 400 kB past a live `std::vector` writes real
bytes and exits 0 **in the browser and on the native oracle alike** — the address is inside a mapped
heap in both cases. The premise "it segfaults natively" holds only for a write that leaves the mapping,
which a heap overrun almost never does. So the oracle is not louder than the browser for this class,
and the conclusion is stronger rather than weaker: there is no safety net on either target today.*

- **The sanitised wasm client does not finish a declared run, and the instrument is therefore native
  only.** `make wasm-asan` links — ASan × emdawnwebgpu × `-pthread` was the named risk and it did not
  materialise — boots in Chromium 151.0.7922.34, reaches `impostor_baked` and `stands_collected`, and
  then does not reach `run finished` on `demo/frame`, **one frame**, in 480 s of wall clock at ~4 cores
  saturated (`user 1911 s / real 481 s`); a second run of the same scene was stopped, still running,
  at **2 040 s**. The same scene, same browser (Chromium 151.0.7922.34), same host, unsanitised:
  **28.3 s, `rc=0`**. That is a floor of **72×** on a phase the architect's
  bare-translation-unit measurement put at 2.84×, and the factor is not the instrument's published
  cost — it is the load phase's own spin (see *The load loop polls the pool ~190 000 times a second*)
  with every `std::set` operation and every mutex acquisition instrumented. Two consequences, both
  load-bearing: `make gates` runs the **native** sanitised run and nothing else, and the wasm
  sanitiser cannot decide any question about the shipping heap, because emscripten silently raises
  `-sINITIAL_MEMORY=296MB` to **474 611 712 B = 452.6 MiB** in the ASan module (read out of
  `web/gpu-asan.js`) to make room for its shadow. A sanitised walk therefore has 53 % more linear
  memory than the client that ships and **cannot reproduce an exhaustion that depends on the 296 MB
  ceiling** — the `world/ChunkMesh.h` hypothesis below stays open, and the instrument that would close
  it is the native run under a cut heap, not this one.
- **The wasm hash in a measurement line names the file the host serves at `/gpu.wasm`, not the module
  the page loaded.** `clients/SimHost.cpp:151` — `wasm_build_id()` opens `WEB_ROOT "/gpu.wasm"`
  unconditionally and `/config.js` hands the result to every page, so any second page reports the
  shipping module's identity for a module it never ran. Harmless while one page existed; live the
  moment `web/asan.html` did, and it presented `wasm=b26dd4e50694430c` — the shipping module's hash —
  for a run of `gpu-asan.wasm`. `make wasm-asan` blanks `FB_BUILD` on its page rather than publish
  the wrong number, which pins that run to nothing. Right: `/config.js?module=<name>.wasm` hashes the
  module named, rejecting a name with a separator in it, and every page states the module it loads.
- **An exhausted heap is reported as malformed terrain — at eight sites, and no longer at the four that
  mattered most.** `world/ChunkMesh.h:50,51,89,141` now take their `NN·3·8 + NN·4 + NN·12` bytes and the
  vertex block through `Heap::Take` (since `1424214`), which ends the run naming the item and the count,
  so the "no mesh"/"no memory" confusion is gone there and the earlier description of those four as
  `malloc`+`return 0` was stale. It is live at `world/terrain/terrain.cpp:85,136`,
  `world/terrain/osmmesh_terrain.cpp:49,67,130` plus two `calloc` at `227,245`, and
  `clients/SimHost.cpp:186`: seven `malloc` and two `calloc` in a tree whose global `operator new` has
  ended the run properly since `core/io/Heap.cpp` landed. Right: `Heap::Take` at each, and for the
  C-ABI files a C-linkage door in `core/io/` so `tiles/` can satisfy the same symbol — those five are
  the only sites where the cost is more than an edit.
  **The freeze hypothesis stands and its instrument has changed.** "The client freezes after a few
  hundred metres" is still consistent with exhaustion: nothing evicts (below), `heapPeakKB` reads
  207 460 of 296 MB at rest (`sim/logs/demo-walk-wasm-20260811T181012Z.csv`) and the native gate run of
  the same scene reports `tileMeshMB=202.902` with `evictions=0`. But the deciding run named here — "the
  same walk with the four sites routed through `Heap`" — **is now simply a wasm walk on the shipping
  module**: if the freeze is those four allocations, the run already ends with
  `outshine heap exhausted: item=terrain node offsets …` instead of stopping. It needs no sanitiser and
  no second build. Two instruments that were proposed for it do not exist: the wasm ASan module raises
  its linear memory by 53 % (above), and **macOS cannot cut a native heap with `ulimit`** — measured on
  this host, `ulimit -v` is rejected outright and `ulimit -d 262144` fails with `Invalid argument`,
  `RLIMIT_AS` reads `9223372036854775807`, and a process then allocates *and touches* 4 GB without a
  refusal. A cut heap on this host means a ceiling inside `core/io/Heap`, which is the single door, not
  a limit from the shell.
- **The one bounds check the whole tree leans on can be defeated by arithmetic.** `core/Span.h:33`,
  `Span::Sub`, asserts `first + count <= Size_` in `size_t`. The sum wraps, so `Sub(4, SIZE_MAX - 2)`
  passes the assert and returns a span of `SIZE_MAX - 2` elements over `Data_ + 4`; every subscript of
  that span then also passes `assert(i < Size_)`. Reachable through `generators/draw/DrawSet.cpp:21`,
  `placed.Sub(range.First, range.Count)`, where both arguments are computed. Right:
  `assert(first <= Size_ && count <= Size_ - first)` — one line, no runtime cost, and it is the
  difference between a checked type and a type that looks checked. `ES.103` (no overflow) and
  `Bounds.4`-style reasoning: the check that guards the range must not itself be unguarded.
- **A bounds situation resolved by clamping, and the clamp is published as a measurement.**
  `render/ClusterCut.h:66` writes `ByLevel_[c.Level < kLevelBins ? c.Level : kLevelBins - 1]` with
  `kLevelBins = 8`. `DagCluster::Level` is a `uint8_t` filled by `core/ClusterDag.h:857` from a loop that
  stops only when a level has ≤ 1 cluster or ≤ 8 triangles, so a z14 chunk of ~130 k triangles halving
  per rung reaches roughly **14 levels** — bins 8…13 are all folded into bin 7, silently. That
  histogram is the LOD ladder's own diagnostic and is logged as `cutLevels` at
  `clients/SceneRunner.cpp:242-245`; its top bin is a sum over an unbounded number of rungs, so "L7" has
  never meant level 7. Right: `assert(c.Level < kLevelBins)` and no clamp — a DAG level past the bin
  count is a DAG defect, not a display case — or `kLevelBins` derived from `DagBuild`'s own ceiling.
  Note what is *right* here and must not be broken while fixing it: the extent travels with the
  pointer as `TerrainDraw::kLevelBins = ClusterCut::kLevelBins`, so the far end of
  `const long *TrianglesByLevel()` cannot loop past it.
- **The two directories that do the most unchecked pointer arithmetic hold no assertion at all.**
  Runtime `assert` sites, measured over 285 files and 33 777 lines: core 2 · core/io 0 · world 2 ·
  **world/terrain 0** · generators 5 · generators/draw 1 · render 1 · **render/stages 0** · clients 1 =
  **12**, one per 2 815 lines. `world/terrain/` is the only C-ABI code in the tree and indexes raw
  `float*` grids throughout; `render/stages/` is 19 files that compute every GPU buffer offset and
  extent by hand. (The figure "28 asserts" in circulation is `grep -c 'assert('` and counts the 16
  `static_assert`s; the runtime half is less than half of it.)

## Buildings

- **A dome on a rectilinear plan.** `ReadsAsRound` (`generators/draw/BuildingShape.cpp:258`) keys on corner count, fill and squareness — all of which a stepped rectilinear plan satisfies. Visible as a smooth 390 px arc over the town in `after/town.png` (620,255)–(1010,330). Right: add a turning test, every exterior angle ≤ 60°.
- **One ridge kinks over a bent plan.** `MassOf` applies Row → Wing → Setback once, top-down, to the whole. A bent bar fails `RowCut`'s `Fill ≥ 0.80`, is winged into two long masses, and neither piece is offered to `RowCut` again. Same defect on a T-plan. Right: a work list, ~15 lines.
- **The chimney reads as a 5 m post.** `BuildingMesh.cpp:338` runs the box from eaves to ridge + 0.85 m. The 0.85 above the ridge is correct practice; the box must terminate at the roof plane, not at the eaves.
- **The pavement's flag grid degenerates on inhabited ground.** `render/stages/BuildingDraw.cpp:339-340` builds its basis from `cross(upv, vec3f(0.577,0.577,0.577))`. That rotates with position on the globe, so flags never align with the kerb, and it is **exactly degenerate where up ∥ (1,1,1)/√3 — geocentric 35.264 N, 45.000 E**, near Kirkuk–Sulaymaniyah: `normalize` of ~0 → NaN. Root cause is the encoding: `uv.x < 0` spends the whole float on kind plus identity, leaving one metre coordinate, so any non-wall surface needing a 2-D pattern must invent a frame in the shader.
- **The footway ends mid-frontage** (`generators/draw/BuildingMesh.cpp:413-414`) — a per-edge binary accept/reject on a continuous stand-back. A consequence of the footway belonging to the building instead of to the street.
- **A wrong reason defends a right number.** `BuildingMesh.cpp` states "38–45 is the German pantile range (below 22 a pantile does not seal)". The ZVDH Regeldachneigung for a Hohlpfanne is H1 ≥ 35°, H2 ≥ 40°, and the absolute minimum for Ziegel is 10° with additional measures. 22° is neither. `kPitchOutbuildingDeg = 22` may be right; its stated reason is not. Likewise `MinAreaBox`'s comment "on L, T, U and notched bars every hull edge is a ring edge" is false — an L's hull has a chord.

## World and streaming

- **The DEM path answers for a place the caller did not ask about, because `fb_geo_to_tile` clamps.**
  `tiles/src/tilemath.h:16-18` clamps the latitude into the Mercator band and then projects, so a
  standpoint outside the band gets the ground of the nearest point inside it with no refusal and no
  note. Measured on `fb_world` at 85.052 N / 15 E before the band was refused at the declaration: the
  terrain requests were `/t/terrain/14/8874/0` — x correct for 15 E, y clamped to row 0, whose
  southern edge is 85.0466 N — and the run reported `groundAslM=-3448.27` for a standpoint **97.3 m**
  north of anything the scheme can address, exit 0. The same file holds a **second spelling of the
  band**, `FB_MERC_LAT_MAX 85.0511287798`, which differs from `world/terrain/geo.h`'s
  `osmmesh_mercator_lat_max_deg = 85.05112877980659` from the 11th digit on. One statement, two
  places, two values — and a third is still *printed*: `Scene::Read` hands the constant to `Need`,
  whose message runs it through `std::to_string`, so a refused declaration reads `lat out of range
  [-85.051129,85.051129]` while `lat: 85.051129` is itself refused (measured on `fb_world`: 85.05112877
  exits 0, 85.05112878 and 85.051129 exit 2). The message names an admissible value that is not one,
  by 1.3 cm. Right: `fb_geo_to_tile` refuses like
  `osmmesh_geo_to_tile` does — it is the same projection — and the bound is declared once. **Priced:
  `fb_geo_to_tile` has no caller in `tiles/` at all** (`grep -rn fb_geo_to_tile tiles/src` is empty),
  so making it refuse costs three call sites in `world/TerrainLoader.cpp:289,323,324` and nothing on
  the tile server.
- **The point query is refused by nobody, and it is the `world` target's whole purpose.**
  `clients/WorldMain.cpp:117` reads `argv` pairs through `std::atof` straight into
  `Sim::At(double, double)`, which reaches `Region::Of` and `fb_stream_ground` with no band check
  anywhere on the path. Measured **after** the round that added the declaration-time refusal, against
  `localhost:8081`, scene declared at 78.2 N: `fb_world pctrl probe 86.0 15.0 89.9 15.0 78.2 15.0`
  exits 0 and answers `lat=86 groundResolved=1 groundAslM=-3448.27` and
  `lat=89.9 groundResolved=1 groundAslM=-3448.27` — **bit-identical for two places 424 km apart**,
  which is the clamp and not the bathymetry; the control at 78.2 N gives `-173.103`. `groundResolved=1`
  is the lie: the ground was resolved for 85.0511 N, not for the pole. Right: the same refusal the
  declaration gets, and it is not a third `if` — a standpoint type that cannot be constructed outside
  the band (`doc/requirements.md` § I.17), so `At` cannot be called with two loose doubles.
- **A standpoint that replaces the declared one is checked by nobody.** `clients/AppWalk.cpp:35-46`
  lets `shots.jsonl` replace lat/lon through `Sim::SetStance`, which is `clients/Sim.h:77`
  `void SetStance(const Stance &s) { if (!Opened_) Stance_ = s; }` — a setter that silently does
  nothing once the world is open and validates nothing when it does act. The band check therefore
  lives at the declaration (`clients/Scene.cpp`) and this path goes around it: a hand-written
  snapshot at 86 N reaches `Sim::LoadTables`, where the failure surfaces as
  `sim ring_has_no_region lat=86 lon=15` — true, and it names the symptom (no region in the ring)
  rather than the cause (the standpoint is outside the projection). Right: one place validates a
  standpoint whichever declaration it came from, and a setter that cannot honour its argument
  refuses instead of returning.
- **The fabrication window was reported 21 % too wide, and the upper bound is wrong.** `doc/todo.md`
  and the round's report give `85.05113 < lat ≤ 85.0534 — about 250 m`. Measured against the tree's
  own code (`Schedule(Ring{14,1}).Widest(lat, 15)` and `osmmesh_geo_to_tile`, bisected to 1e-12 deg):
  the window is **85.051128779807 < lat ≤ 85.053023927135**, width 0.0018951 deg = **211.7 m**
  (WGS84 meridional 111 694 m/deg at 85 N; 211.0 m spherical). The upper end is where
  `Region::Of(14, lat, ·).Y()` drops from −1 to −2 and the ring's whole `y+1` row leaves the grid —
  at the reported 85.0534 the Y is already −2 and the run refuses. Right: the bound is a property of
  `Schedule::Widest` at `RadiusRegions = 1` and moves with the radius, so the number is derived from
  the ring rather than quoted.

- **A caster "vertex count" is neither a vertex count nor the caster's.**
  `render/stages/BuildingDraw.cpp:471-473` derives `BaseVerts` as `max(First + Count)` over the
  level-0 clusters and publishes it as `CasterVertexCount()` (`BuildingDraw.h:35`), which
  `Renderer.cpp:802` spends as `ShadowStage::NVerts`. Two things are wrong. A cluster's `First`/`Count`
  are **index** ranges — `World.cpp:556` offsets them by `BuildingDagIdx.size()` and
  `ShadowStage.cpp:151` hands them on as a `DrawRange` over the index buffer — so the number counts
  indices under a name that says vertices. And the building ladder is a concatenation of one DAG per
  ingested tile (`World.cpp:552-558`), so the maximum lands at the end of the **last** block's level 0
  and the range it names covers every earlier block's simplified levels as well; which block is last
  is the arrival order's. It decides nothing today: `ShadowStage` reads it only as a non-zero guard
  (`ShadowStage.cpp:158,296`, `Active()` at `ShadowStage.h:45`), the cut itself being per cluster. It
  is a number that is wrong under a name that claims otherwise, which is the state a defect is in
  before it is read by something. Right: the caster extent is the sum over the blocks and it is called
  an index count, or the field goes and `Active()` asks the buffer.
- **The vector fields never re-anchor, and the shader pays for it in cancellation, not in
  resolution.** `world/World.cpp:87-94` gives `BuildingField` and `WaterField` one ECEF origin — the
  standpoint, at `Open` — and nothing moves it again, while both keep ingesting for as long as the
  camera travels. The vertex shader computes `p + b.anc.xyz` in **f32**
  (`render/stages/BuildingDraw.cpp:106`; `anc` is written at `516-518` as `(float)(Anchor - Eye)`), so
  a near wall's camera-relative position is the difference of two quantities whose magnitude is the
  distance travelled from the standpoint, and the subtraction cancels down to a few metres while both
  terms keep the absolute error of that magnitude. The half of it that matters is **temporal**: `anc`
  is rewritten every frame from a moving eye, so the near field steps by one ulp of the travelled
  distance per frame — visible as jitter, where the static quantisation of `p` would not be.
  Priced (derived, IEEE-754 binary32; 720p at 60° gives a pixel focal length of 623.5 px,
  `core/PixelFocalLength.h`): the step reaches **1 px on a wall 5 m away at 65 km** from the
  standpoint (ulp 7.8 mm) and 1 px at 20 m away at 262 km. By verb: 65 km on foot is 13 h and never
  arrives; at 30 m/s it is 36 min; **at 250 m/s it is 4.3 min**, and flying is a declared verb. It is
  also a deviation from this tree's own convention with no reason beside it — the terrain anchors per
  tile on the z10 ancestor (`World::SurfaceAnchor`), which is ≤ 24 km at this latitude and bounded by
  the block rather than by the traversal. Right: one anchor per DAG block, which bounds `|p|` by the
  block and `|anc − eye|` by the view radius, because only near blocks are drawn.
- **`make gates` writes four inflated `demo/frame` rows into the archive every time it runs.**
  `sim/Makefile` `verify-still` runs the shipping client through `tools/tile_delay.cjs`, which holds
  every tile response back by 0–400 ms, and sets `OUTSHINE_TILES` but not `OUTSHINE_SIM` — so each of
  the four seeded runs posts to fb-sim under the same identity as a declared measurement run
  (`clients/AppWalk.cpp:53-58`: `client=gpu_walk`, `mod=demo`, `scene=frame`). Measured on this host,
  same binary, warm cache: final `loadMs` **6544 / 6620 ms** over two plain runs against **21 161 /
  21 580 / 20 947 ms** over three seeded ones — the load time an archive row reports is inflated
  **3.2×** and nothing in the row says a proxy was in the path. Same class as the sanitised-row defect
  `doc/requirements.md` § I.17 names, one instrument further: the gate is an experiment on the network
  and its rows are not observations of the product. Right: the instrument is a field of the identity,
  and until it is, a gate run posts nowhere.
- **A refusal is logged once per pass, and a stalled load spins at kHz.** `world/TilePool.cpp`
  `RunMesh` emits `tile_mesh_refused` on every attempt and `FetchInto` emits `tile_refused` on every
  GET, both of which repeat for as long as the leaf is in the target cut. Measured against a
  synthesised decode failure on one z14 target leaf (`demo/frame`, native oracle, one tile truncated
  to half its bytes by a proxy): **340 359 `tile_mesh_refused` lines in 60.8 s** (≈5.6 kHz, one per
  pass) and, for a synthesised 403 on the same leaf, **48 802 `tile_refused` lines plus 16 365 GETs**
  against the tile server. The refusal itself is correct and must stay loud — an absence and a
  refusal are different facts, and the quiet version of this is the defect the round removed — but
  the record it writes is unbounded and the GET rate is a load the server did not ask for. Right: the
  count carries the repetition (`Ledger::MeshRefused`, `Ledger::FetchRefused` already do) and the
  line is written when the reason for a given tile CHANGES, not when it recurs.

- **A remembered absence has no expiry on the client, and no weight in the cache that holds it.**
  `tiles/src/cache.c` gives a negative entry a lifetime (`FB_ABSENT_TTL_S`, 30 d) because an upstream
  404 can be transient. `TilePool::Remember(path, nullptr, 0, true)` (`world/TilePool.cpp`) drops
  that: the entry carries no timestamp, so `Lookup` answers `Reply::Absent` from it for the life of
  the process. Worse, it is stored with `len == 0`, so it adds nothing to `CacheBytes_` and the LRU
  loop — `while (!Cache_.empty() && CacheBytes_ + len > ByteBudget_)` — is never entered on its
  account. Over ground that is all 204 (open water, imagery gaps) `Cache_` grows in entry count while
  `CacheBytes_` stays at 0, and every byte request pays a linear scan of it under `CacheMutex_`.
  Decidable from the code, not measured. Right: the entry count is a budget of its own, and an
  absence carries the age at which it is re-asked.

- **`Reply::Absent` has a second door, and the comment on the first says there is only one.**
  `world/TilePool.cpp:465` reads `out->State = miss == Miss::Hole ? Reply::Absent : Reply::Pending;`
  under `/* ONE DOOR TO A TERMINAL ANSWER, and only a Hole goes through it. */`. Six lines below,
  `RunDag` mints `Reply::Absent` for `nverts < 3`. The behaviour is defensible — a soup too small to
  partition is a deterministic statement about content the caller supplied, not about the heap or the
  wire, and `World::Refine` now handles it explicitly — but the invariant the comment asserts is
  false in its own file, which is the one thing a comment here is allowed to do and the one thing it
  must not get wrong. Right: the sentence names the mesh path, or `RunDag`'s answer stops being
  spelled `Absent`.

- **`TilePool` can be told to forget a mesh and cannot be told to forget a DAG.** `Poll` now keeps an
  `Absent` in both `Posted_` and `Done_` on purpose, and `ForgetMesh` is the only eraser — it keys
  `MeshKey`, so a `DagKey` entry has no release. `World::Refine` sets `BuildingDagId = 0` after a
  refused block and never asks that id again (`BuildingDagId = ++BuildingDagSeq` is monotonic, so it
  cannot be re-derived either): the entry is unreachable and immortal. One map node plus one set node
  per refused building block, monotonic over a run. `R.1` — an owner with no matching release. Right:
  one `Forget(uint64_t key)` with `ForgetMesh` as its caller, and `World` calling it beside the
  `building_block_refused` line.

- **The 64-bit push guarantee stops at an `(int)` cast.** After this round `TelemetryRow::Push` and
  `LogField` carry no `long` overload, so an implicit `long` is ambiguous on both targets — verified.
  Six `long` accumulators survive behind explicit casts: `StreetField::Unwidthed_`, `Tunnels_`,
  `WaterField::NoGround_`, `Outliers_`, `OsmField::Missing_`, `Bad_`, `ClassField::UnknownFeats_`,
  `Submits_`, published as `(int)` at `world/World.cpp:518-519` and `clients/SceneRunner.cpp:261,263`.
  All are error counts at low rates, so no wrap is expected — the finding is that the new guard does
  not reach them and the cast is what makes them spellable. `World::MeshVram` is `long` too but is
  zeroed every `Refine`, so it is a gauge and not exposed.

- **The east stitch stops at the antimeridian instead of wrapping.** The new guard in
  `world/terrain/osmmesh_terrain.cpp` (`if (x + 1 < n)`) correctly stops a request the tile server
  cannot route, but longitude wraps and latitude does not: at `x == 2^z - 1` the true east neighbour
  is `x == 0`, so the fix leaves a one-column height seam at ±180° — the same seam the pre-existing
  `x > 0` guard leaves on the west side. The N/S guards are right as written. Cost of the correct
  form: a modulo instead of a bound, on two of four sides.

- **`stitch_edge` asked for tiles that cannot exist.** `world/terrain/osmmesh_terrain.cpp`
  `fetch_terrain_grid` guarded the west and north neighbours against `x == 0` and `y == 0` and had no
  guard at the other end, so a tile on the map's east or south edge issued a GET for `x = 2^z` or
  `y = 2^z` — a coordinate `tiles/src/route.c` cannot route (`xx >= n || yy >= n` fails the parse) and
  answers `404 no such route`. It was invisible because it only fires at the edge of the world and
  the answer was read as "this neighbour has no data". Fixed in the same round it was found, because
  the caller now reads a 404 as a defect rather than as a hole; recorded here because the class —
  a guard written on one side of a symmetric pair — is not searched for anywhere.

- **RETRACTED — "nothing streams during play" was tile-size confounding, not a defect.** The founding
  reading (t=31…77 s of `sim/logs/demo-walk-wasm-20260811T150518Z.csv`: `poolHttpGets` 310 flat,
  `tilesBuilt` 0 over 46 s) covers 46 s × 1.4 m/s = **64 m of walking, 4.3 % of a z14 tile edge**
  (`kMaxZ = 14`, `world/World.cpp:27`; pitch `40 075 016.686 · cos 52.106° / 2^14` = 1502 m).
  `doc/requirements.md` line 154 already carried the method; the derivation for *this* cut, per rung,
  is what decides it. A rung's ring radius is where `WantSplit` stops — `R(z) = span(z) · f / kEdgeTau`
  with `f = 0.5 · 720 / tan 30°` = 623.5 px and `kEdgeTau` = 384 px — so the z14 ring reaches only
  **2 439 m**, not the scene's 60 km view. New tiles per second is `2πR(z)·v / span(z)²`, which
  halves per rung: 0.0095 + 0.0048 + 0.0024 + 0.0012 + 0.0006 = **0.0184 tiles/s at 1.4 m/s**. That is
  **0.85 tiles expected in those 46 s** and **6.6 per 500 m**. Flat is the correct answer, and
  `tilesInView` 45→47 with `tilesTotal` flat is a disc translating by 4 % of a tile — not
  "orientation reached `Refine` and position did not".
  Confirmed by the eye columns on two declared runs: `demo/walk-500` walks 501.4 m and gets
  `poolPosts` 161→172 (+11) and `meshAdmitted` 130→135 (+5) against 6.6 predicted;
  `demo/crossing` covers 2 252 m and gets `poolPosts` 217→238 (+21) and `meshAdmitted` 130→159 (+29)
  against 29.7 predicted. Same order both times, and the second is on the number. **Delete this bullet once `doc/todo.md` items 2 and 3
  are restated** — it is kept only so that the walk gate is not built to catch a defect that is not
  there. What is genuinely unmeasured is *latency*: how far the eye travels between a tile entering
  the target cut and its mesh being drawable. No column carries it.
- **22 950 KiB of the heap has no owner at rest.** `heapResidualKB` is now measured, not inferred: 52 650–73 564 KiB at t=1, up to 94 132 KiB mid-load, settling at 22 950 KiB in three of five runs of `dfdd8e3a82efeefc` (`sim/logs/demo-walk-wasm-20260811T16*.csv`). The eight pthread stacks account for ~2 MiB of it. `prototypeKB` 6 527 and `standsKB` 1 676, logged at `outshine stands_collected`, are CPU-side and in no ledger column — the first place to look, and a decidable one: give them a column and the residual must fall by their sum or the overlap is somewhere else.
- **Nothing evicts.** `BuildingField`, `WaterField` and `StreetField` grow monotonically and their unit of removal does not exist. At 545 KiB of building heap per tile and ~29 MiB of real headroom, that is on the order of fifty tiles before exhaustion.
- **The in-cone priority boost is multiplicative at 20×** (`world/World.cpp:181`, 1.0 against 0.05). The reference adds a capped 0.5 to a 10-point scale and documents 1.0 as the setting that produces thrash.
- **`kGrace = 180` is counted in passes** (`world/World.cpp:32`) — 3.0 s at 60 fps and 6.0 s at 30, so the machine's pace decides what the world holds.
- **The byte cache finds its LRU victim by linear scan under a held lock** (`world/TilePool.cpp:236-240`), n ≈ 600 at 64 MiB of z14 tiles.
- **`World::Refine` builds no intermediate level** (`world/World.cpp:241`, and the traversal's own comment at 246) — correct for a cold start, wrong for travel, because there is then no ancestor rung to hold coverage while a fine rung streams.
- **`Sim::Features` gained a slice, but a feature inside the tile's 23.3 m buffer still yields twice.**
- **A crossing costs +1.77 ms at p50** against its neighbourhood, 1.03 of it the ring's own snapshot — in no column, because `Populate` runs after `Refine` inside one function.
- **A failed allocation and a decode failure mint a terminal hole.** `world/TilePool.cpp` `RunMesh` answers `Reply::Absent` at four sites and only two of them are a refusal: `!tile.terrain` after `OSMMESH_OK` (the server said 204) is one, `fetched != OSMMESH_OK` and `!ChunkBuildEcef(...)` and an empty `TileDagBuild` product are not. `OSMMESH_ERR_DECODE` (`world/terrain/osmmesh_terrain.cpp:114,233`) is a corrupt or truncated PNG; `ChunkBuildEcef` returns 0 from **three `malloc` failures** (`world/ChunkMesh.h:52,95,153` — see *An exhausted heap is reported as malformed terrain*, whose consequence this line states). Since `Absent` became terminal at the node (`world/World.h` `MeshState::Vacant`), each of those permanently retracts the split over that quadrant, deletes 58 % of the buildings standing on it (line below) and reports the load **complete** — where before it hung the load, loudly. `Classify` (`world/TilePool.cpp:49`) adds a fifth: every 4xx except 408/429 becomes `Absent`, so one proxy 403 during a deploy is a permanently coarse quadrant. Two of the four sites log nothing at all. Right: the miss carries **why** — the thread-local side channel that already carries `tMeshPending` becomes an enumeration `{None, Hole, Refused, Wait}` and `RunMesh` maps only `Hole` to `Absent`; `Refused` and `Wait` are `Pending` plus a counted, named refusal, and an allocation failure goes to `Heap::Exhausted` like the OOM three lines above it. Then "a transport failure deletes ground" is unspellable rather than ruled out by reading.
- **`poolPosts` and `poolRepeats` still wrap on wasm32.** The round that widened every counter to `long long` left `long Posts_ = 0, Repeats_ = 0` (`world/TilePool.h:177`) — the accumulators — and widened only the `Ledger` fields they are copied into, so the column is 64-bit-typed and 32-bit-valued. These are the two fastest counters in the tree: `poolRepeats` reaches **2 201 113 194 = 1.025 × 2³¹** in 2 868 rows of `sim/logs/demo-frame-gpu_walk-20260811T184845Z.csv`. That run shows no negative value because `walk` is the native oracle, where `long` is 64-bit — **the measurement used to clear the defect is the one build that cannot see it**. Signed overflow is UB (`ES.103`), not a wrapped column. Right: `long long`, and every 32-bit accumulator behind a 64-bit column found the same way — by type, not by looking at native output.
- **A completed mesh nobody asks for again is retained for the life of the pool.** `TilePool::Poll` erases a `Done_` entry only when a caller polls for it; a node that stops asking — which is exactly what the new retraction makes happen to a sibling whose build was in flight — leaves a full `TileBuild` (verts + indices + clusters, ≈ 4 MB at `kGrid = 128`) in the map for ever. Decidable from the same run: `tilepool_closed` reports `meshTiles=131 meshAbsent=1`, i.e. 130 completed builds, against `built=129` uploads (`world fbworld`, `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad/hole/afterF.log`). The round cleared `Build` for the `Absent` arm and not for this one. Right: the pool drops a `Ready` result whose key left the caller's cut, which needs the cut to be a thing the pool can be told about — see the cut-once shape below.
- **The drawn cut and the counted cut are two implementations that must agree, and nothing makes them.** `World::Descend` decides coverage with `Ready`, `World::CountTargets` decides progress with `Settled` (`Ready || Vacant`), and both re-derive the same tree from `Splits`. They agree today only because `Splits` removes a vacant child from *both* walks; delete that one call from `CountTargets` and progress reaches 1.0 over a square the draw pass leaves empty — the silent-hole failure, one edit away, with no test and no identity that catches it. `CanCover` makes it worse structurally: `Descend` calls it per child and it re-walks the whole subtree, so one pass is O(N·depth) node visits with a hash lookup each (`Splits` calls `Find` although `Descend` already holds the index), ~800 visits per root ring at ~1 800 passes/s during load. Right: **the cut is computed once per pass** into `(idx, role)` — target leaf · holder · drawn — and `Descend`, `CountTargets` and the request walk read it. Then two walks disagreeing is unspellable, the retraction is one rule in one place, and the duplicated `anyV`/`Wants`/`Ready`/`Emit` block the retraction added to `Descend` disappears with it.
- **A DEM hole deletes the built world standing on it, although the ground under it is drawn.** Measured 2026-08-11 over a synthesised hole (one z14 terrain tile, 8620/5404, answered 204 by a proxy in front of `fb-tiles`): the terrain cut retracts to the z13 parent and the picture is continuous, but `buildingVerts` falls 405 504 → 170 601 and one whole region grows nothing (`sim` `region_without_ground`, `clients/Sim.cpp` `Ask`). The cause is one answer used for two questions: `world/BuildingField.cpp:235` drops a footprint whose corner heights do not resolve (`NoGround_++`), and a region whose ground block is Missing is refused. But a height for that place **does** exist — it is the one the picture draws, the coarse ancestor's. Right: a place with no tile at the finest rung reads the finest rung that HAS one, so the footprint and the stand stand on the surface the eye sees; today the oracle answers only at `gSurface.Z` (`world/TerrainLoader.cpp:301`, *"no other zoom of this surface exists"*).
- **An absent tile is remembered for the life of the pool and nothing removes it.** `world/TilePool.cpp` `Poll` keeps the `Absent` result in `Done_` and the key in `Posted_` — that is what makes the answer final and stops a thread being spun on it — but nothing evicts either table, so a flight over a large hole grows both without bound. Bounded today by the number of distinct absent tiles a run asks about, which is 1 in every measured run. Right: the same unit of removal eviction needs everywhere else (see *Nothing evicts*), not a second mechanism.
- **The per-pass build budget bounds installs, not asks.** `world/World.cpp:390-417` decrements `budget` only in the `Ready` arm, so a pass the pool cannot answer asks **every** candidate and spends nothing: the cost of a stalled pass is O(wanted), not O(2). Measured over `demo/crossing` (900 frames plus load, `sim/logs/demo-crossing-gpu_walk-20260811T172219Z.csv`): `meshCapped` 217 against `meshWanted` 2 029 402, i.e. 0.011 % of wants. Two separate things are wrong: (a) the ask is unbounded, which is what `doc/requirements.md` § 0.2 calls the missing second cap — *how many may start* per update; (b) even as an install cap, 2 is not the binding constraint and neither is the in-flight cap. The binding constraint is **CPU inside the mesh build**: `world tilepool_closed` for the same run reports `meshCpuMsPerTile = 237.29` over 4 threads = 16.9 tiles/s, against a measured drain of 12–13/s (`poolQueued` 116→0 while `meshAdmitted` 11→130 in 9 s) and 118/s admissible at 59 fps. Do not conclude that the cap is useless — it is the only bound on a warm-cache teleport; conclude that it is measured against the wrong thing, and that a queue that empties is not a pool that is fast.
- **The load loop polls the pool ~190 000 times a second** (`poolRepeats` 2 069 319 against `poolPosts` 196, same run). Attribution matters and the earlier phrasing had it wrong: **99.99 % of the repeats happen before residency, not during play.** In `demo/walk-500` `poolRepeats` is 1 953 923 at t=10 s (residency) and 1 954 287 at t=183 s — 364 repeats in 173 s of walking against 1.95 M in 10 s of loading. The load spins `Refine` at ~1 800 passes/s × ~119 unready leaves; every one of those takes `QueueMutex_` and attempts a `std::set<uint64_t>` insert on the thread that draws, against the four workers that need the same mutex to pop (`world tilepool threads=4 inFlightCap=4`). The host was loaded during the measurement, which makes 190 kHz a **lower** bound. Right: a pending ask answerable without the queue's lock — a per-node "already posted" flag in the node, or an atomic set — and a load loop that is not a spin.
- **`eyeTravelM` counts a teleport as walking** (`clients/EyeTelemetry.cpp:14-25`). `Moved` has one input and cannot tell a step from a jump, so `Walker::Reset` on the `R` key (`clients/AppWasm.cpp:87`, and again at 372) adds the whole distance back to the declared standpoint to the path length: walk 500 m, press `R`, and the record says 1 000 m walked and 0 m displaced — which is the *same* row a 500 m circle writes, the one case the header claims the column exists to separate. Not reached by any run in `mods/demo` today (no scene has two motion runs at different standpoints), reached by the shipped browser client on one keystroke. Right: `Restood(Stance)` beside `Moved(Stance)` — the discontinuity is spelled at the call site that causes it, re-anchors nothing and adds nothing to travel.

## The memory ledger

- **The layout guard on `mallinfo` is an algebraic tautology and cannot fail on the case it was written
  for.** `core/io/HeapProbe.cpp:34-36` accepts the struct when `uordblks + fordblks == arena + hblkhd`,
  and its comment claims "a field order that had slipped could not satisfy it". In the dlmalloc this
  sysroot links (`~/Git/emsdk/upstream/emscripten/system/lib/dlmalloc.c:3593-3599`) `arena = sum`,
  `hblkhd = footprint − sum`, `uordblks = footprint − mfree`, `fordblks = mfree`, so **both sides equal
  `footprint` in every heap state** — the identity says nothing about the heap and only tests four
  offsets. Measured under emscripten 6.0.3, `-pthread`, at three heap states (pristine, +14 MB, after
  free): the identity held every time with `hblkhd = 0`, and **940 of the 10 000 assignments of the four
  roles to the ten offsets satisfy it**. Four fields are permanently 0 (`smblks`, `hblks`, `hblkhd`,
  `fsmblks`), so **256 of those 940 are all-zero quadruples**: they pass the guard and make `LiveBytes()`
  return a constant 0 — the exact "zero that reads as measured" the round forbade, arriving through the
  guard rather than around it. Ruled out as harmless: the two realistic toolchain bumps *are* caught
  (`-sMALLOC=emmalloc` fails the identity, `u+f = 1 744` against `a+h = 134 144 384`; `-sMALLOC=mimalloc`
  is a link error), so the guard is fail-safe **today** and the defect is the claim, not the current
  reading. Right: a falsifiable probe instead of an identity — read `uordblks`, `malloc` a known
  `kProbe`, read again, `free`, read again, and require `after − before ∈ [kProbe, kProbe + 64)` and
  `back == before`. That fails on an all-zero struct, on a permuted order and on a stub, and costs one
  allocation at static init. Reproduction: `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad/guard.cpp`.
- **A correct comment was recorded as wrong, from a run with the wrong thread count.**
  `world/TerrainLoader.cpp:41-44` states "at 256 KiB per z14 grid this is 4 MiB a thread, 24 MiB at the
  six-thread ceiling", and the ledger's first reading was published as proof that "~24 MiB was wrong".
  The run measured had **`threads=4`** (`sim/logs/demo-walk-wasm-20260811T165201Z.log`: `threads=4
  inFlightCap=4 demCacheTilesPerThread=16`). The measurement closes exactly on four:
  `16 404.421875 KiB = 16 798 128 B = 64 × 262 144 + 4 × 5 228`, i.e. 16 full slots per thread at
  exactly 256 KiB and 5 228 B of `osmmesh_ctx` each (`dem_lru[128]` × ~40 B + ~108 B). So the comment is
  right to the byte and the correction is the error. Right: strike the correction; and note what the
  same arithmetic exposes — `kMaxTileThreads = 6` is a ceiling this host never reaches, so
  `inFlightCap` is 4 against the browser's six connections per origin, and the comment that ties those
  two numbers together ("which is why the in-flight cap and the thread count are the same number") is
  giving away a third of the transport on any host that reports six cores.
- **A 1 Hz probe is quoted as a peak of quantities that change every frame.** `HeapProbe::Sample()` was
  removed from `Outshine::CloseFrame` and now runs only inside `MemoryTelemetry::SampleTelemetry`, so
  `heapPeakKB` — the number a fixed linear memory has to be sized from — is the largest of ~1 sample per
  second, not of ~48. The same defect is already visible in the record: `poolSchedulerKB`, whose `Done_`
  map holds finished vertex buffers, reads **32 / 282 / 1 645 / 4 420 / 5 873 KiB** as the "peak" of five
  runs of one scene and one binary (`sim/logs/demo-walk-wasm-20260811T164925Z.csv`, `…165201Z`,
  `…165641Z`, `…165743Z`, `…165844Z`), every large value landing at t = 1–18 s during load. A 180× spread
  across replicates of the same run is a sampling artefact, so the true peak is unknown and the quoted
  282 KiB is the friendliest of the five. Right: the high-water mark is kept where the quantity changes —
  `TilePool` already holds `QueueMutex_` when `Done_` grows — and the ledger reads the mark, not an
  instantaneous walk.
- **One measured pool already sits outside `Pools`, and forgetting the next one still compiles.**
  `poolRegionsKB` is `Sim_.GeneratorHeapBytes()`; it is a published column and is folded into
  `poolSumKB` by hand at `clients/MemoryTelemetry.cpp` (`pools.Sum() + generator`), not by `Pools::Sum`.
  So the claim that "a measured pool outside the sum no longer compiles" is false in both directions: a
  tenth field added to `World::Pools` and omitted from `Sum()` compiles silently, and one pool is outside
  the struct today. `C.41` is about constructors leaving an object fully initialised and does not bear on
  this at all. Right: `std::array<size_t, (size_t)Pool::Count>` indexed by an enumeration with `Sum()` an
  accumulate — then a new pool is a new enumerator and the sum covers it by construction, which is what
  the open registry line in `requirements.md` §0.1 asks for.
- **`TelemetryBus::Tick` never checks that a source pushed as many fields as it declared**
  (`core/io/Telemetry.cpp:27-33`). The header is written once from the schema and every row is written
  from `Row_.Fields()` with no comparison, so a source that declares N channels and pushes N−1 shifts
  every column to its right in silence, for the whole run and every run after. This round added five
  channels and split the pushes across three new private functions, which is exactly the shape that
  makes it easy; the counts do match today — verified in `sim/logs/demo-walk-wasm-20260811T165201Z.csv`,
  column 74 is `heapResidualKB` and `heapKB − poolSumKB − heapResidualKB = 0` in all 138 rows. Right:
  `Push` takes the channel it fills, or at minimum `Tick` refuses a row whose size is not the schema's.
- **`Heap.cpp`'s exhaustion line prints `liveBytes=0` when the layout guard failed.**
  `core/io/Heap.cpp:16-20` formats `HeapProbe::LiveBytes()` unconditionally, and `LiveBytes()` returns 0
  when `LiveBytesKnown()` is false. The CSV was taught that an unmeasured quantity is empty; the abort
  message, which is the one place a reader has nothing else to go on, still prints a zero that reads as
  measured. The root is the interface: `LiveBytes()` returns `size_t` and answers 0 for "unknown", so
  every caller has to remember `LiveBytesKnown()` and one already does not. Right: `[[nodiscard]] bool
  TryLiveBytes(size_t *out)`, the shape `core/GroundSample.h` and `core/WaterDepth.h` already carry —
  and **not** `std::optional<size_t>`, which was this line's earlier recommendation and is wrong for
  the same reason the defect exists: `*opt` reads the payload without anyone having consulted the
  state, so the zero would stay spellable. With `Try` the number is unreachable except through the
  answer (`I.13`-style reasoning: make the interface carry the invariant, not the comment).
- **The probe's published cost is the frame thread's wait, not the work it imposes.** `mallinfo()` walks
  every chunk under dlmalloc's global lock, `TilePool::SchedulerBytes()` walks `Queue_` and `Done_` under
  `QueueMutex_`, and `TilePool::ByteCacheBytes()` walks the whole table under `CacheMutex_` — all three
  once a second, all three stalling the six worker threads for their duration. `heapProbeMs` (p50 0.21,
  p99 0.53–0.81, max 1.49 ms over five runs) measures only the caller. In thread-milliseconds the ledger
  costs up to seven times what it publishes, and the tile threads are where the world is built. Right:
  measure the stall on the worker side too, or stop walking — an allocation counter and per-pool
  high-water marks answer the same questions in O(1).
- **The frame-distribution comparison offered as evidence cannot fail.** The round reports the frame
  distribution "indistinguishable before and after" with `maxMs` 23.44 → 23.83. One probe per second at
  the measured 48 fps (`sim/logs/demo-walk-wasm-20260811T165201Z.csv`, `fps` 47.1–50.0 from t = 41 s)
  touches 2.1 % of frames, which lands at p98 — *below* the published p99 — and `maxMs` is a single order
  statistic, so a 1.5 ms addition is invisible by construction at every statistic quoted. The p50 is
  21 ms, so the addition is 1 % of a frame that is already 26 % over its 60 Hz budget. The host load the
  round disclosed is not the weakness; the design of the comparison is. Right: force the probe every
  frame for one declared run and compare the distributions of that against the unprobed run — then the
  effect is 100 % of frames instead of 2 %, and the per-frame cost follows by division.

## Light and shadow

- **A wall the sun cannot see is lit by the sun.** `render/stages/SurfaceLight.h:88` forms the near-field
  bounce as `nearE = (skyH + I.sun.xyz * (sunUp * sunVis * thruDir)) * alb`, where `sunUp = dot(sunDir,
  upv)` and `skyH` is the file's own **horizontal** irradiance (line 74-76). It is then weighted by
  `(0.5 + 0.5*ndu) * kSelfShelter`, so on a vertical façade (`ndu = 0`) the weight is 0.175 and the term
  carries the full horizontal solar irradiance regardless of where the sun stands relative to the wall.
  The comment three lines above states the intent exactly — "the SAME material under the SAME local
  light" — so this is a wrong expression under a right design, not a modelling choice. Numbers, at
  `alb = 0.5` and sun elevation 50° (`sunUp = 0.766`): the spurious irradiance is
  0.175 · 0.766 · 0.5 = 0.067 E⊥, against a legitimate sky term of (0.5)(1 − 0.35) · E_sky,horiz ≈
  0.325 · 0.10 = 0.033 — **twice the whole sky ambient on a shaded wall**, and it does not move when the
  sun's azimuth does. Every shaded façade, trunk and leaf back-face is flattened by it, which is the
  single largest gap between this picture and KCD's. Right: drive the near term from the irradiance the
  fragment already has — `(skyH * (0.5 + 0.5*ndu) + I.sun.xyz * max(dot(n, sunDir), 0.0) * sunVis *
  thruDir) * alb`. Decides it: two walls of one albedo facing 180° apart under one sun; their radiance
  ratio must follow their irradiance ratio, and today the shaded one does not change at all as the sun
  swings behind it.
- **The shadow bias is 0.82 m in the near cascade, and a different physical length in each.**
  `render/stages/ShadowStage.cpp:213` sets `par[1] = 1.5e-3` and calls it an "ortho depth bias";
  `ShadowSample.h`'s `refZ = ndc.z - C.par.y` subtracts it from a `[0,1]` depth whose range is
  `dz = zf - zn = 2R + 500` (`ShadowStage.cpp:192`). Cascade 0 has `R = 24`, so `dz = 548 m` and the bias
  is **0.822 m along the sun** — 17 texels of a 4.7 cm cascade-0 texel, where practice is one to three.
  The normal offset meant to carry the job is 0.07–0.16 m there, five times smaller, so the crude term
  dominates the refined one. Derived consequence: a shadow starts `bias · cos(el)` from its caster —
  0.63 m at 40° sun, 0.81 m at 11° — so every trunk, post, kerb and wall floats. And because `dz` grows
  with the cascade radius, the same constant is 2.5 m in cascade 3: one number, four different lengths.
  Right: state the bias in **metres along the light**, sized to that cascade's own `texelM`, and divide
  by that cascade's `dz` on the way into the uniform. Decides it: a vertical post on flat ground — the
  gap between its foot and the start of its shadow must stay under one cascade-0 texel.
  *Checked and ruled out as the cause:* the cascade **selection** is sound. Selection is by radial
  `length(rel)` against `far[c] = R_c` while the box is centred 0.5 R ahead, which looks like a
  fall-through hole, but for a visible fragment the worst offset is `R·sqrt(1.25 − cos φ)` and stays
  inside the box for any off-axis angle up to 75.5°; the 60° fov's corner is 49.6°.
- **Two adjacent terrain tiles compute two different normals at the posting they share.**
  `world/ChunkMesh.h:100-108` clamps the central difference at the grid border (`i0 = i > 0 ? i - 1 : i`),
  so the east edge of tile (x,y) is a one-sided difference toward the tile's interior and the west edge
  of tile (x+1,y) is the opposite one-sided difference toward *its* interior. The **positions** agree
  exactly — `osmmesh_tile_frac_to_geo(z,x,y,1,·)` and `(z,x+1,y,0,·)` are the same point, which is why
  there is no crack — but the shading normals do not, and `TerrainDraw`'s fragment builds its whole
  relief frame off the interpolated vertex normal (`nn = normalize(nrmIn)`, `groundMat`). The result is a
  lighting discontinuity along every tile boundary, a rectilinear grid at ~1.5 km spacing on z14. Right:
  sample one ghost posting beyond each edge — `osmmesh_tile_frac_to_geo` is defined outside `[0,1]` and
  needs no neighbouring tile, so this costs `2(gr + gc)` extra ellipsoid conversions and no streaming
  dependency — and give every drawn posting a centred difference. Decides it, and it is **decidable**
  with no reference: the normal at `fx = 1.0` of one tile against the normal at `fx = 0.0` of its
  neighbour must be the same vector.

## Frames and units

- **`TangentFrame::Geo` is not the inverse of `TangentFrame::Project`, and the bound written beside it is
  wrong by 1.9×.** `core/TangentFrame.h:37` states "over the 900 m a scatter reaches its error stays
  under a metre". `Project` is the exact ellipsoid projection (line 27); `Geo` is planar on
  `kMPerDeg = 111320`. Measured round trip `Project(Geo(e, n))` at longitude 9°: **1.771 m** at 900 m
  east and 0.735 m at 900 m north at 50 °N; 1.879 m / 0.410 m at 52.1 °N. It is a *scale* error, not
  noise, so it grows linearly — 5.9 m at 3 km east — and it is systematically eastward, so a whole stand
  is displaced the same way. The east term is wrong because the exact longitude scale is
  `N cos φ · π/180` = 71 700 m/deg at 50 °N against `kMPerDeg · cos φ` = 71 555, a ratio of 1.00203; the
  north term is wrong because the meridian scale is 111 229 m/deg there against 111 320. `Geo` feeds
  `clients/StandField.cpp:32`, whose result is immediately handed back to `Project`. **The same
  spherical conversion is written a second time in `clients/SceneRunner.cpp:287,318`**, where it turns
  `camera.eastM` and `camera.northM` into a stance — so a declared metre of channel is 1.002086 m of
  world eastward and 0.999546 m northward at 52.106 °N. **Confirmed to 5·10⁻⁵ m** now that the eye is
  in the row, and the comparison must be made frame-by-frame or it proves nothing: the last row of
  `sim/logs/demo-crossing-wasm-20260811T172832Z.csv` (wasm `9b110bb85af592ce`, Chromium 151.0.7922.34)
  is at frame 899 of 900, where the channel commands 2250 · 899/900 = **2247.500 m** and `eyeEastM`
  reads **2252.189145** against 2252.18914 predicted from `N cos φ · π/180 / (kMPerDeg · cos φ)`.
  Northward the sign flips: `demo/walk-500`'s last row commands 504 · 10749/10800 = 501.620 m and
  `eyeTravelM` reads 501.392034 against 501.39200 predicted — so the run reaches **503.771 m of the
  declared 504**, and the remaining 2.4 m of the apparent shortfall is only the 1 Hz row landing 51
  frames before the end. `demo/ring` would be 18.8 m long over its 9 km. **`eyeTravelM` itself is not
  contaminated** — it is the exact measurement of a wrongly commanded motion, and it is what makes the
  error decidable. Right: either make
  `Geo` the exact inverse (one Newton step on the ellipsoid, or invert through ECEF), or scale it with
  the frame's own `M` and `N cos φ` computed once in the constructor — and in either case correct the
  stated bound. Decides it: `Project(Geo(e, n)) == (e, n)` to a declared tolerance, a pure unit test with
  no reference.

## Picture

- **The canopy changes colour with distance further than the air can explain.** Seen in
  `demo/walk` from the shipping wasm module `70993b0f2d7a5327` in Chromium 151.0.7922.34, 1280x720,
  sun at 11.2 deg elevation: the crowns in front of roughly 100 m are a saturated yellow-green, and
  every crown behind that is a pale, low-contrast near-white that reads as a row of identical
  lollipops along the whole treeline. Aerial perspective over 100-400 m of clear air at that sun
  angle moves a canopy by a few percent, not from mid-green to near-white, so the step is the tree
  representation and not the medium. **The mechanism is not isolated and the still cannot isolate
  it** — a single frame cannot separate "the impostor bakes a different albedo" from "the impostor
  bakes a different light" from an LOD cut placed where the eye can see it. What decides it: the same
  standpoint with the impostor distance moved outward, two frames, one difference; and the same walk
  in motion, because if the two representations disagree in colour then every stand that crosses the
  cut pops, which a still cannot show at all.
- **The demo road reads as a dirt track** since the unmapped substrate landed: the ground fragment uses the default row as the **runner-up** class where the structure has no second hit.
- **Crowns are too transparent at 30–80 m.** A stand reads as a wall of white trunks with a green fringe; no canopy closure. Opacity, not form.
- **The bow-tie crown persists**, reduced but not eliminated — two crowns in `horizon-after.png` still show a straight diagonal seam.
- **A near trunk reads as a straight grey slab**, not as a beech.
- **The hornbeam hedge reads as a young plantation** — ten stems in a row with a bare lower third. The grower has no cut response, which is also what blocks coppice stool and pollard.
- **Leaf lamina is wrong on small dense plants** — box comes out ~8 cm against a real 2 cm, because `CardLeafM` solves LAI ÷ crown projection ÷ card count and few cards means huge leaves.
- **The poplar's stem stands at 84 % of its own buckling height.** `assets/world/species/poplar.json`
  declares `height_m 30` and `dbh_cm 25`, derived from `H/D = 1.20`. The *convention* is right — height
  in m over DBH in cm is the forestry slenderness ratio × 1/100, and the file's spruce at 0.85 → 85 sits
  exactly in the documented Norway-spruce snow-break band, so that derivation is sound and is not the
  defect. The *value* is: 120 is past every published stability threshold, and Greenhill's limit says so
  without appeal to forestry practice. `h_crit = 0.792 (E/ρg)^{1/3} d^{2/3}`; with `E ≈ 1.0e10 Pa` and
  `ρ ≈ 700 kg/m³` for green poplar, `d = 0.25 m` gives `h_crit = 35.6 m`, so a 30 m stem is at 0.84 of
  critical where real trees stand at 0.2–0.6 (McMahon 1973). Run backwards at 0.6 the same relation gives
  `d ≈ 0.42 m`. `dbh_cm` is what the grower solves its whole radius cascade against, so the error is the
  drawn trunk: a 30 m mast rather than a tree. Right: `dbh_cm` 40–60, i.e. `H/D` 0.50–0.75, and the
  origin string amended — the crown of `Populus nigra 'Italica'` being narrow lowers `h_crit` further
  rather than excusing the slenderness, so the reason currently written there argues the wrong way.

## Constants, names and units

*Found 2026-08-12 in the design round for the const header. Every line here is a number that exists in
the tree more than once, or under a name that says the wrong unit.*

- **One upstream's zoom bound is written three times, in two languages, and one of the three is
  suspect.** `world/TilePool.cpp:28` and `world/TerrainLoader.cpp:30` each define
  `constexpr int kProviderTerrainMaxZ = 15` — two definitions in two translation units, each with its
  own comment naming `tiles/src/tilesrc.c` as the authority — and that authority is a third copy,
  `K[FB_TILE_TERRAIN].maxz = 15` in `tiles/src/tilesrc.c:13`. `doc/requirements.md` § I.16 carries
  *"Zoom above 14 for terrain — `/t/terrain/15/…` returns non-PNG, so z14 may be the finest served;
  unresolved"*, so the value asserted in three places may be wrong in all three. It decides nothing
  today because `world/World.cpp:27` splits at `kMaxZ = 14` and never asks for z15 — which is itself
  the finding: a fourth number, in a fourth place, with no stated relation to the other three. Right:
  the zoom range is one upstream's own declaration about itself, stated once by the thing that fetches
  from it, and checked against what the wire actually answers.
- **The suffix `Ms` names two different units in the same tree.** `core/Units.h:22`
  `kMsToKt` is metres per second to knots; `clients/Walker.h:17` `kWalkSpeedMs = 1.4` is metres per
  second; `clients/FrameTelemetry.h:33` `kWindowMs = 1000.0` is milliseconds. Reading any one of them
  correctly requires reading its comment, against `CLAUDE.md`'s *a name that needs a comment is the
  wrong name*. `core/Units.h:15` `kMPerDeg` shows the unambiguous spelling already exists in the same
  file. Right: one declared suffix table, `MPerS` for velocity and `Ms` for milliseconds, applied
  everywhere; the ambiguity is decidable by grep and there are three sites.
- **A number derived by eye is spelled as if it were physics, and nothing in the tree can tell.**
  `render/stages/SceneScale.h:17` `kSceneExposure = 11.0` multiplies every scene radiance the renderer
  produces — sky, ground, buildings, blades and haze — and its own derivation says it was chosen by
  *"placing that at ACES input 0.32 — the value whose sRGB output is 0.70, mid-frame for a sunlit
  surface"*. That is an exposure decision anchored on where a mid-tone should land in an 8-bit picture,
  sitting on the path every physical quantity takes. It may be entirely correct as an exposure; the
  defect is that **no measurement in this tree can decide which side of the line it is on**, because
  `render/Renderer.h:59` `ReadPixels` is *"tightly packed W*H*4 RGBA8, already sRGB-encoded"* and there
  is no readback of scene-referred linear radiance at all. Right: the linear tap ahead of
  `ExposureStage`, and the constant then lives in the exposure stage or is shown to be physics
  (`doc/requirements.md` § I.26).

## Declaration and build

- **Ten declared scenes carry eighty world fields that nothing reads, and one of them is declared
  twice with the C++ copy winning.** Every `subject` scene in `mods/demo/mod.json`
  (`subject-beech`, `-meadow`, `-hazel`, `-elder`, `-box`, `-dog-rose`, `-hedge-hornbeam`,
  `-snag-spruce`, `-log-beech`, `-stump-beech`) declares `lat 52.10602`, `lon 9.43453`, `eyeM 1.7`,
  `yawDeg 280`, `pitchDeg 0`, `fovDeg 30`, `utc`, `windDeg 250`, `windMs 6.0` — and
  `clients/SubjectBench.h:1-18` states the bench replaces the world and the light outright. The bench
  places its own camera from its own `View` table, declares its own key at
  `SubjectBench.h:65 kSunElDeg = 11.0`, and runs its own 0/6/12 m/s wind row; only `lat`/`lon` reach it
  at all, through `Stand(Scene_.Lat(), Scene_.Lon(), kSubjectGroundAslM)` where
  `SceneRunner.cpp:32 kSubjectGroundAslM = 100.6` is a third literal the declaration cannot see.
  `fovDeg: 30` in the JSON and `SubjectBench.h:62 kFovDeg = 30.0` are one statement in two places and
  the JSON one is inert — against `CLAUDE.md`'s *every statement has exactly one place*. Right: a
  scenario whose stage is a studio has no latitude, no civil time and no met wind to declare, so the
  eight dead fields have no spelling (`doc/requirements.md` § I.25).
- **The C-ABI exception's stated reason is not true.** `CLAUDE.md`'s naming rule exempts
  `world/terrain/` because it is *"C-ABI, shared with `tiles/`"*. It is not shared: `tiles/osmmesh/`
  holds its own `terrain.c`, whose header comment says so in its own words — *"a deliberately smaller
  copy of the client's full terrain.c … which also builds ENU/ECEF meshes — a rendering concern this
  server binary has no use for"* (`tiles/osmmesh/terrain.c:1-7`). Two copies of one decoder, 111 lines
  against 164, drifting independently since the 2026-07-24 split; and the duplication has already cost
  one recorded defect in this file, the two spellings of the Mercator band
  (`tiles/src/tilemath.h` `FB_MERC_LAT_MAX 85.0511287798` against `world/terrain/geo.h`
  `osmmesh_mercator_lat_max_deg 85.05112877980659`, differing from the 11th digit). Every file under
  `src/world/terrain/` is compiled as C++ already; no C compiler in this tree reads them. Right: the
  exception is struck and the seam becomes a C++ interface, which closes § I.17's open *a C-ABI status
  reaches a house type at the boundary* rather than adding work (`doc/requirements.md` § I.22).

- **Six ticked or priced lines in `doc/requirements.md` name a tool that no longer exists.** The
  hardening ledger (`sim/tools/hardening_ledger.py`, its baseline and the `verify-hardening` gate) was
  deleted on 2026-08-12 by the owner's ruling that validators are C++ under `sim/test/` and not Python.
  `doc/requirements.md:501` is still `[x]` and names the script, the baseline and the gate as what
  implements it; `:464` (`[[nodiscard]]` on every `bool`/house-enumeration return, **214 of 214**) and
  `:471` (no `default:` over a house enumeration) are `[x]` and say the gate is what holds them; `:458`,
  `:465`, `:467`, `:469`, `:480` and `:481` quote its counts as their current measurement. **What is
  actually unheld is `:464`**: the attribute is a per-declaration fact, `-Wall -Werror` does not carry
  it, and no other gate reads it — a new `bool Foo()` written after this date re-opens the count with
  nothing failing. `:471` is unaffected, because `-Wswitch -Werror` is what holds it and the ledger only
  reported it. Right: the C++ suite carries the `[[nodiscard]]` sweep, and until it does, `:464` and
  `:501` are ticks with no instrument behind them.
- **The language standard has two values.** `CLAUDE.md` says C++17; `sim/Makefile:31` sets
  `CXXSTD := -std=c++17` and uses it for the compile gates; every shipping compile line
  (`Makefile:206,225,275,294,329`) hard-codes `-std=c++20`. The program is C++20 — forced by
  `vendor/dawn`, whose `webgpu_cpp.h` needs `std::type_identity` and `std::span` — while the gates
  judge a dialect the program is not built in, so a C++20 construct in `render/` or `clients/` is
  covered by no gate. Right: one variable, one value, and the reason (Dawn) beside it.

- **No gate reads the log levels of the run it just declared green.** `verify-walk-asan` now asserts
  the run's own motion verdict — `frames=10800 impostorStands=9565 treeTris=19130` — but a line at
  `ERROR` in the same 10 800-frame run still passes it, and the run emits exactly one:
  `render device_lost reason=2 msg="Device was destroyed."` (`sim/Makefile` `verify-walk-asan`,
  `render/Renderer.cpp:163`). Two defects, and the second is the reason the first cannot simply be
  closed. `2` is `wgpu::DeviceLostReason::Destroyed` (`vendor/dawn/out/gen/include/dawn/webgpu_cpp.h:276`)
  — the device the client destroyed on purpose at teardown, reported at the level reserved for a run
  that failed, and reported as an integer rather than as the enumerator (`Enum.3`). So an `ERROR`
  assertion added today would go red on a healthy run. Right, in this order: the callback answers
  `Destroyed` at `Info` and every other reason at `Error`; then the gate asserts that the run it
  declares silent logged nothing at `ERROR`.

- **`core/ClusterDag.h:72` reads `FB_TAU` from the environment** — the picture depends on an undeclared variable. **And it is one of six.** Also live: `FB_TAA` (`render/Renderer.h:318`, default on) switches temporal antialiasing, which changes both the pixels and `frameMs`; `FB_GEOM` (`render/GeometryIsolation.h:15`) disarms the shadow receivers; `FB_MOON_SCALE` (`Renderer.h:230,360`, applied at `Renderer.cpp:399`) scales the moon off its real 0.0045 rad; `FB_GROUND_CLASS_VIZ` (`TerrainDraw.cpp:642`) and `FB_TONE_PROBE` (`TaaStage.cpp:226`) replace the fragment outright. **None of the six appears in any telemetry column**, so two runs of one wasm hash are not comparable and no CSV can say which picture it measured — which is the same defect as a resolution that moves under load, wearing a different hat. Right: the four that change the picture leave the environment entirely; the two diagnostics stay and ride a published column.
- **`core/Mat4.h` is entirely dead, and the comment defending it names a test that does not exist.** `Mat4Identity`, `Mat4Mul`, `Mat4Perspective`, `Mat4LookAt`, `Vec3Normalize` and `Vec3Cross` have no caller outside `core/Camera.h`; inside `Camera.h`, `CameraBasisFrom`, `CameraAxes`, `HorizonDipRad`, `MvpTranslate`, `Frustum`, `FrustumFrom` and `AabbVisible` have none either. `CameraBasisEcef` is the only live function in the pair (`clients/Sim.cpp:497`, `clients/SubjectBench.cpp:239`) — verified repo-wide, not only under `sim/src`. `Camera.h:76` asserts "CameraBasisFrom above is NOT dead: sky dome and star field are an infinity pass in LOCAL render-ENU"; `SkyStage` and `StarsStage` call nothing in the file. Two comments say "Pinned in `test_camera.c`"; no such file exists anywhere in the tree. Three consequences, worst first: the dead `Mat4Perspective` builds a **GL-style [-1,1] reversed-Z** projection, so anyone reviving it under WebGPU's [0,1] clip volume silently loses everything past the mid-range; `outshine::Frustum` (`Camera.h:132`) and `outshine::Render::Frustum` (`render/Frustum.h`) are two spellings of one statement against "every statement has exactly one place"; and a false comment is worse than no comment. Right: delete `core/Mat4.h` and everything in `core/Camera.h` but `CameraBasisEcef`.
- **Five WGSL constants that decide the canopy carry no origin.** `render/Sward.h:59-63` declares `kMinSinEl 0.05`, `kLeafTrans 0.85`, `kTransIso 0.35`, `kTransFwd 2.6` and `kTransP 4.0` with no `[SET]`, no derivation and no unit — alone among the twenty constants in that function, and against the rule that every number carries its origin. Two are load-bearing. `kLeafTrans` multiplies the leaf colour on the transmitted path and its **name is the trap**: a green leaf at 550 nm has R ≈ 0.10 and T ≈ 0.085, so a literal "leaf transmittance" is 0.08 and someone will one day write it there and lose the whole back-lit canopy; what makes 0.85 right is that it is the *ratio* T/R, consistent with `kScatCut = sqrt(1 - ω)` and `ω = R + T = 0.185` on the line above. And `kTransIso + kTransFwd·cos^kTransP` is divided by the same `kInvPi` a Lambertian gets, with no statement anywhere that its hemispherical integral is 1 — so the transmitted path is not shown to conserve what the reflected path gives up. Right: one origin line per constant; rename `kLeafTrans` to what it is; and either normalise the lobe or state the deviation beside it.
- **Two headers guard themselves with reserved identifiers.** `core/Ephemeris.h:6` `#ifndef _EPHEMERIS_H` and `core/State.h:3` `#ifndef _FBSTATE_H`. A leading underscore followed by a capital is reserved to the implementation **in every scope** ([lex.name]/3) — undefined behaviour, not a style preference, and the rest of the tree already spells it `GEODESY_H`.
- **The winding is hard-coded at seven sites**; it belongs in the draw product beside the cluster list.
- **A stage that reports the same number every frame is reporting that it is not being measured.** In `after/town-spin.csv` (a rotation about a fixed point) `worldMs`, `meshMs`, `uploadMs`, `buildingMs`, `classMs`, `populateMs`, `nodes`, `drawnLeaves`, `draws`, `built` and `evicted` are all constant across 240 rows, and `distM` is 0.000 in every one — that run is not motion acceptance and was reported as if it were. Note the general form of the claim is false: of 89 columns in a live walk, 35 are constant and most legitimately so (identity, declared limits, stack ceilings). What is suspect is a *cost* column that does not move.
- **The log's timestamp is dead** — every `walk key` line carries `t=0.0` — and key repeat events are logged individually, so a held key floods the buffer.
- **`FacadeUv.h` has no `static_assert` anywhere**: 11 enumerators against a stride of 16, `kStyleCount 8` against 7 enumerators. A 17th `Facade` silently aliases identity 1.
- **`TreeGrower::GrowOnce` is ~130 lines** (`F.2`/`F.3`), and `TreeSpecies::Parse` is a 90-line flat key list (`F.3`).

- **A wire decoder reads multi-byte values in the host's byte order.** `world/OsmVector.cpp:111-112`
  takes protobuf `fixed32` and `fixed64` with `std::memcpy(&f, v.P, 4)` and `std::memcpy(&d, v.P, 8)`
  straight off the wire buffer. Protocol Buffers fixes those two wire types as **little-endian**
  (encoding spec, `fixed32`/`fixed64`), so this is correct on every target we build for today —
  wasm32 is little-endian by specification and both native hosts are — and it is wrong on any
  big-endian one. Filed as a bug rather than as a requirement because the code claims to decode
  protobuf and decodes host order instead; the caveat was checked and it is the only reason it has
  never shown: **the tree has zero `reinterpret_cast` and the rest of its decoding is byte-wise**
  (`world/OsmVector.cpp:18-19` assembles varints with `<<` and `0x7f`, `world/terrain/terrain.cpp:94-98`
  assembles Terrarium height from `p[0]`, `p[1]`, `p[2]` as bytes), so this is a single site, not a
  habit. The neighbouring `std::memcpy`s of floats into word arrays (`core/ClassStructure.cpp:74-76,124`,
  `world/ClassBuilder.cpp:340`) are **not** this defect — they are an in-memory layout that is never
  serialised, and host order is the right order for them. Right: two byte-wise assemblers beside the
  varint reader, `LittleEndianF32` and `LittleEndianF64`, and no `memcpy` from a wire buffer anywhere.

- **The declared negatives pass on any compile failure of the right shape.** `sim/Makefile`
  `verify-generators` and `verify-world` accept a fixture as refused if the compiler exits non-zero
  **and** its output contains the substring `file not found` — so a fixture that misspells its own
  forbidden header (`#include "Rendererr.h"`) is a green gate that proves nothing, and so is one whose
  body stops compiling for an unrelated reason while the include still resolves. Four fixtures ride
  this: `RendererIsNotReachable`, `WorldIsNotReachable`, `LogIsNotReachable`, `DrawIsNotReachable`,
  plus `world/gate/GeneratorIsNotReachable`. The three `-Werror` negatives under `src/core/gate/` are
  weaker still — `verify-types` checks only the exit status and matches no diagnostic at all, so any
  error in `HeightIsNotReachableWithoutItsState.cpp`, `AnswerIsNotIgnorable.cpp` or
  `DepthIsNeverNegative.cpp` passes it. Right, and it costs nothing: demand the **exact** expected
  diagnostic text (`fatal error: 'Renderer.h' file not found`, `error: ignoring return value of
  function declared with 'nodiscard' attribute`) **and** that it is the *only* error the compiler
  emitted, which is what makes a typo elsewhere in the fixture fail the gate instead of satisfying it.

- **`generators/gate/SameRegionSamePlacement.cpp` is 689 lines behind one `main`** (`F.3`), carrying on
  the order of thirty distinct claims — determinism of placement, the class lattice, water depth,
  way half-widths, `sizeof(Body)`, an allocation count — in one process. Three consequences: the
  first hard failure (it dereferences `std::optional` results directly, e.g. `ways.MadeAt(...)->WidthM`)
  hides every claim after it; the run reports `verify-generators: N failed` without a machine-readable
  name per claim; and no claim in it can be run alone. It also holds the tree's only `malloc` outside
  `world/terrain` and `clients/SimHost.cpp` (`:315`). Right: one translation unit per claim under
  `sim/test/generators/`, each with its own `main`, which is what the suite this file's requirements
  now describe is for.

- **The state channel has no collector.** `clients/ServerLog.cpp`, `clients/ServerTelemetry.cpp` and
  `clients/HttpPost.cpp` post the log and every telemetry row to `OUTSHINE_SIM`
  (default `http://localhost:8080`); the only implementation of that endpoint in this tree was
  `clients/SimHost.cpp`, deleted with the container on 2026-08-12. Measured with nothing on :8080:
  `demo/frame` exits 0, draws the same picture (`852bd4246ee34f65`) and writes its `give` products
  through `FileArtifacts` — and **not one of its 674 log lines mentions a refused post or a dropped
  batch**. `ServerLog`'s own comment promises "a gap is visible rather than silent", and the count
  that would make it visible rides the next batch that gets through, which never comes. So the
  archive the whole comparability argument rests on stops growing without a word, and the two sinks
  fire once per second into a closed socket. Right: a file sink beside `FileArtifacts` for both
  channels — the run already knows a directory, `OUTSHINE_OUT` — and `Server*` gone or re-homed with
  the collector it needs.

- **Twenty preprocessor conditionals in the library compile for a target no build produces.**
  `world/TilePool.cpp` (4, plus an `EM_JS` HTTP primitive and `fb_take_http_body`),
  `core/io/HeapProbe.cpp` (6), `clients/HttpPost.cpp` (4), `render/Renderer.cpp` (3),
  `core/io/StackProbe.cpp` (2) and `world/TerrainLoader.cpp` (1), with six `#include <emscripten…>`.
  No target compiles them since the wasm targets were deleted, so they are unbuilt code inside `src/`
  — the one thing `-Werror` cannot see. `HttpPost.cpp:17` additionally justifies a static's lifetime
  with `-sEXIT_RUNTIME=0 (sim/Makefile)`, a flag that no longer exists anywhere in the tree. Right:
  the branches leave with the round that builds `src/host/`, and until then nothing may be added to
  them.

- **Nothing tests that a cumulative counter survives a 32-bit target.** `verify-counters` did, in the
  browser, and went with it: last green 2026-08-12 on Chromium 151.0.7922.34, `poolPosts=220
  poolRepeats=2147999561 sizeofLong=4`. `TilePool.h:52` still declares its ledger `long long` for
  exactly this reason and the comment naming it is now the only thing holding it. Right: the check is
  a property of the declarations, not of a platform — a static assertion over the ledger's field
  widths costs nothing and needs no 32-bit host.

- **`clients/Walker.h`/`Walker.cpp` has no caller.** The browser shell that steered it is deleted and
  `test/clients/AppWalk.cpp:101` refuses an interactive scene, so the walking verb is compiled (it is
  in `APP_SRCS`, so it cannot rot) and constructed nowhere. Right: the client with an input medium is
  its caller, or it goes with the round that decides there will not be one.

- **`tiles/src/wxfmt.h:6` names two things that do not exist.** `sim/src/core/FBWxFormat.h` moved to
  `src/core/FBWxFormat.h` on 2026-08-12, and the checker the line credits with holding the contract —
  "sim's `build/fb-test-weather`" — has no target in any Makefile in this tree and no source behind
  it. The wire format's one written statement of who verifies it verifies nothing. Right: the path,
  and either a test that reads both headers or no claim that one exists.

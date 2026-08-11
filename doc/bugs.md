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
- **`treebench` measures nothing and reports success.** `clients/TreeBench.cpp:98` — `PlantsIn(assets)` on a missing or empty directory returns an empty list, the header prints, no row follows and the exit code is 0. Verified: `treebench --assets /private/tmp/nope-does-not-exist` exits 0. The round that made the bench enumerate its directory did so precisely because "a form nobody grew would otherwise have looked green"; zero forms still looks green. Right: an empty species set is a refusal that names the directory it looked in.
- **`emscripten_exit_pointerlock()` discards its result** (`clients/AppWasm.cpp:92`).
- **A status that is read into a variable and only logged is still discarded.** `BindInput`
  (`clients/AppWasm.cpp:236-266`) now routes all six registrations through `Bound`, which logs
  `input_unbound` on failure and **returns anyway**; the six results become six fields of one
  `input_bound` line and nothing branches on them. The comment above `Bound` states the class
  correctly — "the picture comes up with no input and no reason" — and then the picture still comes
  up. `[[nodiscard]]` cannot catch this shape: assignment satisfies it. Right: a keyboard that did not
  bind is a **refusal to run**, not a log line, because every verb the client has goes through it.
- **`[[nodiscard]]` is absent from the two directories that most need it.** 33 in the tree against
  **134** functions returning `bool` or a status enumeration, and the distribution is the finding, not
  the ratio: `generators/` 21, `clients/` 7, `core/` 3 — and **`world/` 0 against 29 such functions**,
  **`render/` 0 against 12**, `world/terrain/` 0, `core/io/` 0. `world/` is where `Pending`, `Absent`
  and `Ready` live, i.e. where every streaming defect in this file was found. The attribute is free,
  costs no frame time and is a compile error under `-Werror`.

## Bounds, allocation, and what the platform hides

*Measured 2026-08-11 in `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad`,
emsdk 6.0.3 / node 26.7.0 / clang, all at `-O2`: an index 400 kB past a live `std::vector` writes real
bytes and exits 0 **in the browser and on the native oracle alike** — the address is inside a mapped
heap in both cases. The premise "it segfaults natively" holds only for a write that leaves the mapping,
which a heap overrun almost never does. So the oracle is not louder than the browser for this class,
and the conclusion is stronger rather than weaker: there is no safety net on either target today.*

- **An exhausted heap is reported as malformed terrain.** `world/ChunkMesh.h:47,48,91,147` allocate
  `NN·3·8 + NN·4 + NN·12` bytes plus the vertex block per tile (1.59 + 0.26 + 0.79 MB at a 257×257 z14
  grid, ×4 workers), check the result, and answer failure with `return 0` — **the same value written
  three lines above for `return 0; /* terrain is always a grid; refuse soup */`.** The caller cannot
  tell "this DEM is not a grid" from "the 296 MB heap is full", so a tile that failed to allocate is
  dropped as bad data, silently, at exactly the moment the heap is fullest. `core/io/Heap.h` exists for
  this: `Heap::Exhausted("chunk postings")` ends the run naming the item. **This is a candidate root
  cause for "the client freezes after a few hundred metres", and it is a hypothesis, not a finding** —
  what makes it plausible is that nothing evicts (see below) while `heapPeakKB` already reads 207 460 of
  296 MB at rest (`sim/logs/demo-walk-wasm-20260811T181012Z.csv`); what would decide it is the same walk
  with the four sites routed through `Heap`, where the run then ends with a line instead of stopping.
  The same defect, without the confusion, at `world/terrain/terrain.cpp:85,136`,
  `world/terrain/osmmesh_terrain.cpp:49,67,130,227,245` and `clients/SimHost.cpp:186` — nine `malloc`
  sites in a tree whose global `operator new` has ended the run properly since `core/io/Heap.cpp`
  landed.
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
- **An `Absent` tile is answered once and reads as `Pending` for ever after** (`world/TilePool.cpp:534-540`, `Poll`). The result is erased from `Done_` unconditionally while the key deliberately stays in `Posted_`, so every later ask finds no result, fails to post, and returns `Pending`. Decidable from the code: `meshAbsent` can rise at most once per tile while `meshWaiting` keeps rising for it for ever. Right: the verdict belongs in the cache's own absent state, which `Lookup` already consults, not in the queue's posting set.
- **`World::AdmitMesh` names `Absent` and does nothing with it** (`world/World.cpp:409-411`) — the second half of the bullet above and the load-bearing one. The `switch` arm increments a counter and returns; `nd.haveMesh` stays 0, so the leaf stays in `TargetTot`'s unready set whether or not the pool ever answers again, and `Resident()` can never become true. **A hole in the DEM holds the loading screen for ever, and fixing `TilePool::Poll` alone does not fix it.** This is the silent-success shape at the top of this file with a `case` label instead of a `void` return: the three-state reply is now spelled out and two of the three states behave identically. Right: an absent leaf is a terminal node state (`nd.absent = 1`), excluded from `CountTargets`' unready set and from `AddWork`, so a tile that does not exist stops being asked for.
- **The per-pass build budget bounds installs, not asks.** `world/World.cpp:390-417` decrements `budget` only in the `Ready` arm, so a pass the pool cannot answer asks **every** candidate and spends nothing: the cost of a stalled pass is O(wanted), not O(2). Measured over `demo/crossing` (900 frames plus load, `sim/logs/demo-crossing-gpu_walk-20260811T172219Z.csv`): `meshCapped` 217 against `meshWanted` 2 029 402, i.e. 0.011 % of wants. Two separate things are wrong: (a) the ask is unbounded, which is what `doc/requirements.md` § 0.2 calls the missing second cap — *how many may start* per update; (b) even as an install cap, 2 is not the binding constraint and neither is the in-flight cap. The binding constraint is **CPU inside the mesh build**: `world tilepool_closed` for the same run reports `meshCpuMsPerTile = 237.29` over 4 threads = 16.9 tiles/s, against a measured drain of 12–13/s (`poolQueued` 116→0 while `meshAdmitted` 11→130 in 9 s) and 118/s admissible at 59 fps. Do not conclude that the cap is useless — it is the only bound on a warm-cache teleport; conclude that it is measured against the wrong thing, and that a queue that empties is not a pool that is fast.
- **The load loop polls the pool ~190 000 times a second** (`poolRepeats` 2 069 319 against `poolPosts` 196, same run). Attribution matters and the earlier phrasing had it wrong: **99.99 % of the repeats happen before residency, not during play.** In `demo/walk-500` `poolRepeats` is 1 953 923 at t=10 s (residency) and 1 954 287 at t=183 s — 364 repeats in 173 s of walking against 1.95 M in 10 s of loading. The load spins `Refine` at ~1 800 passes/s × ~119 unready leaves; every one of those takes `QueueMutex_` and attempts a `std::set<uint64_t>` insert on the thread that draws, against the four workers that need the same mutex to pop (`world tilepool threads=4 inFlightCap=4`). The host was loaded during the measurement, which makes 190 kHz a **lower** bound. Right: a pending ask answerable without the queue's lock — a per-node "already posted" flag in the node, or an atomic set — and a load loop that is not a spin.
- **The counters that diagnose a stuck load overflow during a stuck load.** `clients/StreamTelemetry.cpp:102-112` narrows six `long` admission counters and `poolPosts`/`poolRepeats` to `int` because `core/io/Telemetry.h:29` has no wider `Push`. `meshWanted` reaches 2 029 402 in 11 s of load and `poolRepeats` 2 069 319 in the same 11 s; at that rate `INT_MAX` is 3.2 hours away, and 3.2 hours of loading is exactly the run the Absent defect above produces. The column then goes negative and both published identities break, which reads as "a counter stopped being taken". Right: `void TelemetryRow::Push(long long)`, and no cast at any call site — `ES.46`, and the cast is the only reason the narrowing is not a warning.
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

## Declaration and build

- **`core/ClusterDag.h:72` reads `FB_TAU` from the environment** — the picture depends on an undeclared variable. **And it is one of six.** Also live: `FB_TAA` (`render/Renderer.h:318`, default on) switches temporal antialiasing, which changes both the pixels and `frameMs`; `FB_GEOM` (`render/GeometryIsolation.h:15`) disarms the shadow receivers; `FB_MOON_SCALE` (`Renderer.h:230,360`, applied at `Renderer.cpp:399`) scales the moon off its real 0.0045 rad; `FB_GROUND_CLASS_VIZ` (`TerrainDraw.cpp:642`) and `FB_TONE_PROBE` (`TaaStage.cpp:226`) replace the fragment outright. **None of the six appears in any telemetry column**, so two runs of one wasm hash are not comparable and no CSV can say which picture it measured — which is the same defect as a resolution that moves under load, wearing a different hat. Right: the four that change the picture leave the environment entirely; the two diagnostics stay and ride a published column.
- **`core/Mat4.h` is entirely dead, and the comment defending it names a test that does not exist.** `Mat4Identity`, `Mat4Mul`, `Mat4Perspective`, `Mat4LookAt`, `Vec3Normalize` and `Vec3Cross` have no caller outside `core/Camera.h`; inside `Camera.h`, `CameraBasisFrom`, `CameraAxes`, `HorizonDipRad`, `MvpTranslate`, `Frustum`, `FrustumFrom` and `AabbVisible` have none either. `CameraBasisEcef` is the only live function in the pair (`clients/Sim.cpp:497`, `clients/SubjectBench.cpp:239`) — verified repo-wide, not only under `sim/src`. `Camera.h:76` asserts "CameraBasisFrom above is NOT dead: sky dome and star field are an infinity pass in LOCAL render-ENU"; `SkyStage` and `StarsStage` call nothing in the file. Two comments say "Pinned in `test_camera.c`"; no such file exists anywhere in the tree. Three consequences, worst first: the dead `Mat4Perspective` builds a **GL-style [-1,1] reversed-Z** projection, so anyone reviving it under WebGPU's [0,1] clip volume silently loses everything past the mid-range; `outshine::Frustum` (`Camera.h:132`) and `outshine::Render::Frustum` (`render/Frustum.h`) are two spellings of one statement against "every statement has exactly one place"; and a false comment is worse than no comment. Right: delete `core/Mat4.h` and everything in `core/Camera.h` but `CameraBasisEcef`.
- **Five WGSL constants that decide the canopy carry no origin.** `render/Sward.h:59-63` declares `kMinSinEl 0.05`, `kLeafTrans 0.85`, `kTransIso 0.35`, `kTransFwd 2.6` and `kTransP 4.0` with no `[SET]`, no derivation and no unit — alone among the twenty constants in that function, and against the rule that every number carries its origin. Two are load-bearing. `kLeafTrans` multiplies the leaf colour on the transmitted path and its **name is the trap**: a green leaf at 550 nm has R ≈ 0.10 and T ≈ 0.085, so a literal "leaf transmittance" is 0.08 and someone will one day write it there and lose the whole back-lit canopy; what makes 0.85 right is that it is the *ratio* T/R, consistent with `kScatCut = sqrt(1 - ω)` and `ω = R + T = 0.185` on the line above. And `kTransIso + kTransFwd·cos^kTransP` is divided by the same `kInvPi` a Lambertian gets, with no statement anywhere that its hemispherical integral is 1 — so the transmitted path is not shown to conserve what the reflected path gives up. Right: one origin line per constant; rename `kLeafTrans` to what it is; and either normalise the lobe or state the deviation beside it.
- **Two headers guard themselves with reserved identifiers.** `core/Ephemeris.h:6` `#ifndef _EPHEMERIS_H` and `core/State.h:3` `#ifndef _FBSTATE_H`. A leading underscore followed by a capital is reserved to the implementation **in every scope** ([lex.name]/3) — undefined behaviour, not a style preference, and the rest of the tree already spells it `GEODESY_H`.
- **The winding is hard-coded at seven sites**; it belongs in the draw product beside the cluster list.
- **A stage that reports the same number every frame is reporting that it is not being measured.** In `after/town-spin.csv` (a rotation about a fixed point) `worldMs`, `meshMs`, `uploadMs`, `buildingMs`, `classMs`, `populateMs`, `nodes`, `drawnLeaves`, `draws`, `built` and `evicted` are all constant across 240 rows, and `distM` is 0.000 in every one — that run is not motion acceptance and was reported as if it were. Note the general form of the claim is false: of 89 columns in a live walk, 35 are constant and most legitimately so (identity, declared limits, stack ceilings). What is suspect is a *cost* column that does not move.
- **The log's timestamp is dead** — every `walk key` line carries `t=0.0` — and key repeat events are logged individually, so a held key floods the buffer.
- **`FacadeUv.h` has no `static_assert` anywhere**: 11 enumerators against a stride of 16, `kStyleCount 8` against 7 enumerators. A 17th `Facade` silently aliases identity 1.
- **`TreeGrower::GrowOnce` is ~130 lines** (`F.2`/`F.3`), and `TreeSpecies::Parse` is a 90-line flat key list (`F.3`).

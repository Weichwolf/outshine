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
  `test/generators/SameRegionSamePlacement.cpp:410` already constructs a `Structure` with
  `FeatureLevel::None()` — so the rule is one ingest site away from being false. If it ever is: a
  top-less feature enters the `highest`/`deepest` comparison at 0.0 m ASL, beating every declared top
  below sea level and losing to every one above, and at `Water.cpp:53` becoming a water level whose
  `WaterDepth::Between(0.0, ground)` reports `LevelBelowGround` for every dry-land outline — a missing
  datum counted as a disagreement between two models.
  `test/clients/WorldMain.cpp:64-65` writes `waterDepthM=0` for a dry place, but the same row already
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
  map as its own neighbourhood. **No longer reachable from a declared scenario** (2026-08-12): a world
  stage carries a `Scenario::Standpoint`, a type only the refusing factory `Standpoint::At` mints
  (`scenario/Standpoint.h`), so an out-of-band place is not a value a declaration can produce. It is
  still reachable from every other direction — `World::Open` (`world/World.cpp:79`) has no latitude
  guard at all, and `Sim::SetStance` takes two bare doubles (see the snapshot entry below).
  It is **one function at two sites, not a habit** — checked: of the seven `int`-returning C-ABI
  functions in `world/terrain/`, the other six are checked at every call site
  (`osmmesh_create` ×2, `osmmesh_fetch_tile`, `osmmesh_tile_grid`, `osmmesh_enu_init`,
  `osmmesh_terrain_decode_png`, `osmmesh_terrain_build_mesh`). What *is* systemic is that no mechanism
  reaches any of them: `int` is neither `bool` nor an enumeration, so the `[[nodiscard]]` sweep and
  every gate pass over the whole directory.
  Right: the range refusal reaches `World::Open` and is named. And note what it costs to say
  otherwise — Web Mercator ends at ±85.0511° by construction, so *every point on Earth is a valid
  start* is a claim the tile scheme does not hold and a refusal is the honest half of it.

## The test harness and its instruments

- **`verify-data`'s strongest claim passes vacuously, and the claim itself is narrower than the
  tree.** The gate ends with
  `n=$(nm -u build/obj-walk/data-*.o build/obj-walk/core-*.o build/obj-walk/world-*.o 2>/dev/null | grep -c curl_ || true)`
  and has **no prerequisite on `walk`**, while `GATES_BUILD` lists `verify-data` *before*
  `verify-walk`, which is the target that builds those objects. With the directory absent the
  pipeline yields `n=0` and the gate prints *"no transport symbol in the library"* — verified
  2026-08-12 against a path that does not exist. Second half of the same defect: the three groups it
  reads are not the library. **`build/obj-walk/app-HttpPost.o` carries 7 `curl_` symbols**, measured
  by `nm -u` per group (core 0 · data 0 · gen 0 · world 0 · sim 0 · render 0 · **app 7** · host 8) —
  `src/clients/HttpPost.cpp` is library source under `src/`, includes `<curl/curl.h>` directly and
  carries an `EM_JS` arm beside it, and `ServerLog`/`ServerTelemetry` are its consumers. So the round
  declared a host seam for the ingress wire and left the egress wire hard-wired, and the gate looks
  past it. Right: `verify-data: walk`, a refusal when the object directory is empty, and every
  `src/`-derived group in the `nm` set — then `HttpPost` has to move behind a declared seam
  (`doc/requirements.md` § I.22) rather than being invisible.
- **The registry's and the store's counters have no reader.** `Data::SourceSet::Ledger` publishes
  nine (`Asked`, `Delivered`, `HandedOver`, `Vacant`, `Undeclared`, `Refused`, `Retried`,
  `FromStore`, `DeliveredBytes`) and `Data::ContentStore::Ledger` six (`Hits`, `Misses`, `Writes`,
  `WriteFailures`, `Swept`, `SweptBytes`); `grep` for any of them outside `src/data/` and the two
  unit tests returns nothing, so no telemetry row and no close-out line carries one. The consequence
  is measured: `demo frame` reports `fetches=309 fetchedMB=28.265 fetchMs=202.5` at
  `tilepool_closed`, i.e. 131 MB/s, which is a warm store and not a network — and **the record does
  not say so**, it has to be divided out. The store's hit rate is the one number that decides whether
  the store is doing anything. `Per.6`, and § I.23's zero-consumer rule applies to a counter as much
  as to a constant. Right: both ledgers ride `tilepool_closed` and the ordinary row.
- **Two tests hold each other's claim.** `test/data/OutsideIsNeverAsked.cpp` never puts an
  `Outside` source beside an `Inside` one and shows the first was not begun; its four blocks check
  that an *uncovered request* answers `Undeclared`. The claim its name makes is held one file over,
  in `test/data/AbsenceHandsOver.cpp:177-195` (`outside->Asked == 0`). Both claims are held by the
  pair and neither file holds the one it is named for, so a reader looking for the coverage rule
  opens the wrong file — `NL.1`'s reason applied to a filename. The same file's
  `CountingTransport` comment reads *"a transport that would fail the test if it were ever used"*
  and it is used, twice, answering 200. Right: the names follow the strongest claim in each file, or
  the block moves.
- **`verify-walk-asan` cannot see a stack lifetime error.** `Makefile:384-400` runs
  `build/gpu_walk_asan` with no `ASAN_OPTIONS`, and `detect_stack_use_after_return` is **off by
  default** — so the one class the tree just created three instances of (see *The tile pool's worker
  threads outlive the registry*) is outside the sanitised run's reach. Verified by running the same
  two scenes with the option on: both silent, but only because both drain their queue. Right:
  the gate sets it, and says which options it set in its own line.
- **An interrupted instrument leaves something bound, and the next run goes red for a reason that has
  nothing to do with the code.** The named instance is gone (2026-08-12): `verify-still` no longer
  starts `test/world/tile_delay.py` on `:8171` with `&` inside a recipe and kills it by pid — the
  arrival order is imposed in process by `test/host/DelayedTransport`, and the file is deleted. **The
  class is not**: a make recipe has no job control, so anything a recipe backgrounds is not a process
  group of its own and survives any exit between the `&` and the `kill`. Reported by the architect
  when the proxy existed: a `make gates` run went red because a previous interrupted `verify-still`
  still held the port. The same shape waits in `verify-refusals` and in every future instrument that
  binds or sleeps. Right, and `test/run.sh` now carries it end to end: `set -m`, so every child is a process
  group; one trap on `INT`/`TERM`/`HUP`/`EXIT` that kills the groups it started; and every kill a
  group kill, so a watchdog's `sleep` and whatever the subject itself left running die with it.
  Measured on the harness: 1 test process, 1 watchdog `sleep`, 1 leaked grandchild alive mid-run, all
  three gone one second after `SIGINT`, and the run exits 130. **Confirmed independently 2026-08-12**
  with `ps -o pid,ppid,pgid`, which shows why it works and where it stops: the test is pgid = its own
  pid, the watchdog is a second group, and the leaked grandchild is already `ppid 1` yet still carries
  the **test's pgid** — reachable only by the group kill. A grandchild that calls `setsid()` escapes
  it, and no instrument in this tree does. The eleven `verify-*` recipes still carry the defect. **Do
  not copy this shape into eleven recipes** — it is eleven copies of one statement (`ES.3`) in the
  language that made the first copy wrong. Either one recipe-level helper both `verify-still` and
  `verify-refusals` call, or — the direction § I.20 already names — the instrument moves inside the
  harness with `make gates`, and then there is one lifecycle instead of eleven.

- **A trailer the reporter did not write is accepted, so a file that never includes `Check.h` can
  print a green verdict over its own failure.** `test/run.sh:216-229` authenticates the trailer by
  shape alone — one line matching `^CHECKS `, six fields, three numbers. Demonstrated 2026-08-12
  against the repaired harness: a `test/harness/ForgedTrailer.cpp` containing no `#include "Check.h"`
  at all, printing `FAIL something/actually.cpp:1 …` and then `CHECKS 1 FAILURES 0 SKIPPED 0`, and
  returning 0, was reported **`PASS`** and the run exited 0. This is the same defect the round closed,
  one layer in: the channel is now the right one and nothing checks that the reporter is what spoke.
  Right, and it costs two lines and no change to `Check.h`: every increment of `Failures` prints
  exactly one line beginning `FAIL ` (`Check.h:49`, `:61`, `:89`) and every `Skips` exactly one
  beginning `SKIP ` (`:80`), so `grep -c '^FAIL '` **must** equal `FAILURES` and `grep -c '^SKIP '`
  must equal `SKIPPED`. That is a second witness on an independent path — per-failure `printf` against
  a counter — and it also catches a counter zeroed by any spelling `Tally` does not forbid (placement
  new, `memcpy`). Verified against all twelve logs the harness has produced here, planted probes
  included: the identity holds exactly in every one. `CHECKS` has no printed witness and stays
  single-sourced.

- **A hard error stops the run, so one malformed test hides the verdict of every test after it.**
  `test/run.sh:218`, `:222`, `:224`, `:324` all `Die`, which exits 2 mid-loop. Demonstrated: with a
  planted `(void)Report()` test present, the run printed two passes, died on the disagreement and
  never built the two tests that followed. `Makefile:409` states the opposite rule for gates in its own
  words — *"Every gate runs even after one has fallen, because the second failure is information the
  first one would have hidden"* — and the harness is the instrument that rule matters most in. Right:
  a missing, doubled, malformed or disagreeing trailer is a per-test verdict of its own that is red and
  counted, the loop continues, and the run exits non-zero. Only the pre-flight directory scan
  (`:256-268`) refuses before anything is built, which is correct there because nothing has run yet.

- **The harness's build cache is keyed by path relative to the root, so two checkouts of this tree
  share objects, logs and binaries.** `test/run.sh:41-42` fixes `BUILD=$TMPDIR/outshine-tests` and
  `:159` names an object `$BUILD/obj/src-core-Foo.o` with no component identifying the root; `:287-288`
  do the same for the log and the binary. `UpToDate` then compares mtimes of prerequisites resolved
  against the *current* root, so a second checkout whose sources are older than the first's objects
  links the **first checkout's** binaries and every number read from them belongs to the other tree. A
  git worktree and a `git bisect` clone are ordinary, and the effect is silent. Observed here: a probe
  root at a different path linked this tree's objects and ran in 1.2 s instead of a cold build. Right,
  one line: fold the root's real path into the build directory, e.g.
  `BUILD=${TMPDIR}/outshine-tests/$(printf %s "$ROOT" | cksum | cut -d' ' -f1)`.

- **A directory declared as the Makefile's is trusted, and a test placed in one is silently not run.**
  `test/run.sh` `NotTheHarnesses` names `.`, `clients`, `generators` and `compile/*` as directories the
  harness does not build; a `.cpp` there that includes the reporter and checks claims is run by
  nothing, which is the silent non-test one level up from the one just closed. It is bounded — a
  directory that is in neither list is a hard error before anything is built — but it is not closed.
  **Demonstrated 2026-08-12**: `test/compile/core/ARealTestInAMakefileDirectory.cpp`, one failing
  `CHECK` and `return Report()`, is named by no line of the run's output and the run exits 0.
  The deeper shape is not the four strings, it is that **the same fact is stated twice** — which files
  the Makefile builds is the Makefile's, and `NotTheHarnesses` restates it with nothing failing when
  the two disagree, exactly the defect § I.20 already files against the duplicated `INC_*` sets. Right,
  and it lands without moving a file: derive the second list instead of writing it. Every `.cpp` under
  a non-layer directory must be named in the `Makefile` — by path, or by the stem the Makefile composes
  (`GEN_NEGATIVES`) — and every `test/…cpp` the Makefile names must lie in a non-layer directory. Run
  by hand over the tree at this commit: **12 Makefile-owned sources, all 12 named, no stray** — so the
  cross-check is green today and its cost is about six lines of shell. The direction beyond that is
  `doc/todo.md`'s: nothing under `test/` that is not a declared run, at which point `NotTheHarnesses`
  has nothing left to name and goes.

- **The unit-height check accepts 168× the worst deviation it measures, and it bypasses the reporter's
  own rule about tolerances.** `test/generators/draw/GrownBarkIsAClosedMesh.cpp:227` judges with a raw
  `std::fabs(v.DeclaredExtent - 1.0) > 1e-5` rather than `CHECK_NEAR`, so the number that decides an
  acceptance carries no origin and no frame of reference — the thing `Check.h:52-54` was written to
  forbid. Measured over all 31 declarations: the worst deviation is **5.96046448e-08 in `dog_rose`**,
  which is `2^-24` exactly, the float spacing immediately below 1.0 — one ulp, and the other 30 land on
  1.0 bit-for-bit. `1e-5` is 168 ulps, so a normalisation that drifted to 0.99999 passes. Right:
  `CHECK_NEAR(extent, 1.0, 2.4e-7 /* 4 ulp at 1.0 */, "of height", …)`, with the ulp derivation beside
  it. Note also what the lying branch proves: `DeclaredExtent` (`:101-107`) re-decides `GrowthForm::
  Lying` the same way `TreeGrower::NormalizeToUnitHeight` (`TreeGrower.cpp:607`) does, so for a lying
  form the check is *consistency* between two copies of one predicate and not the decidable class —
  only the standing case is decidable.

## Bounds, allocation, and what the platform hides

*Measured 2026-08-11 in `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad`,
emsdk 6.0.3 / node 26.7.0 / clang, all at `-O2`: an index 400 kB past a live `std::vector` writes real
bytes and exits 0 **in the browser and on the native oracle alike** — the address is inside a mapped
heap in both cases. The premise "it segfaults natively" holds only for a write that leaves the mapping,
which a heap overrun almost never does. So the oracle is not louder than the browser for this class,
and the conclusion is stronger rather than weaker: there is no safety net on either target today.*

- **An exhausted heap is reported as malformed terrain — at eight sites, and no longer at the four that
  mattered most.** `world/ChunkMesh.h:50,51,89,141` now take their `NN·3·8 + NN·4 + NN·12` bytes and the
  vertex block through `Heap::Take` (since `1424214`), which ends the run naming the item and the count,
  so the "no mesh"/"no memory" confusion is gone there and the earlier description of those four as
  `malloc`+`return 0` was stale. It is live at `world/terrain/terrain.cpp:85,136`,
  `world/terrain/osmmesh_terrain.cpp:49,67,130` plus two `calloc` at `227,245`, and
  and one more in `clients/SimHost.cpp:186` until `b83285f` deleted it: **seven** `malloc` and two `calloc` when this was counted, **six and two** now, in a tree whose global `operator new` has
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

## Vegetation

- **A weeping willow is drawn with 22.5 % of its bark below the point it is planted at, and no shoot
  is constrained at the ground.** `assets/world/species/willow.json` (*Salix* × *sepulcralis*,
  `height_m: 18`, `crown: weeping`) grows a bark mesh whose lowest vertex sits at **−0.9334 of the
  tree's own height** — **16.8 m below the trunk foot** — and **14 493 of its 64 539 bark vertices,
  22.5 %**, are under that plane. Six further declarations do the same, less far: dog_rose −0.542,
  blackthorn −0.229, hedge_hornbeam −0.117, hawthorn −0.112, hedge_privet −0.082, guelder_rose −0.064.
  Measured 2026-08-12 over all 31 declarations under `assets/world/species/`, native, at full detail;
  the figures above are bark only and are what `test/generators/draw/GrownBarkIsAClosedMesh.cpp`
  prints. *The leaf-point figures and the six "above the plane by 3.6e-5…1.2e-4" figures this entry
  carried before came from a probe that is not in the tree — that test measures neither — and are
  withdrawn until something measures them.* Nothing downstream lifts the mesh:
  `TreePrototype.cpp:111` copies `BoxMin.Y` into `Crown_.Bottom`, which only bounds the in-crown query
  (`clients/StandField.cpp:43`) and the impostor box (`render/stages/ModelDraw.cpp:749`), and the
  instance transform puts y = 0 on the terrain. Two consequences, and they are of different kinds. The
  **cost** is measured: on flat ground a fifth of every willow's bark is transformed every frame for
  geometry that cannot be seen — vertex work only, since terrain depth kills most of those fragments
  before shading, so do not claim the fragment half without measuring it. The **picture** is inferred
  and not yet measured: geometry 16.8 m under the planting point emerges wherever terrain falls away
  faster than that within the crown radius, which is the bank of a watercourse, and a willow is placed
  on banks. It is decidable from one frame at a declared riverside standpoint and nobody has taken it.
  **The contract reading in the entry this replaces was wrong, and the harmless explanation is three
  lines from the code that produces the number.** `TreeMesh.h:1-3`'s *"base at y = 0"* means the
  **trunk foot**, not the mesh minimum, and `TreeGrower.cpp:598-601` says so in its own words —
  *"Y = 0 IS THE TRUNK FOOT, not the lowest vertex … a branch below zero belongs below the terrain;
  that is where it grows"* — with a measurement behind it (taking the mesh minimum put a willow's foot
  6.87 m and a spruce's 3.67 m above the ground). The mesh therefore honours its contract, and this is
  not a contract violation. It is a **missing** constraint, so the feature line is
  `doc/requirements.md` § III.2 *A shoot stops at the ground*; what stands here is the picture and the
  cost the absent constraint produces today.
  The grower's ruling is right for a spruce skirt at −0.02 of height and wrong at −0.93: *Salix* ×
  *sepulcralis*'s branchlets tumble **to touch** the ground ([RHS](https://www.rhs.org.uk/plants/81798/salix-sepulcralis-var-chrysocoma/details))
  and *Rosa canina*'s arching stems climb **up** through neighbouring shrubs to 1–5 m
  ([RHS](https://www.rhs.org.uk/plants/16017/rosa-canina-s/details)) — neither grows downward.
  Right: the hanging tip is clamped at the base plane the way it is already bent back at the crown
  envelope, and the permitted dip is one small sourced number, not a free consequence of shoot length.
  Decides it: `min y ≥ −δ` over bark **and** leaf points for every declaration, in the test that
  already measures the bark half.

- **`TreeMesh.h`'s stated contract has two readings and the wrong one has already produced a wrong
  check.** `TreeMesh.h:1-3` says *"Delivered NORMALISED — base at y = 0, centred in x/z, height
  exactly 1"*. Read as the bounding box, all three clauses are false: measured over the 31
  declarations, `BoxMin.Y` is negative for 7 and non-zero for 13, `hedge_privet` spans x
  −1.320…+1.309 and `log_beech` x −0.013…+0.987, and only the **box** reaches y = 1 — the topmost
  bark vertex is below it, because the finest shoots are leaf points. Read as *the origin is the trunk
  foot and the extent is measured from it along the axis `height_m` names*, all three hold, and that
  is what `TreeGrower.cpp:580-608` implements and says. The ambiguity is not academic: it is exactly
  what made `BarkVerdict::HighestY` measure the bark maximum while its author believed it was checking
  *"height exactly 1"* (`doc/requirements.md` § I.20 still restates the ambiguous words). This is a
  comment stating an invariant, so it is `I.7`/`NL.2` and not taste. Right, and it is three lines:
  say origin, not base — *origin at the trunk foot; `BoxMax.Y` = 1 for a standing form and the larger
  horizontal run = 1 for a lying one; `BoxMin.Y` may be negative and a branch below it grows below the
  terrain*. Decides it: the sentence names which axis and which extremum, so a reader can write the
  check without opening `TreeGrower.cpp`.

## World and streaming

- **The tile pool's worker threads outlive the registry, the content store and the transport they
  borrow.** `clients/Sim.h:231` declares `World::World W_;` and `:271-274` declare `Wire_`, `Store_`,
  `Content_` and `Sources_` **after** it. Members are destroyed in reverse declaration order, so
  `~Sim` destroys the `SourceSet` and the `ContentStore` first and reaches `~W_` — which is the only
  thing that calls `fb_stream_close()` → `~TilePool` → `join()` on the workers — forty declarations
  later. Every worker inside `TilePool::FetchInto` holds `Data::SourceSet &Sources_` and
  `Data::Transport &Wire_`, and `SourceSet::Collect` reads `Store_` on the way in. The same inversion
  is written twice more at the entry points: `test/clients/AppWalk.cpp:75` constructs
  `Clients::Outshine app` before `:83-84` construct `Host::CurlTransport wire` and
  `Host::DelayedTransport ordered`, and `test/clients/WorldMain.cpp:100` constructs `sim` before
  `:104` constructs `wire` — so `~CurlTransport` joins its own threads and calls
  `curl_global_cleanup()` while the pool's are still running. `C.13` names exactly this
  (*"if data member `B` uses another data member `A`, declare `A` before `B`"*), `R.3`/`R.4` say the
  reference is non-owning, and `Lifetime.1` is the failure. **Latent, not live, and the harmless
  explanation was looked for and holds for the runs that exist**: `demo frame` and `demo walk-500`
  under `ASAN_OPTIONS=detect_stack_use_after_return=1` are both silent (measured 2026-08-12,
  `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad/sar/out.log`
  and `.../sar500/out.log`), because both reach `progress=1` with `fetchGaveUp=0` and drain the queue
  before exit. It fires the first time a run ends with a tile in flight — an early return between
  `Prepare` and the end of the traversal, a scene shorter than its stream, a refusal after `Open`.
  Right: the pool stops before the things it borrows, which is three line moves — `Sources_` and
  `Content_` declared before `W_`, the transport constructed before `app`. Unspellable rather than
  moved: `TilePool` is scoped inside whatever owns the registry, so the order cannot be written the
  other way round. **The gate that would catch it does not exist**: `verify-walk-asan` never sets
  `detect_stack_use_after_return`, which is off by default and is precisely this class.
- **A refusal carries no reason, which is the defect § I.22 was written to close, one level down.**
  `data/Fetched.h:14` `Meaning::Refused` is minted in `data/WebTileSource.cpp:52-54` from
  `Classify(status, size)` and **the status is dropped on the spot**; `data/Delivery.h:46`
  `Delivery::Wire()` carries nothing at all; `world/TilePool.cpp` logs `tile_refused` with only the
  request key. A 401, a 500 that outlived its four-try budget, a body over `CurlTransport::Config::
  MaxBodyBytes`, a `StarBands` file that would not open and a `TryTake` on a wire that did not answer
  are one value — four readings of one empty buffer, moved from the top of the type into its
  `Refused` arm. `data/` may not name `Log` by the section's own ruling (`-Isrc/core -Isrc/data`,
  `Makefile` `INC_DATA`), so the reason has to be a **value** and not a log line. Right: `Refused`
  carries the status, or a declared enumeration beside it, and `TilePool` writes what it was handed.
- **`Delivery::Undeclared` is retried for ever and the load never ends.** `world/TilePool.cpp`
  `FetchInto` maps `Delivery::State::Undeclared` to `Reply::Refused` under a comment that says *"it
  will not heal by asking again"*, and `world/World.cpp:447-449` then maps `Reply::Refused` to
  `Adm_.Waiting` — correct for a wire error, wrong for a declaration error. `World::CountTargets`
  counts a leaf settled on `Ready || Vacant` only, so a node no source covers never settles,
  `progress` never reaches 1, and every pass writes another `tile_undeclared` error line. Right:
  `Undeclared` is its own reply at the pool and it ends the run loudly (§ I.17) instead of sharing an
  arm with the refusal that does heal.
- **A `Data::SourceSet::Query` owns a live transfer and is not a resource handle.**
  `data/SourceSet.h:41-63` holds `Ticket_` with `Query(Query &&) = default` and **no destructor**;
  cancelling is `SourceSet::Abandon(query, transport)`, a free operation the caller must remember. A
  moved-from `Query` keeps the ticket value, so `Abandon` on either end cancels the other's transfer
  (`C.64`: a move must leave the source valid); a `Query` dropped without `Abandon` leaves a
  `CurlTransport::Transfer` — url, status and the whole body — in `Transfers_` for the life of the
  transport, because `test/host/CurlTransport.cpp:70-78,80-100` erase only on collect or cancel.
  Nothing leaks today: the one caller calls `Abandon` on the one path that needs it. The rule is
  written down and not carried — `R.1`, `C.30`, `C.31`. Right: the ticket is an owning handle that
  cancels in its own destructor, so a dropped query cannot leave a transfer running.
- **`TryTake` is documented as once-only on four types and is not.** `data/Transport.h:36` says
  *"hands the status and the body over exactly once"*; `Wire::TryTake`, `Fetched::TryTake`,
  `Delivery::TryTake` and `World::TerrainBytes::TryTake` all move the payload out and leave `Where_`
  unchanged, so a second call returns `true` with an empty body and a caller reads *delivered, zero
  bytes* — the exact reading the four states exist to make unspellable. Right: the take transitions
  the state, so the second call is `false`.
- **`Data::ContentStore`'s cap is enforced once, before the first write, and never during the run.**
  `data/ContentStore.cpp:46-77` sweeps in the constructor; `Keep` (`:106-132`) adds bytes and counts
  nothing. `ContentStore.h:37` says so in its own words. A long session grows the directory without
  bound — the same failure as the 7 GB store this replaces, moved from *no cap* to *a cap checked
  before anything is in it*; and the test that says *"the store is capped"*
  (`test/data/TheStoreNamesBytesByTheirKey.cpp:94-115`) only ever reopens a full directory, so it
  cannot see the difference. Right: a running byte total, and the sweep amortised over `Keep`.
- **The DEM path answers for a place the caller did not ask about, because the projection clamps.**
  `world/tiles/TileMath.h` `GeoToTileClamped` clamps the latitude into the Mercator band and then projects, so a
  standpoint outside the band gets the ground of the nearest point inside it with no refusal and no
  note. Measured on `fb_world` at 85.052 N / 15 E before the band was refused at the declaration: the
  terrain requests were `/t/terrain/14/8874/0` — x correct for 15 E, y clamped to row 0, whose
  southern edge is 85.0466 N — and the run reported `groundAslM=-3448.27` for a standpoint **97.3 m**
  north of anything the scheme can address, exit 0. The second spelling of the band left with the
  server (2026-08-12) and `world/tiles/TileMath.h:19 kMercatorLatMaxDeg = 85.05112877980659` is now
  the only one — but a third value is still *printed*: `Scene::Read` hands the constant to `Need`,
  whose message runs it through `std::to_string`, so a refused declaration reads `lat out of range
  [-85.051129,85.051129]` while `lat: 85.051129` is itself refused (measured on `fb_world`: 85.05112877
  exits 0, 85.05112878 and 85.051129 exit 2). The message names an admissible value that is not one,
  by 1.3 cm. Right: the clamping form refuses like `TileIndex::TryXy` does — it is the same
  projection — and the bound is declared once, which it now is. Priced at three call sites in
  `world/TerrainLoader.cpp`.
- **The point query is refused by nobody, and it is the `world` target's whole purpose.**
  `test/clients/WorldMain.cpp:117` reads `argv` pairs through `std::atof` straight into
  `Sim::At(double, double)`, which reaches `Region::Of` and `fb_stream_ground` with no band check
  anywhere on the path. Measured **after** the round that added the declaration-time refusal, against
  `localhost:8081`, scene declared at 78.2 N: `fb_world pctrl probe 86.0 15.0 89.9 15.0 78.2 15.0`
  exits 0 and answers `lat=86 groundResolved=1 groundAslM=-3448.27` and
  `lat=89.9 groundResolved=1 groundAslM=-3448.27` — **bit-identical for two places 424 km apart**,
  which is the clamp and not the bathymetry; the control at 78.2 N gives `-173.103`. `groundResolved=1`
  is the lie: the ground was resolved for 85.0511 N, not for the pole. Right: the same refusal the
  declaration gets, and it is not a third `if` — a standpoint type that cannot be constructed outside
  the band (`doc/requirements.md` § I.17), so `At` cannot be called with two loose doubles.
- **A standpoint that replaces the declared one is checked by nobody.** `test/clients/AppWalk.cpp:35-46`
  lets `shots.jsonl` replace lat/lon through `Sim::SetStance`, which is `clients/Sim.h:77`
  `void SetStance(const Stance &s) { if (!Opened_) Stance_ = s; }` — a setter that silently does
  nothing once the world is open and validates nothing when it does act. The band check therefore
  lives at the declaration (`scenario/Standpoint.h`, a type only a refusing factory mints) and this
  path goes around it by handing `Sim::Stance` two bare doubles: a hand-written
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
  `Makefile` `verify-still` runs the shipping client through `test/world/tile_delay.py`, which holds
  every tile response back by 0–400 ms, and sets `OUTSHINE_TILES` but not `OUTSHINE_SIM` — so each of
  the four seeded runs posts to fb-sim under the same identity as a declared measurement run
  (`test/clients/AppWalk.cpp:53-58`: `client=gpu_walk`, `mod=demo`, `scene=frame`). Measured on this host,
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

- **An imposed arrival order and the content store cannot be exercised in the same run.** The order
  instrument is a decorator over the host transport (`test/host/DelayedTransport.h`), and the store is
  consulted inside `Data::SourceSet::Collect` **before** the source is begun — so a store hit answers
  without a ticket ever existing and no delay can apply to it. `verify-still` therefore declares
  `OUTSHINE_NO_CONTENT_STORE=1` and pays four cold scene loads over the real upstreams (110 s measured
  2026-08-12, against 103 s for the proxy version in front of a warm container). Two things are wrong
  with that: the gate never exercises the store at all, and the one path a shipping run takes — a warm
  store — is the one path no gate imposes an order on. Right: the arrival order is a property of the
  *delivery* rather than of the wire, so the instrument belongs where both a store hit and a wire
  answer pass; that place is inside `SourceSet`, which is library, so it needs to be a declared
  library facility rather than a host decorator.

- **`Delivery::At` is exercised only by a test, never by a run.** A request above a source's last
  native zoom is answered from the ancestor and says so (`data/WebTileSource.cpp` `Serves`,
  `test/data/TheAnswerNamesItsAddress.cpp`), and the whole path is carried through the byte cache
  (`world/TilePool.h` `Landing`) because a crop computed from the requested address over an ancestor's
  pixels is a wrong picture drawn silently. **No run reaches it**: `world/World.cpp` `kMaxZ = 14`, the
  stitch asks at the same zoom, and the only elevation upstream declares `MaxZoom = 15`, so the
  difference is always zero. The mechanism is right and untested outside a unit test — which is the
  honest statement, not "it works". Right: the first scenario that asks above z15, or a declared run
  that narrows the registry to a source with a lower bound so the fill fires.

- **A remembered absence has no expiry on the client, and no weight in the cache that holds it.**
  The deleted server's disk cache gave a negative entry a lifetime (30 d) because an upstream 404 can
  be transient; nothing in the tree does now. `TilePool::Remember(key, nullptr, 0, at, true)`
  (`world/TilePool.cpp`) carries no timestamp, so `Lookup` answers `Reply::Absent` from it for the life of
  the process. Worse, it is stored with `len == 0`, so it adds nothing to `CacheBytes_` and the LRU
  loop — `while (!Cache_.empty() && CacheBytes_ + len > ByteBudget_)` — is never entered on its
  account. Over ground that is all 204 (open water, imagery gaps) `Cache_` grows in entry count while
  `CacheBytes_` stays at 0, and every byte request pays a linear scan of it under `CacheMutex_`.
  Decidable from the code, not measured. Right: the entry count is a budget of its own, and an
  absence carries the age at which it is re-asked.

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

- **`stitch_edge` asked for tiles that cannot exist.** The predecessor of `world/tiles/TerrainTiles.cpp`
  guarded the west and north neighbours against `x == 0` and `y == 0` and had no
  guard at the other end, so a tile on the map's east or south edge issued a GET for `x = 2^z` or
  `y = 2^z` — an address no source covers (`data/WebTileSource::Covers` refuses it now, which is where
  the guard belongs). It was invisible because it only fires at the edge of the world and
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
- **A failed allocation is reported as a refused tile instead of ending the run.** `world/TilePool.cpp` `RunMesh` reaches `Miss::Refused` when `ChunkBuildEcef` returns 0, and that function returns 0 from **three `malloc` failures** (`world/ChunkMesh.h:52,95,153` — see *An exhausted heap is reported as malformed terrain*, whose consequence this line states). An exhausted heap is not a statement about a tile and must not be reported as one. **The terminal-hole half of this is closed** (2026-08-12): the global `Classify` that turned every 4xx into `Absent` is gone — status-to-meaning is per source and declared now (`data/TerrariumDem.cpp`) — the thread-local `tMiss` is gone with it, and `RunMesh` maps only `Miss::Hole` to `Reply::Absent` while a refusal is `Reply::Refused`, which `World::AdmitMesh` retries rather than retracting the split. What is left is the allocation: it should reach `Heap::Exhausted` like the OOM path beside it, not the mesh verdict.
- **`poolPosts` and `poolRepeats` still wrap on wasm32.** The round that widened every counter to `long long` left `long Posts_ = 0, Repeats_ = 0` (`world/TilePool.h:177`) — the accumulators — and widened only the `Ledger` fields they are copied into, so the column is 64-bit-typed and 32-bit-valued. These are the two fastest counters in the tree: `poolRepeats` reaches **2 201 113 194 = 1.025 × 2³¹** in 2 868 rows of `sim/logs/demo-frame-gpu_walk-20260811T184845Z.csv`. That run shows no negative value because `walk` is the native oracle, where `long` is 64-bit — **the measurement used to clear the defect is the one build that cannot see it**. Signed overflow is UB (`ES.103`), not a wrapped column. Right: `long long`, and every 32-bit accumulator behind a 64-bit column found the same way — by type, not by looking at native output.
- **A completed mesh nobody asks for again is retained for the life of the pool.** `TilePool::Poll` erases a `Done_` entry only when a caller polls for it; a node that stops asking — which is exactly what the new retraction makes happen to a sibling whose build was in flight — leaves a full `TileBuild` (verts + indices + clusters, ≈ 4 MB at `kGrid = 128`) in the map for ever. Decidable from the same run: `tilepool_closed` reports `meshTiles=131 meshAbsent=1`, i.e. 130 completed builds, against `built=129` uploads (`world fbworld`, `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad/hole/afterF.log`). The round cleared `Build` for the `Absent` arm and not for this one. Right: the pool drops a `Ready` result whose key left the caller's cut, which needs the cut to be a thing the pool can be told about — see the cut-once shape below.
- **The drawn cut and the counted cut are two implementations that must agree, and nothing makes them.** `World::Descend` decides coverage with `Ready`, `World::CountTargets` decides progress with `Settled` (`Ready || Vacant`), and both re-derive the same tree from `Splits`. They agree today only because `Splits` removes a vacant child from *both* walks; delete that one call from `CountTargets` and progress reaches 1.0 over a square the draw pass leaves empty — the silent-hole failure, one edit away, with no test and no identity that catches it. `CanCover` makes it worse structurally: `Descend` calls it per child and it re-walks the whole subtree, so one pass is O(N·depth) node visits with a hash lookup each (`Splits` calls `Find` although `Descend` already holds the index), ~800 visits per root ring at ~1 800 passes/s during load. Right: **the cut is computed once per pass** into `(idx, role)` — target leaf · holder · drawn — and `Descend`, `CountTargets` and the request walk read it. Then two walks disagreeing is unspellable, the retraction is one rule in one place, and the duplicated `anyV`/`Wants`/`Ready`/`Emit` block the retraction added to `Descend` disappears with it.
- **A DEM hole deletes the built world standing on it, although the ground under it is drawn.** Measured 2026-08-11 over a synthesised hole (one z14 terrain tile, 8620/5404, answered 204 by a proxy in front of `fb-tiles`): the terrain cut retracts to the z13 parent and the picture is continuous, but `buildingVerts` falls 405 504 → 170 601 and one whole region grows nothing (`sim` `region_without_ground`, `clients/Sim.cpp` `Ask`). The cause is one answer used for two questions: `world/BuildingField.cpp:235` drops a footprint whose corner heights do not resolve (`NoGround_++`), and a region whose ground block is Missing is refused. But a height for that place **does** exist — it is the one the picture draws, the coarse ancestor's. Right: a place with no tile at the finest rung reads the finest rung that HAS one, so the footprint and the stand stand on the surface the eye sees; today the oracle answers only at `gSurface.Z` (`world/TerrainLoader.cpp:301`, *"no other zoom of this surface exists"*).
- **An absent tile is remembered for the life of the pool and nothing removes it.** `world/TilePool.cpp` `Poll` keeps the `Absent` result in `Done_` and the key in `Posted_` — that is what makes the answer final and stops a thread being spun on it — but nothing evicts either table, so a flight over a large hole grows both without bound. Bounded today by the number of distinct absent tiles a run asks about, which is 1 in every measured run. Right: the same unit of removal eviction needs everywhere else (see *Nothing evicts*), not a second mechanism.
- **The per-pass build budget bounds installs, not asks.** `world/World.cpp:390-417` decrements `budget` only in the `Ready` arm, so a pass the pool cannot answer asks **every** candidate and spends nothing: the cost of a stalled pass is O(wanted), not O(2). Measured over `demo/crossing` (900 frames plus load, `sim/logs/demo-crossing-gpu_walk-20260811T172219Z.csv`): `meshCapped` 217 against `meshWanted` 2 029 402, i.e. 0.011 % of wants. Two separate things are wrong: (a) the ask is unbounded, which is what `doc/requirements.md` § 0.2 calls the missing second cap — *how many may start* per update; (b) even as an install cap, 2 is not the binding constraint and neither is the in-flight cap. The binding constraint is **CPU inside the mesh build**: `world tilepool_closed` for the same run reports `meshCpuMsPerTile = 237.29` over 4 threads = 16.9 tiles/s, against a measured drain of 12–13/s (`poolQueued` 116→0 while `meshAdmitted` 11→130 in 9 s) and 118/s admissible at 59 fps. Do not conclude that the cap is useless — it is the only bound on a warm-cache teleport; conclude that it is measured against the wrong thing, and that a queue that empties is not a pool that is fast.
- **The load loop polls the pool ~190 000 times a second** (`poolRepeats` 2 069 319 against `poolPosts` 196, same run). Attribution matters and the earlier phrasing had it wrong: **99.99 % of the repeats happen before residency, not during play.** In `demo/walk-500` `poolRepeats` is 1 953 923 at t=10 s (residency) and 1 954 287 at t=183 s — 364 repeats in 173 s of walking against 1.95 M in 10 s of loading. The load spins `Refine` at ~1 800 passes/s × ~119 unready leaves; every one of those takes `QueueMutex_` and attempts a `std::set<uint64_t>` insert on the thread that draws, against the four workers that need the same mutex to pop (`world tilepool threads=4 inFlightCap=4`). The host was loaded during the measurement, which makes 190 kHz a **lower** bound. Right: a pending ask answerable without the queue's lock — a per-node "already posted" flag in the node, or an atomic set — and a load loop that is not a spin.
- **`TileEnuMap` answers with a plausible wrong place instead of refusing.** Every other type in
  `world/tiles/TileGeodesy.h` hides its payload behind a state — `TileIndex::TryXy`, `EnuFrame::
  TryFromGeo`, `TerrainGrid::TryField`, `TerrainMesh::TryPositionsEnuM`. `TileEnuMap` (`:153-175`) has
  no state, a private all-zero default body, and `Apply` is a plain total function; its own comment
  argues for it — *"A map over an unusable frame carries zero scale, so it places every vertex at the
  origin rather than somewhere plausible."* Placing every vertex of a tile at the frame origin **is**
  the plausible-looking wrong answer, and it is unreachable to a caller: nothing distinguishes a
  degenerate map from a good one. The guarantee the other four types establish leaks at the one seam
  that converts per-vertex positions. Right: `EnuFrame` exists only in its usable state — `At` returns
  the state-carrying answer and the frame is obtained through it — after which `TileEnuMap::Over` is
  total, `TryFromGeo`/`TryToGeo` lose their `bool`, and the degenerate map has no spelling (`C.41`,
  `C.42`).
- **The producer knows the grid's width and the consumer guesses it back with a 0.5 m threshold.**
  `world/tiles/TerrainGrid.cpp:61-62` computes `rowsOut`/`colsOut` and `TerrainMesh` keeps neither;
  `world/ChunkMesh.h:31-38` then recovers the column count by scanning for the first easting that
  decreases by more than `0.5f` metres and refuses the tile as *soup* when it finds none. The constant
  is a magic number (`ES.45`) standing in for an extent that was exact one call earlier, and it fails
  silently wherever the posting spacing drops under 0.5 m — at z14 the spacing is ~4.8 m, so this is a
  latent bound on stride and zoom rather than a live wrong picture. Right: `TerrainMesh` carries `Rows()`
  and `Cols()` beside `VertexCount()` (`F.21`), and `ChunkBuildEcef` takes them.
- **`eyeTravelM` counts a teleport as walking** (`clients/EyeTelemetry.cpp:14-25`). `Moved` has one input and cannot tell a step from a jump, so any re-stand adds the whole distance back to the declared standpoint to the path length: walk 500 m, press `R`, and the record says 1 000 m walked and 0 m displaced — which is the *same* row a 500 m circle writes, the one case the header claims the column exists to separate. Not reachable at all since `b83285f` deleted the only caller (`AppWasm`'s `R` key), and no run in `test/mods/demo` has two motion runs at different standpoints — but the column is the *record's*, not the client's, so it comes back with the interactive client rather than being fixed by its absence. Right: `Restood(Stance)` beside `Moved(Stance)` — the discontinuity is spelled at the call site that causes it, re-anchors nothing and adds nothing to travel.

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
  allocation at static init. Reproduction: the probe above, written as one translation unit — the file the original reading used lived under the system temp directory and is gone, which is why the recipe and not the path is what this line carries.
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
- **Two constants that describe soil are applied to water, glass and paint, and no material can decline
  them.** `render/stages/SurfaceLight.h:33 kGroundBounce = 0.12` and `:40 kSelfShelter = 0.35` are
  documented — correctly, and with sources — as the mean visible reflectance of Central European land
  cover and as the sky fraction a clod, a furrow or a sward hides from a point between them. Both are
  statements about **one kind of surface**; both are spliced into **every** lit surface, because
  `litRadiance` is the single lighting function and every draw stage calls it:
  `stages/TerrainDraw.cpp:120`, `stages/BuildingDraw.cpp:371`, `stages/ModelDraw.cpp:225` and `:240`,
  `stages/WaterDraw.cpp:36`, `stages/BenchGroundStage.cpp:89`, `Sward.h:308-309`. Arithmetic, for an
  up-facing facet under sun alone (`skyH = 0`, `ndu = 1`, `sunUp = sunVis = thruDir = 1`):
  `ambE = nearE·(1.0·0.35) = 0.35·ρ·E⊥` and `dirE = E⊥`, so the outgoing radiance is
  `k·(1 + 0.35ρ)·E⊥` against Lambert's `k·E⊥` — **+17.5 % at ρ = 0.5, +26.3 % at ρ = 0.75**, on a pane of
  glass and on open water exactly as on a ploughed field. Caveat sought and it does not hold: the values
  are not wrong and `CLAUDE.md`'s *one lighting model* is not the problem — the defect is that a
  **surface** property is an **engine** constant, so there is no configuration of this renderer in which
  a Lambertian surface is spellable, which is what the first external check of `doc/requirements.md`
  § I.26 rung 3 needs in order to have a referee. Distinct from the near-field bug above: fixing that
  expression leaves both constants exactly as unreachable. Right: two scalars in the material row —
  they switch no pipeline state, which is the material row's whole definition — ground declaring
  0.12/0.35 and a manufactured surface declaring 0. Decides it: one facet, albedo 0.5, one sun at normal
  incidence, no sky; the linear tap must read `ρ·E⊥/π` and today reads 1.175× that.
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

- **`core/Sha256.h`'s justification for 256 bits carries one right number and one wrong by
  seventeen orders of magnitude.** The header (`:11-14`) argues *"At 10^12 distinct keys the birthday
  bound is 10^24 / 2^257 ~ 1e-53 … a 64-bit truncation would be 1e-13 at the same load, which is
  not"*. The first is right to an order (n²/2·2²⁵⁶ = 4.3 × 10⁻⁵⁴). The second is **2.7 × 10⁴** —
  n²/2·2⁶⁴ at n = 10¹², i.e. a collision is *certain*, not one in 10¹³; the birthday threshold for
  64 bits is 2³² ≈ 4 × 10⁹, three orders below the stated load. The conclusion the number supports is
  unaffected and the derivation beneath it is false, which is the one thing `CLAUDE.md`'s *every
  number carries its origin* forbids. Right: the number, or the sentence goes. The six digests
  themselves are correct — all six vectors in `test/core/Sha256MatchesTheStandard.cpp` reproduce
  against an independent SHA-256 (checked 2026-08-12).
- **`TilePool` holds a worker thread for up to 30 s where it held one for 3 s, asleep in a 1 ms
  loop.** `world/TilePool.cpp` `kPollMs = 1`, `kPollAttempts = 30000`. The previous shape blocked the
  worker inside one synchronous transfer for 60 × 50 ms; the new one sleeps a millisecond at a time
  and re-takes `CurlTransport::Mutex_` on every wake, against the transport's own 8 threads
  (`test/host/CurlTransport.cpp:14 kDefaultThreads = 8`) — up to 14 threads on 2 performance and 4
  efficiency cores (`CP.40`, `CP.41`). **Not attributable and the reason is stated**: the only
  comparison available is `verify-still` at 110–119 s against 103 s for the proxy version *in front
  of a warm container*, which is not a baseline — different upstreams, different cache state. The
  number that would decide it does not exist yet and is one field away: `Ledger::FetchMs` is now the
  sum of `SourceSet::Collect` calls rather than wire time, so wall-per-request split into wire and
  poll, p50/p95/p99 over a cold traversal, is a *not yet measured* and not a limit. Right: the
  transport declares a wait, so a thread with nothing to do blocks (`doc/requirements.md` § I.22).

- **The one number every physical quantity in the renderer passes through is marked DERIVED and its
  derivation ends at a display code.** `render/stages/SceneScale.h:17 kSceneExposure = 11.0` is
  introduced as *"DERIVED from the measured frame, not tuned by eye"*, and the derivation's last step is
  *"placing that at ACES input 0.32 — **the value whose sRGB output is 0.70**"*. The first three steps
  are radiometry and the fourth is a mid-tone in an 8-bit picture, so the constant is a **fit to a
  display**, wearing the marker `CLAUDE.md` reserves for a number that follows from other numbers. It is
  applied by `litRadiance` to sky, ground, buildings, blades, water and haze alike (`SurfaceLight.h:91
  k = alb * (kSceneExposure * kInvPi)`), i.e. to every radiance this engine produces. Compounding it:
  **nothing in the tree can read the quantity it scales.** `render/Renderer.h:59 ReadPixels` returns
  *"tightly packed W*H*4 RGBA8, already sRGB-encoded"* and is the only colour tap — `ReadDepth` and
  `ReadIrradiance` are the other two readers and neither is scene radiance — so the claim
  `SceneScale.h` makes about itself is unfalsifiable from inside, and an exposure constant and a physics
  error are the same observation here. Caveat sought: a scene-referred pipeline with a display-anchored
  exposure is legitimate *if the anchor lives in the exposure stage*; this one lives ahead of every
  surface shader, which is the part that is wrong regardless of the value. Right: the marker is `[SET]`
  until a linear tap exists (`doc/requirements.md` § I.26), and the number then either moves into
  `ExposureStage` or is shown to be 1.0 with the difference in the irradiances. Decides it: the linear
  readback of one facet of declared albedo under one declared irradiance against `ρ·E·cos θ/π`.
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

- **A derived constant whose derivation no longer exists.** `world/ChunkSurface.h:58`
  `kSurfaceAgreementM = 9.17e-4f` is the ceiling on how far the two evaluators of the terrain surface
  may disagree, and it is the sum of seven float32 terms. The instrument that summed them and checked
  the sum against plumb runs was `tools/surface_budget.py`, deleted with `tools/` on 2026-08-12. The
  number is unchanged and may well be right; what is gone is any way to recompute it, so it is a
  measured value with no reproducible origin — against `CLAUDE.md`'s *every number carries its
  origin*. Right: a test under `test/world/` that reconstructs the seven terms and asserts the
  constant bounds them, which is the same arithmetic in the language the tree is written in.

- **The browser is gone from the code and still in the prose, 38 times.** `grep -rniE
  'browser|wasm|canvas|emscripten|console\.log|Chrome'` over `src/**.{h,cpp,wgsl}` returns **38 hits in
  27 files** with the last `#ifdef __EMSCRIPTEN__` deleted (2026-08-12) — `clients/Walker.h:1,22,35`,
  `clients/SceneRunner.h:7`, `clients/Png.h:10`, `clients/Artifacts.h:11`, `clients/Sim.h:42`,
  `core/io/Log.cpp:18`, `core/GroundSample.h:2`, `core/Camera.h:82` among them. A comment that
  explains a decision by a platform no target compiles for is a reason the reader cannot check, which
  is `NL.2` failing in the direction that costs most. One of them is not a comment: `clients/RunIdentity.h:22`
  carries an `Agent` field that is *"the browser's own version string and empty natively"* and is
  published as a telemetry column that is now always empty. Right: each site either states the reason
  that still holds or goes, and the `agent` column goes with the browser that filled it.

- **`Artifacts` is an interface over one implementation, and two of its states cannot occur.**
  `clients/Artifacts.h` exists because *"a directory natively, an HTTP endpoint in the browser"*; the
  browser endpoint is gone and `clients/FileArtifacts.h` is the only implementation, whose `Settle()`
  returns `Complete` on the line that asks. So `Delivery::InFlight` is unreachable, and the arm that
  handles it — `clients/SceneRunner.cpp:129-131` with `kDeliverWaitMs = 20000.0` at line 48, a
  20-second wait derived from *"the collector's own POST timeout"* — is dead code guarding against a
  collector that no longer exists. `C.121`/`I.25`: an abstract interface with one implementation and
  three unreachable branches. Right: the runner writes to a directory, `Delivery` is `Complete` or
  `Refused`, and the wait has no subject to wait for.

- **A studio picture is not independent of the geodetic anchor it is drawn at.** A studio stage
  declares no place (`doc/requirements.md` § I.25), so `scenario/Studio.h:22 kAnchorLatDeg/LonDeg`
  is the arbitrary point the camera basis is built at. **Measured 2026-08-12** over
  `demo subject-beech`, two binaries differing only in that constant — the shipped 0/0 against
  52.10602/9.43453 — **all 48 pictures differ**: mean |Δ| over the 48 is 12.5 codes of 255, worst
  frame `beech-leaf_hd-frontlit` mean 44.9 with 98.3 % of pixels moved, worst single pixel 247.
  Three components are visible in the difference image
  (`beech-portrait-frontlit`, sky band mean 19.4, floor band 16.3, card band 0.8): the ruled floor's
  grid phase, which is drawn in world metres and follows the anchor **by design**; a ~1 px shift of
  the horizon line; and the whole crown's leaf scatter, salt-and-pepper across every leaf. The last
  two are not explained. Nothing in the bench hands the renderer an absolute position except the
  camera's ECEF eye and basis — `clients/SubjectBench.cpp:216-240` — and the atmosphere rebases onto
  its own sphere from the local ground radius (`render/stages/AtmoCommon.h:43 atmoPos`), so a
  latitude-dependent crown is a read of world position by something that should not have one. Right:
  the anchor can be moved and only the world-metre ruler moves with it; until then no comparison of
  two bench runs may cross an anchor change.

- **The sward bench photographs nothing and reports success, 57 times.** `demo subject-meadow` writes
  57 PNGs into `bench-meadow` and exits 0, and **every one of them logs `fillPct=0`** — an empty
  ruled floor beside the neutral card, no grass anywhere in frame. The cause is one line: the herb
  subject's template index was stored and never read — `clients/SubjectBench.h:137 int Bucket_` at
  `757e335`, written at `SubjectBench.cpp:146` and read nowhere — so the renderer is never told which
  vegetation template to draw. It predates this round (the field is write-only at `757e335` too) and
  survived the move to a declared studio, where the index is now not even carried. It is the
  `verify-refusals` shape one layer up: *a bench with nothing to measure must refuse*, and this one
  measures 0.0 % cover 57 times without a word. Right: the sward is drawn, or the bench refuses a
  subject it cannot draw — `SubjectBench::Write` already computes the number that decides it.

- **The language standard has two values.** `Makefile:26` sets
  `CXXSTD := -std=c++17` and uses it for every compile gate (`verify-generators`, `verify-world`,
  `verify-types`, the `gen_gate` link, the `world` target's C++ half); every shipping compile line
  (`Makefile:165,184,219`) hard-codes `-std=c++20`. `CLAUDE.md` no longer names a dialect at all —
  it says *modern C++* — and `doc/requirements.md` § I.19 declares C++20 as the one value, unticked. The program is C++20 — forced by
  `vendor/dawn`, whose `webgpu_cpp.h` needs `std::type_identity` and `std::span` — while the gates
  judge a dialect the program is not built in, so a C++20 construct in `render/` or `clients/` is
  covered by no gate. Right: one variable, one value, and the reason (Dawn) beside it.

- **No gate reads the log levels of the run it just declared green.** `verify-walk-asan` now asserts
  the run's own motion verdict — `frames=10800 impostorStands=9565 treeTris=19130` — but a line at
  `ERROR` in the same 10 800-frame run still passes it, and the run emits exactly one:
  `render device_lost reason=2 msg="Device was destroyed."` (`Makefile` `verify-walk-asan`,
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

- **The declared negatives pass on any compile failure of the right shape.** `Makefile`
  `verify-generators` and `verify-world` accept a fixture as refused if the compiler exits non-zero
  **and** its output contains the substring `file not found` — so a fixture that misspells its own
  forbidden header (`#include "Rendererr.h"`) is a green gate that proves nothing, and so is one whose
  body stops compiling for an unrelated reason while the include still resolves. Four fixtures ride
  this: `RendererIsNotReachable`, `WorldIsNotReachable`, `LogIsNotReachable`, `DrawIsNotReachable`,
  plus `test/compile/world/GeneratorIsNotReachable`. The three `-Werror` negatives under
  `test/compile/core/` are
  weaker still — `verify-types` checks only the exit status and matches no diagnostic at all, so any
  error in `HeightIsNotReachableWithoutItsState.cpp`, `AnswerIsNotIgnorable.cpp` or
  `DepthIsNeverNegative.cpp` passes it. Right, and it costs nothing: demand the **exact** expected
  diagnostic text (`fatal error: 'Renderer.h' file not found`, `error: ignoring return value of
  function declared with 'nodiscard' attribute`) **and** that it is the *only* error the compiler
  emitted, which is what makes a typo elsewhere in the fixture fail the gate instead of satisfying it.

- **`test/generators/SameRegionSamePlacement.cpp` is 689 lines behind one `main`** (`F.3`), carrying on
  the order of thirty distinct claims — determinism of placement, the class lattice, water depth,
  way half-widths, `sizeof(Body)`, an allocation count — in one process. Three consequences: the
  first hard failure (it dereferences `std::optional` results directly, e.g. `ways.MadeAt(...)->WidthM`)
  hides every claim after it; the run reports `verify-generators: N failed` without a machine-readable
  name per claim; and no claim in it can be run alone. It also holds the tree's only `malloc` outside
  `world/terrain` (`:315`; `clients/SimHost.cpp`'s went with the file in `b83285f`). Right: one translation unit per claim under
  `test/generators/`, each with its own `main`, which is what the suite this file's requirements
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

- **Fifteen preprocessor conditionals in the library compile for a target no build produces.**
  `core/io/HeapProbe.cpp` (6), `clients/HttpPost.cpp` (4), `render/Renderer.cpp` (3) and
  `core/io/StackProbe.cpp` (2), with four `#include <emscripten…>`. Down from twenty: the fetch's own
  `EM_JS` primitive, `fb_take_http_body` and the loader's browser arm left with the HTTP hop
  (2026-08-12).
  No target compiles them since the wasm targets were deleted, so they are unbuilt code inside `src/`
  — the one thing `-Werror` cannot see. `HttpPost.cpp:17` additionally justifies a static's lifetime
  with `-sEXIT_RUNTIME=0`, a flag from a build file deleted in the same commit. Right:
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
  in `APP_SRCS`, so it cannot rot) and constructed nowhere. **The larger half is that it is not one
  dead class but a dead arm of a declared enumeration, and the arm is already declared in a shipped
  mod**: `test/mods/demo/mod.json` has scene `walk` with `"kind": "interactive"`, and
  `./build/gpu_walk demo walk` answers `ERROR run scene_is_interactive scene=walk`, **exit 1**. Worse,
  The half of this that was about an omitted `kind` is gone (2026-08-12): `kind` is a required string
  and a scene without one is refused by path (`scenario/Scene.cpp`), so an unvalidated `runs` block is
  no longer reachable. The dead arm itself stands. `Walker` is only the machinery that arm would have used. A gate that counted unlinked *files* would stay flat through both, which is
  why `doc/requirements.md` § I.21 now asks for unreached symbols instead. Right: the client with an
  input medium is its caller (§ I.14), or the enumerator and the class go together in the round that
  decides there will not be one.

- **`HttpPost.cpp`'s abandoned-status vector is dead, and the comment that justifies it cites a
  build file that no longer exists.** With the browser path gone, `Begin` is the curl arm
  (`clients/HttpPost.cpp:62-88`), which sets `*Status_` to a terminal value **before it returns** —
  an HTTP status or `kNoAnswer`. `~HttpPost` (`:107`) pushes into `gAbandoned` only when
  `*Status_ == kInFlight`, which is now unreachable, so the `std::vector<std::unique_ptr<int>>` at
  `:20` is a static that can never gain an element. Its six-line rationale still argues from
  `-sEXIT_RUNTIME=0` in a Makefile deleted in the same commit and from a promise resolving after
  destruction, neither of which any target can produce. This is what a dead `#ifdef` costs beyond the
  lines it occupies: the *live* code around it keeps a shape and a justification that stopped being
  true, and nothing compiles it wrong. Right: the vector, `kInFlight`, the two-state cell and the
  destructor go together, and `Status_` becomes an answer that exists or does not.

- **`make walk`'s own help text says it builds the interactive client.** `Makefile:193` —
  *"build the interactive client / frame oracle"* — and `test/clients/AppWalk.cpp:101` refuses a scene
  whose kind is interactive. One binary, one of the two roles. A reader's first contact with this tree
  is `make help`, and it states a capability the tree does not have. Right: the text names the frame
  oracle only, until the client that takes input exists (`doc/requirements.md` § I.14).

- **`src/data` does file I/O with `<cstdio>` and there is no `Host::Storage` to do it through.**
  `data/ContentStore.cpp` opens, writes, renames and sweeps with `<cstdio>` and `<filesystem>`, and
  `data/StarBands.cpp` reads its four band files the same way. `doc/requirements.md` § I.24 rules that
  the library tier is read through a declared host seam and never `fopen`; the transport half of that
  seam exists now (`data/Transport.h`, implemented in `test/host/`) and the storage half does not.
  Three further `fopen` sites in `clients/` and `world/` predate this and are the same line. Right:
  `Host::Storage` beside `Host::Transport`, declared by the library and implemented by the host, and
  the content store becomes a policy over it rather than a user of a libc call.

- **A source's refusal cannot name what it looked for.** `data/StarBands::Collect` answers
  `Meaning::Refused` when a band file will not read, and the path it tried is not in the answer — the
  provider layer may not name `Log` (that is the layering, and it is right), and `Fetched` carries a
  meaning and bytes and nothing else. The caller knows the directory and logs it
  (`clients/Outshine.cpp` `star_catalogue_unreachable`), so the run is not silent, but the *file* that
  failed is not in the record. § I.22's *a test that must not reach the network declares zero sources
  and gets a refusal by name* wants the name to travel with the refusal. Right: `Fetched` carries a
  short reason string on the refusing arms only, minted by the source and printed by the caller.

- **`clients/HttpPost.cpp` still carries `curl`, so the tree's transport is behind the host seam and
  the telemetry poster is not.** Measured 2026-08-12 with `nm -u` over `build/obj-walk/*.o`: zero
  `curl_` symbols in every `core-`, `data-`, `gen-`, `world-`, `sim-` and `render-` object, eight in
  `host-CurlTransport.o` where they belong, and **seven in `app-HttpPost.o`**. That is the fb-sim log
  and telemetry channel, which `doc/todo.md` already carries as its own item (*the library owns its
  log*); it is recorded here because "no transport library in the library" is now true of the data
  path and not yet of the tree.

- **A Python file survives in the tree, and it is the one that can recompute the star catalogue.**
  `assets/sky/stars/build_stars.py` (170 lines) fetches HYG v41, applies proper motion and IAU-1976
  precession to the run epoch and writes the four magnitude bands the `hyg.bands` source serves. It
  moved with its data out of the deleted `tiles/` rather than being deleted with it, because the bands
  are admissible measured data only for as long as *we* can recompute them and nothing else in the
  tree can. Against `CLAUDE.md`'s **modern C++, and only C++**. Right: it becomes a declared test that
  bakes the bands and checks them against the committed ones, in C++ — at which point advancing the
  epoch is a run of the test suite rather than a script somebody remembers.

- **Weather and peaks left the tree with the server, and nothing replaced them.** `tiles/src/wx.cpp`
  (531 lines), `grib2.cpp` (330), `peaks.cpp` (157) and their headers — a NOAA GFS GRIB2 decoder and an
  Overpass bounding-box query — were deleted with `tiles/` on 2026-08-12 because **no consumer in
  `src/` or `test/` ever reached them**: measured by grep for `/wx`, `/peaks`, `FBWX`, `wxb`, `grib`
  over the whole tree, zero hits outside `tiles/` itself. `core/WeatherProvider.h` is the seam they
  would have fed and its only implementation is `clients/SceneWeather.h`, which widens two declared
  JSON numbers. `doc/requirements.md` § I.22 carries *weather is a source under the same contract, with
  a validity epoch* and *peaks are a source whose scheme is a query* as open lines; they are now open
  with **no implementation anywhere**, where before they were open with an implementation nothing
  called. Right: they come back as sources when a consumer reaches for them, and the GRIB2 decoder is
  rewritten rather than recovered — its former home compiled under two compilers and failed one.

- **`verify-clients` cannot see a scene-building call it was not told about.** `BUILD_CALLS`
  (`test/clients/verify_clients.py:50-55`) is a closed alternation of twenty method names, and
  `ENTRY_INCLUDES`/`ENTRY_FORBIDDEN` are closed lists too. A new `Renderer::SetX` used from a second
  translation unit passes the gate in silence — the exact failure mode the gate exists for (one client
  drew a tree and the other did not for ten rounds), one level up. The gate is not wrong, it is the
  wrong *kind*: an allowlist reports what it enumerates, where `doc/requirements.md` § I.20 step 7's
  `src/api/` makes the whole class unspellable, because a translation unit that cannot name `Renderer`
  cannot call any method on it, named or not. **The include set cannot do it today**: `clients/Outshine.h:20`
  includes `Renderer.h`, so an entry point reaches the type through the one object it is supposed to
  construct and the Makefile has to hand it `-Isrc/render` (`INC_CLIENTS`) — which is why this file is
  the last Python in the tree. Right: the include set replaces the regex, and rule 2
  (`main()` under 40 lines, `F.3`) is the only clause that still needs a counter afterwards.

- **Eight `fb_*` free functions in `src/` were never covered by the exception that was just struck.**
  `fb_stream_open`, `fb_stream_close`, `fb_stream_ground`, `fb_stream_ground_block`,
  `fb_stream_ground_post_m`, `fb_tile_pool`, `fb_fetch_stars`, `fb_load_image_file`
  — across `world/TerrainLoader.{h,cpp}`, `world/World.cpp`,
  `world/OsmField.cpp`, `world/BuildingField.cpp`, `world/WaterField.cpp`,
  `clients/Sim.cpp`, `clients/Outshine.cpp`, `render/Renderer.cpp`.
  `fb_take_http_body` left with the browser transport (2026-08-12) and `world/TilePool.cpp` is off
  this list; `fb_post` left with `clients/HttpPost.cpp` and `fb_canvas_px` with the canvas surface
  target (both 2026-08-12).
  `CLAUDE.md`'s naming rule exempted `world/terrain/` and `FBWX` and nothing else, so these were already
  outside the exception; with `world/terrain/` gone there is no C-ABI code left in the tree at all and
  the prefix has no remaining justification anywhere. Zero `osmmesh_` names survive, which is the half
  of this that did land.
- **The declared still's identity is taken over the encoded file, so an encoder change reads as a
  picture change.** `Makefile:395` hashes `walk.png` with `shasum`, and the pin in `doc/todo.md:12` is
  that file's sha. Swapping `stbi_write_png_to_func` for `IMG_SavePNG_IO` in `src/clients/Png.cpp` moved
  it `852bd4246ee34f65` → `bec69fea0a4e6837` with **not one pixel changed** — decidable rather than
  argued: decoding each of the four seeds' new `walk.png` and re-encoding it with the deleted stb
  encoder, at the settings `Png.cpp` used at `859f702`, reproduces `852bd4246ee34f65` exactly, four
  times out of four. The gate itself is unaffected (`verify-still` compares runs to each other and pins
  nothing), but the value a human compares against is the wrong subject and will move again at § I.17's
  wasm gate and at phase 3.4. Right: the sha is over the decoded RGBA buffer, hashed before the encoder
  is called — it costs one call, survives every encoder and container change, and moves only when the
  picture does.

## Stale pointers held with confidence — eight sites naming two deleted documents

`doc/architecture.md` and `doc/vision.md` were folded into `CLAUDE.md` and deleted. **Eight comments in
`src/` still cite `doc/architecture.md` as the authority for a rule they state**, and one requirement
line cites it too. A reader who follows the pointer finds nothing; a reader who does not follow it takes
the rule on the comment's word, which is exactly the failure mode a citation exists to prevent. This is
the same defect class as a miscited rule number — a confident reference to something that is not there —
and it costs a round the first time somebody tries to check one of these rules against its source.

- `src/core/Material.h:19` — *"nothing in it can switch a pipeline state (doc/architecture.md)"*. The
  rule is live and correct; it is in `CLAUDE.md` under *the core dictates the pipeline*.
- `src/render/GeometryStage.h:3` · `src/render/Renderer.cpp:785` · `src/render/stages/TaaStage.cpp:110`
  · `src/clients/Sim.cpp:176` · `src/clients/Sim.h:50` · `src/clients/RegionForge.h:2` ·
  `src/generators/Water.h:2` — the same, one each.
- `doc/requirements.md:193` — *"declared in `architecture.md`, not found in `PresentStage`"*: the line's
  own evidence is a document that no longer exists, so the line cannot be checked as written.

Right: each site names `CLAUDE.md` and the sentence there, or states the rule without a citation if the
rule is local. A grep for `architecture.md` returning zero in `src/` is the check.

## German in an English-only repository, and it cites a numbering that is gone

`CLAUDE.md`'s first rule is that the repository speaks one language. Two comments are in German, and
both compound the error by citing a numbered principle list that the current `CLAUDE.md` does not have —
it carries *the constraints*, *stance* and *setup*, with no numbered principles at all.

- `src/scenario/Animation.h:15` — *"a bespoke format here would be the parser nobody ordered (Prinzip 1)"*.
- `src/core/Keyframes.h:22` — *"(Prinzip 7: a run must …)"*.
- `src/render/Renderer.cpp:633` — *"(CLAUDE.md, Prinzip 5)"* — half-translated, and the cited number
  does not exist in the file it names.

`src/render/stages/TerrainDraw.cpp:81` and `src/world/TerrainLoader.cpp:329` cite *"CLAUDE.md principle
2"* in English, which is the same dangling number in the right language. Right: the sentence the rule
actually is, quoted or paraphrased, with no number — a number into a list that is not numbered is worse
than no citation, because it reads as precise.

## Cycles' first Metal frame costs 200 s and would be attributed to the scene

Not a defect in the tree yet — a **trap laid for § I.26's oracle**, recorded here because the harness
that walks into it does not exist yet and the number is cheap to lose. Measured on this host, Blender
5.2.0 LTS (`fbe6228777e7`, built 2026-07-14), factory startup cube, 1280×720, 128 spp, adaptive
sampling off, denoising off, `diffuse_bounces = 0`, seed 0, OpenEXR float32:

| device | cold | warm |
|---|---|---|
| Metal, Apple A18 Pro, 5 GPU cores | **200.9 s** | **2.087 s** |
| CPU, Apple A18 Pro | — | 11.6 s (128 spp) · 49.6 s (512 spp) |

The 200.9 s is Cycles compiling its Metal kernels once per kernel-cache generation. A reference run that
times its first frame attributes it to the scene and reports a per-frame cost two orders out. Right: the
oracle harness renders one throwaway frame before it starts timing, and publishes *cold* and *warm*
separately as the instrument's own floor beside the result.

A second trap in the same measurement, and it is the one that produced the CPU column: setting
`scene.cycles.device = 'GPU'` is **not** sufficient — `preferences.addons['cycles'].preferences` must
have `compute_device_type` set *and* `get_devices()` called *and* the device's `use` flag set, or Cycles
falls back to CPU silently and the run is 5.6× slower with no message. A harness that does not assert
the device it got has measured something it did not choose.

## A conclusion about a host drawn from four paths — the vacuous-gate shape, in a research method

**Mine, this round, and it produced a wrong finding inside a design that was otherwise careful.** I
checked `download.blender.org` at `/peach/`, `/durian/`, `/mango/`, `/institute/` and `/demo/movies/`,
found three redirect stubs and one directory of rendered video, and wrote into `doc/requirements.md`
that the Blender open movies are **not fetchable** and that the film rung therefore had to be built from
something else. Walking `/demo/` properly refutes it:

| Path | What is actually there |
|---|---|
| `/demo/sprite_fright_030_0020_A.zip` | 254 374 945 B — a **complete Sprite Fright shot**, 47 `.blend`, three animation takes, character, spider, mushroom grove, village, plants, fungi |
| `/demo/bbb/blender.zip` | 830 709 844 B — **Big Buck Bunny's entire production tree**, 591 `.blend` |
| `/demo/cycles/` | four large Cycles demo scenes, one of them the best-known on the host |
| `/demo/eevee/*/README.txt` | **per-file licences the web page does not state** — the strongest evidence available, and the first pass never looked |
| `studio.blender.org/terms-and-conditions` | *"all digital content … is available under the Creative Commons Attribution 4.0"* — the licence was never the constraint; **access** is, and only for part of it |

**The shape, and why it belongs in this file rather than in a round's report.** It is the same defect as
a gate that certifies over an empty object set: *the check ran, the check was sound, the population it
ran over was not the population the conclusion names.* Five paths were examined and the sentence written
was about a host. The failure is invisible from inside the method — every individual observation was
correct and correctly reported — which is exactly why it needs a rule rather than more care.

**The rule this earns:** a negative existence claim — *X is not available*, *no such asset exists*, *the
tree contains none* — names the enumeration it is drawn from, and the enumeration is **exhaustive over
the container** or the claim is written as *not found at these paths*. A directory index is cheap to
walk to the bottom; the first pass did not walk it at all. `doc/requirements.md`'s own measurement rule
already ranks *correctness — checked against something outside* above *consistency*, and a negative
claim is the one kind that cannot be checked by more internal agreement.

**Right, and it is one line of method:** for a claim about a file host, recurse the index; for a claim
about a repository, list the tree at the pinned SHA — which this round *did* do for Khronos, where the
tree was enumerated whole and the licence findings from it stand. The two halves of the same round used
two methods and only one of them was sound.

**A second shape from the same round, and it is worth separating from the first.** The availability
error was a conclusion drawn over a population nobody enumerated. This one is narrower and cheaper to
catch: **a claim contradicted by a measurement the same round had already taken.** § I.26.11 justified
the oracle cache with *"200 Cycles renders at 720p is hours"* while § I.26.4, four sections earlier in
the same file, carried the measured **2.087 s/frame** that makes it **7.0 minutes**. Both numbers were
mine, written the same day.

The rule: **a magnitude word — hours, huge, negligible, orders — that stands next to a measurement this
tree already holds is arithmetic, and it gets done.** Where the arithmetic contradicts the word, the
word goes. Where no measurement exists the claim is labelled a **projection** and says what would settle
it; the corrected passage now labels every one of its seven numbers *measured*, *derived* or
*projection*, which is what `CLAUDE.md` asks of a number and what an unqualified "hours" evades.

The cache survived the correction with better reasons than it had — the 200.9 s cold-start cliff, the
film's frame count, and the fact that a cached oracle cannot change underneath a comparison. **An
inflated justification was hiding a correctness argument stronger than the performance one it displaced**,
which is the second cost of this defect class and the one nobody notices.

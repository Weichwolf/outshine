# Bugs

**What belongs here.** Something that exists and is wrong. `doc/requirements.md` says what must exist
and an unticked line there means *not built*; a line here means *built and broken*. If it has never
worked, it is a requirement. If it worked, or looks like it works, it is a bug.

**A line carries where it is and what decides it** — file and site, the measurement or the picture that
shows it, and what right would look like. A bug without a way to tell it is fixed is a rumour.

**A fixed bug is deleted, not struck through.** `git log` is the record.

**An entry naming a file that no longer exists is not a defect.** It is the same failure as a
requirement line naming a deleted document, and it has cost this project twice. A round that deletes a
file audits this document in the same round.

## The repair policy, and it is not a cadence

*Stated 2026-08-12 because it had been unstated and was being improvised. The argument for it is below;
if it is wrong, the alternative is a repair round on a fixed cadence and the owner decides.*

- **A defect is repaired by the round that needs it**, not by a round scheduled to repair defects. A
  repair round has no acceptance of its own: it cannot tell a fix from a change, because the thing that
  would have told it — the work that meets the defect — is what was deferred.
- **A fixed defect is deleted in the round that fixes it.** Later is never: this audit found **21** dead
  entries, and the file overstated the debt by 14 %.
- **Cheap defects are batched**, because eleven one-line repairs in one round cost one review and eleven
  rounds cost eleven. Band 2 below is that batch.
- **The exception, and it is the only one: a defect that blocks the next round is not deferred to it.**
  Band 1 is repaired before the work that would meet it starts, because meeting it inside that work
  makes the work's own acceptance unreadable.

## The three bands, and every surviving entry is in one

**Band 1 — blocks the Khronos work.** Repaired before the draw-list round, because the assets it must
load are what would hit them.

| entry | where |
|---|---|
| `core/ChunkVtx.h` carries one UV and no `COLOR_0`, while `gltf/Types.h:163` already carries `TEXCOORD_1` for `MultiUVTest` | *Constants, names and units* |
| The glTF reader resolves a URI with no scheme, authority or traversal check | *Declaration and build* |
| `Artifacts` is an interface with **zero** implementations since `FileArtifacts` was deleted | *Declaration and build* |
| The winding is hard-coded at seven sites | *Declaration and build* |
| Three node-transform cases measure an ambient-occlusion estimator at one sample | its own section |
| Frame alpha derived from depth, so a translucent body over nothing is absent from our picture | its own section |
| The preparer and the runner hold two closed sets over one manifest schema, disagreeing on 8 of 26 | its own section |

*Checked and **not** in this band, against the coordinator's reading:* **`SurfaceState` carries
`SurfaceKind::Blended` and `Blends()` at `core/SurfaceState.h:8,23`, and `CoverageCut_` is per-material
at `:44` (`s.CoverageCut_ = material.CoverageCut`) with `AlphaBlendModeTest` named in the comment above
it.** Nothing there blocks that asset. **`extensionsRequired` is read and refused** at
`gltf/Document.cpp:156,295-299`, `kHonouredExtensions = {nullptr}`, so anything named is a refusal —
that entry is deleted below.

**Band 2 — cheap and just undone.** No excuse, no dependency; one round, batched.

| entry | measure |
|---|---|
| Stale pointers naming two deleted documents | **7** sites, not nine |
| German in an English-only repository | 5 sites, all live |
| Two headers guarded by reserved identifiers (`_EPHEMERIS_H`, `_FBSTATE_H`) | [lex.name]/3, undefined behaviour |
| `GpuTimer` takes no slot names and `TakeGpuTimes` has no caller | 2 sites |
| The browser is gone from the code and still in the prose | **30** hits, not 38 |
| `core/Mat4.h` is dead and its defending comment names a test that never existed | 2 files |
| `FacadeUv.h` has **0** `static_assert`s against 11 enumerators and a stride of 16 | 1 file |
| The language standard has two values — `-std=c++17` at `Makefile:24` and `test/run.sh:44`, `-std=c++20` on every shipping line | 4 sites |
| The unit-height check accepts 168 ulps where it measures 1 | `test/unit/generators/draw/GrownBarkIsAClosedMesh.cpp:225` |
| The harness's build cache is keyed by path and not by root | `test/run.sh:41-42` |
| Five camera manifests aim 0.4357 px off their stated derivation, origin unknown | `test/render/coverage/*/manifest.json` |
| Six environment variables change the picture and ride no column | `FB_TAU` · `FB_TAA` · `FB_GEOM` · `FB_TILEWORKERS` · `FB_GROUND_CLASS_VIZ` · `FB_DAGLOG` — `FB_MOON_SCALE` and `FB_TONE_PROBE` are gone |

**Band 3 — waits for the round that needs it, and the round is named.** *"Later" is a named event here
or it is a hope.*

| entry | waits for |
|---|---|
| `Node`'s *matrix XOR TRS* invariant enforced 250 lines from its type | **the round that adds a second node consumer** — one consumer cannot show the leak |
| `Document::ReadJson` is 228 lines | **the round that adds the next extension**, which is when the length becomes a cost rather than a shape |
| `Renderer`'s sixteen unconditional stage objects | **the SDL_GPU port**, which rewrites every one of them |
| `View()` creates a texture view per attachment per frame | **the SDL_GPU port**, same reason |
| Everything under *World and streaming* | **the round that restores a world consumer** — the walk client is deleted and nothing drives that path today |
| Everything under *Buildings*, *Vegetation*, *Light and shadow*, *Picture* | **the round that renders a scene against KCD** — all are picture judgements with no picture to judge |
| The data ledgers have no reader | **the round that restores a telemetry consumer** |
| The trailer is authenticated by shape, and a hard error stops the run | **the round that adds a test the harness cannot already judge** |

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
  `test/unit/generators/SameRegionSamePlacement.cpp:410` already constructs a `Structure` with
  `FeatureLevel::None()` — so the rule is one ingest site away from being false. If it ever is: a
  top-less feature enters the `highest`/`deepest` comparison at 0.0 m ASL, beating every declared top
  below sea level and losing to every one above, and at `Water.cpp:53` becoming a water level whose
  `WaterDepth::Between(0.0, ground)` reports `LevelBelowGround` for every dry-land outline — a missing
  datum counted as a disagreement between two models.
  *The fourth site, `test/clients/WorldMain.cpp:64-65`, went with the client on 2026-08-12; the three
  under `generators/` are what remains and they are the ones the factory closes.*
  Right, and **not** by swapping `Try(T *out)` for `std::optional` — this file's own opening argument
  rules that out and it still stands: `*opt` reads the payload with nobody having looked at the state.
  The answer is one rung up, at `C.41`/`C.42`: a `FeatureField::Feature` whose `Kind` is `Structure`
  or `Water` **cannot be constructed without a top**. It is an aggregate today with
  `FeatureLevel Top = None()`, minted by hand at three places, and the rule that keeps it right lives
  in a fourth directory. A named factory that takes the top as an argument deletes
  `Buildings.cpp:54`, `Water.cpp:20` and `Water.cpp:53` outright — no branch, no cast, no zero —
  and costs nothing at runtime. `GroundPatch` already has exactly this shape and that is why it is
  the one of the six with nothing to fix.

## The test harness and its instruments

*Six entries deleted 2026-08-12 as fixed or as naming deleted sites: `verify-data` (the target, `src/clients/HttpPost.cpp` and every `curl_` symbol are gone — `grep -rl curl src/` is empty); `verify-walk-asan` cannot see a stack lifetime error (`test/run.sh:573` sets `detect_stack_use_after_return=1`); an interrupted instrument leaves something bound (`test/run.sh` carries the group kill and the eleven `verify-*` recipes are gone — the Makefile has three targets); two tests hold each other's claim (`OutsideIsNeverAsked.cpp` is now `test/unit/data/UncoveredIsUndeclared.cpp` and the false `CountingTransport` comment is gone); a directory declared as the Makefile's is trusted (the Makefile owns no test source now, and a compile subject is driven by `-DOUTSHINE_COMPILE` at `test/run.sh:517` rather than being unrun).*

- **The registry's and the store's counters have no reader.** `Data::SourceSet::Ledger`
  (`data/SourceSet.h:76`) publishes nine — `Asked`, `Delivered`, `HandedOver`, `Vacant`, `Undeclared`,
  `Refused`, `Retried`, `FromStore`, `DeliveredBytes` — and `Data::ContentStore::Ledger`
  (`data/ContentStore.h:52`) six. **Verified at `9f4ba9e`**: `Counters()` is called from
  `test/unit/data/AbsenceHandsOver.cpp:136`, `TheStoreNamesBytesByTheirKey.cpp:78,113` and **nowhere
  else in the tree** — no telemetry row, no close-out line. The store's hit rate is the one number
  that decides whether the store is doing anything, and nothing prints it. `Per.6`, and § I.23's
  zero-consumer rule applies to a counter as much as to a constant. **Band 3** — waits for the round
  that restores a telemetry consumer, because there is no row to ride today. Right: both ledgers ride
  the ordinary row and the close-out line.

- **The trailer is authenticated by shape alone, so a file that never includes `Check.h` can print a
  green verdict.** `test/run.sh:343` accepts one line of eight fields with `CHECKS`, `FAILURES`,
  `SKIPPED` in the right places; `:435` cross-checks it against the process exit status, which a
  forger satisfies by returning 0. *The demonstration is gone with `test/harness/ForgedTrailer.cpp`;
  the shape is not, and it is re-demonstrable in one file.* Right, two lines and no change to
  `Check.h`: every increment of `Failures` prints exactly one line beginning `FAIL ` and every `Skips`
  one beginning `SKIP `, so `grep -c '^FAIL '` **must** equal `FAILURES`. That is a second witness on
  an independent path — per-failure `printf` against a counter — and it also catches a counter zeroed
  by any spelling `Tally` does not forbid. `CHECKS` has no printed witness and stays single-sourced.
  **Band 3** — waits for a test the harness cannot already judge.

- **A hard error stops the run, so one malformed test hides the verdict of every test after it.**
  `test/run.sh:435` `Die`s mid-loop when the trailer and the exit status disagree, and fifteen `Die`
  sites remain. The rule the deleted Makefile stated in its own words — *"every gate runs even after
  one has fallen, because the second failure is information the first one would have hidden"* — is
  now stated nowhere, and the harness is the instrument it matters most in. Right: a missing, doubled,
  malformed or disagreeing trailer is a per-test verdict of its own that is red and counted, the loop
  continues, and the run exits non-zero. Only the pre-flight directory scan may refuse before anything
  is built, which is correct there because nothing has run yet. **Band 3**, with the entry above.

- **The harness's build cache is keyed by path relative to the root, so two checkouts of this tree
  share objects, logs and binaries.** `test/run.sh:41-42` — `BUILD=${TMPDIR:-/tmp}` then
  `BUILD=${BUILD%/}/outshine-tests`, with **no component identifying the root**, verified at
  `9f4ba9e`. `UpToDate` compares mtimes of prerequisites resolved against the *current* root, so a
  second checkout whose sources are older than the first's objects links the **first checkout's**
  binaries, and every number read from them belongs to the other tree. A git worktree and a
  `git bisect` clone are ordinary, and the effect is silent. Right, one line: fold the root's real
  path into the build directory, e.g.
  `BUILD=${TMPDIR}/outshine-tests/$(printf %s "$ROOT" | cksum | cut -d' ' -f1)`. **Band 2.**

- **A real test placed in a non-harness directory is run by nothing.** `test/run.sh:207-214`
  `NotTheHarnesses` names `.`, `host` and `unit/compile*`; a `.cpp` there that includes the reporter
  and checks claims is named by no line of the output. Bounded — `:390` is a hard error for a
  directory in neither list, and two of the three are structurally not tests — so this is the narrow
  residue of a larger entry deleted above, not that entry. **Band 2**, and it is about six lines:
  refuse a source under those directories that includes `Check.h`.

- **The unit-height check accepts 168× the worst deviation it measures, and it bypasses the reporter's
  own rule about tolerances.** `test/unit/generators/draw/GrownBarkIsAClosedMesh.cpp:225` judges with
  a raw `std::fabs(v.DeclaredExtent - 1.0) > 1e-5` rather than `CHECK_NEAR`, so the number that
  decides an acceptance carries no origin and no frame of reference — the thing `Check.h:52-54` was
  written to forbid. Measured over all 31 declarations: the worst deviation is **5.96046448e-08 in
  `dog_rose`**, which is `2^-24` exactly, the float spacing immediately below 1.0 — one ulp, and the
  other 30 land on 1.0 bit-for-bit. `1e-5` is 168 ulps, so a normalisation that drifted to 0.99999
  passes. Right: `CHECK_NEAR(extent, 1.0, 2.4e-7 /* 4 ulp at 1.0 */, …)`, with the ulp derivation
  beside it. Note also what the lying branch proves: `DeclaredExtent` re-decides `GrowthForm::Lying`
  the same way `TreeGrower::NormalizeToUnitHeight` does, so for a lying form the check is
  *consistency* between two copies of one predicate and not the decidable class — only the standing
  case is decidable. **Band 2.**

## Bounds, allocation, and what the platform hides

*Measured 2026-08-11 in `/private/tmp/claude-501/-Users-cosmo-Git-flightbox/b5db31bd-4b15-4bfc-83c1-21cc63c39b74/scratchpad`,
emsdk 6.0.3 / node 26.7.0 / clang, all at `-O2`: an index 400 kB past a live `std::vector` writes real
bytes and exits 0 **in the browser and on the native oracle alike** — the address is inside a mapped
heap in both cases. The premise "it segfaults natively" holds only for a write that leaves the mapping,
which a heap overrun almost never does. So the oracle is not louder than the browser for this class,
and the conclusion is stronger rather than weaker: there is no safety net on either target today.*

- **The one bounds check the whole tree leans on can be defeated by arithmetic.** `core/Span.h:33`,
  `Span::Sub`, asserts `first + count <= Size_` in `size_t`. The sum wraps, so `Sub(4, SIZE_MAX - 2)`
  passes the assert and returns a span of `SIZE_MAX - 2` elements over `Data_ + 4`; every subscript of
  that span then also passes `assert(i < Size_)`. Reachable through `generators/draw/DrawSet.cpp:21`,
  `placed.Sub(range.First, range.Count)`, where both arguments are computed. Right:
  `assert(first <= Size_ && count <= Size_ - first)` — one line, no runtime cost, and it is the
  difference between a checked type and a type that looks checked. `ES.103` (no overflow) and
  `Bounds.4`-style reasoning: the check that guards the range must not itself be unguarded.
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
  the figures above are bark only and are what `test/unit/generators/draw/GrownBarkIsAClosedMesh.cpp`
  prints. *The leaf-point figures and the six "above the plane by 3.6e-5…1.2e-4" figures this entry
  carried before came from a probe that is not in the tree — that test measures neither — and are
  withdrawn until something measures them.* Nothing downstream lifts the mesh:
  `TreePrototype.cpp:111` copies `BoxMin.Y` into `Crown_.Bottom`, which only bounds the in-crown query
  (the deleted stand field) and the impostor box (`render/stages/ModelDraw.cpp:749`), and the
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
  `Data::Transport &Wire_`, and `SourceSet::Collect` reads `Store_` on the way in. *The two entry points that wrote the same inversion went with the clients on 2026-08-12; the
  member-order defect inside `Sim` is untouched by that and is what this entry is.* `C.13` names exactly this
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
  (`test/unit/data/TheStoreNamesBytesByTheirKey.cpp:94-115`) only ever reopens a full directory, so it
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
- **The fabrication window was reported 21 % too wide, and the upper bound is wrong.** `doc/todo.md`
  and the round's report give `85.05113 < lat ≤ 85.0534 — about 250 m`. Measured against the tree's
  own code (`Schedule(Ring{14,1}).Widest(lat, 15)` and `osmmesh_geo_to_tile`, bisected to 1e-12 deg):
  the window is **85.051128779807 < lat ≤ 85.053023927135**, width 0.0018951 deg = **211.7 m**
  (WGS84 meridional 111 694 m/deg at 85 N; 211.0 m spherical). The upper end is where
  `Region::Of(14, lat, ·).Y()` drops from −1 to −2 and the ring's whole `y+1` row leaves the grid —
  at the reported 85.0534 the Y is already −2 and the run refuses. Right: the bound is a property of
  `Schedule::Widest` at `RadiusRegions = 1` and moves with the radius, so the number is derived from
  the ring rather than quoted.

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
  `test/unit/data/TheAnswerNamesItsAddress.cpp`), and the whole path is carried through the byte cache
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
  the deleted stitcher (`if (x + 1 < n)`) correctly stopped a request the tile server
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
  `poolSumKB` by hand at the telemetry site (`pools.Sum() + generator`), not by `Pools::Sum` — the file
  that did it went with the browser-era clients, and the shape returns with its replacement.
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

- **A telemetry column with one reachable value, behind a branch no compiler can take.**
  `render/Renderer.cpp:111-115` writes `bool rg11 = false;` and then `if (rg11) feats.push_back(...)`,
  and line 177 logs `"hdr"` as `HdrFormat == RG11B10Ufloat ? "rg11b10ufloat" : "rgba16float"`. The
  format is set unconditionally to `RGBA16Float` at line 112 and never assigned again, so the feature
  request is dead and the log column is a constant string dressed as a measurement. The comment above
  it gives the real reason — rg11b10 has no alpha and the HDR target's alpha carries the occlusion
  fraction — which is a decision, not a run-time condition. Right: the format is a stated property of
  the plan (`doc/requirements.md` § I.27) with the alpha requirement as its reason, and a row that
  reports a format reports one the plan could actually have chosen.
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
  north term is wrong because the meridian scale is 111 229 m/deg there against 111 320. *Both consumers named when this was filed — `clients/StandField.cpp:32` and
  `clients/SceneRunner.cpp:287,318` — went with the clients on 2026-08-12. The header and its wrong
  bound are unchanged, and the next consumer inherits both* — so a declared metre of channel is 1.002086 m of
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
  themselves are correct — all six vectors in `test/unit/core/Sha256MatchesTheStandard.cpp` reproduce
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

- **The suffix `Ms` names two different units in the same tree.** `core/Units.h:22`
  `kMsToKt` is metres per second to knots. *Two of the three sites named here, `clients/Walker.h` and
  `clients/FrameTelemetry.h`, went with the browser-era clients on 2026-08-12; the rule they
  illustrated is unchanged.* Reading any one of them
  correctly requires reading its comment, against `CLAUDE.md`'s *a name that needs a comment is the
  wrong name*. `core/Units.h:15` `kMPerDeg` shows the unambiguous spelling already exists in the same
  file. Right: one declared suffix table, `MPerS` for velocity and `Ms` for milliseconds, applied
  everywhere; the ambiguity is decidable by grep and there are three sites.

## Declaration and build

- **A derived constant whose derivation no longer exists.** `world/ChunkSurface.h:58`
  `kSurfaceAgreementM = 9.17e-4f` is the ceiling on how far the two evaluators of the terrain surface
  may disagree, and it is the sum of seven float32 terms. The instrument that summed them and checked
  the sum against plumb runs was `tools/surface_budget.py`, deleted with `tools/` on 2026-08-12. The
  number is unchanged and may well be right; what is gone is any way to recompute it, so it is a
  measured value with no reproducible origin — against `CLAUDE.md`'s *every number carries its
  origin*. Right: a test under `test/world/` that reconstructs the seven terms and asserts the
  constant bounds them, which is the same arithmetic in the language the tree is written in.

- **The browser is gone from the code and still in the prose, 30 times — Band 2.** The same grep over
  `src/**.{h,cpp,wgsl}` returns **30 hits** at `9f4ba9e` (down from 38 as the browser-era clients were
  deleted) — `clients/Artifacts.h:11`, `clients/Sim.h:42`, `core/io/Log.cpp:18`,
  `core/GroundSample.h:2`, `core/Camera.h:82` among them. A comment that
  explains a decision by a platform no target compiles for is a reason the reader cannot check, which
  is `NL.2` failing in the direction that costs most. One of them is not a comment: `clients/RunIdentity.h:22`
  carries an `Agent` field that is *"the browser's own version string and empty natively"* and is
  published as a telemetry column that is now always empty. Right: each site either states the reason
  that still holds or goes, and the `agent` column goes with the browser that filled it.

- **`Artifacts` is an interface over one implementation, and two of its states cannot occur.**
  `clients/Artifacts.h` exists because *"a directory natively, an HTTP endpoint in the browser"*. **At
  `9f4ba9e` it has zero implementations** — `git grep -l ': Artifacts'` over `src/` and `test/` is
  empty; `FileArtifacts` and `SceneRunner` were deleted with the clients, and nothing replaced them.
  It still declares `enum class Delivery { InFlight, Complete, Refused }` (`:19`), two of whose three
  states no code can now produce. `C.121`/`I.25`: an abstract interface over nothing is not an
  abstraction, it is a shape waiting to be re-derived wrongly. **Band 1** — the Khronos runner is the
  next thing that writes artefacts, and it will either implement this or replace it, so the decision
  is due before it, not during it. Right: the runner writes to a directory, `Delivery` is `Complete` or
  `Refused`, and the wait has no subject to wait for.



- **The language standard has two values — Band 2.** `Makefile:24` and `test/run.sh:44` both set
  `-std=c++17`; `Makefile:87` and `test/run.sh:129` hard-code `-std=c++20` for anything touching
  Dawn. `CLAUDE.md` no longer names a dialect at all —
  it says *modern C++* — and `doc/requirements.md` § I.19 declares C++20 as the one value, unticked. The program is C++20 — forced by
  `vendor/dawn`, whose `webgpu_cpp.h` needs `std::type_identity` and `std::span` — while the C++17 arm
  judges a dialect the program is not built in, so a C++20 construct outside the render layer is
  compiled by nothing that would catch it. Right: one variable, one value, and the reason (Dawn) beside it.

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

- **`core/Mat4.h` is entirely dead, and the comment defending it names a test that does not exist.** `Mat4Identity`, `Mat4Mul`, `Mat4Perspective`, `Mat4LookAt`, `Vec3Normalize` and `Vec3Cross` have no caller outside `core/Camera.h`; inside `Camera.h`, `CameraBasisFrom`, `CameraAxes`, `HorizonDipRad`, `MvpTranslate`, `Frustum`, `FrustumFrom` and `AabbVisible` have none either. `CameraBasisEcef` is the only live function in the pair — **re-verified at `9f4ba9e`: one caller, `clients/Sim.cpp:555`**, the bench that was the second having been deleted. `Mat4Perspective` is reached only from `core/Camera.h:71`, itself dead. `Camera.h:76` asserts "CameraBasisFrom above is NOT dead: sky dome and star field are an infinity pass in LOCAL render-ENU"; `SkyStage` and `StarsStage` call nothing in the file. Two comments say "Pinned in `test_camera.c`"; no such file exists anywhere in the tree. Three consequences, worst first: the dead `Mat4Perspective` builds a **GL-style [-1,1] reversed-Z** projection, so anyone reviving it under WebGPU's [0,1] clip volume silently loses everything past the mid-range; `outshine::Frustum` (`Camera.h:132`) and `outshine::Render::Frustum` (`render/Frustum.h`) are two spellings of one statement against "every statement has exactly one place"; and a false comment is worse than no comment. Right: delete `core/Mat4.h` and everything in `core/Camera.h` but `CameraBasisEcef`.
- **Two headers guard themselves with reserved identifiers.** `core/Ephemeris.h:6` `#ifndef _EPHEMERIS_H` and `core/State.h:3` `#ifndef _FBSTATE_H`. A leading underscore followed by a capital is reserved to the implementation **in every scope** ([lex.name]/3) — undefined behaviour, not a style preference, and the rest of the tree already spells it `GEODESY_H`.
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
  (`world/OsmVector.cpp:18-19` assembles varints with `<<` and `0x7f`, `data/TerrariumDem.cpp`
  assembles Terrarium height from bytes), so this is a single site, not a
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

- **`test/unit/generators/SameRegionSamePlacement.cpp` is 689 lines behind one `main`** (`F.3`), carrying on
  the order of thirty distinct claims — determinism of placement, the class lattice, water depth,
  way half-widths, `sizeof(Body)`, an allocation count — in one process. Three consequences: the
  first hard failure (it dereferences `std::optional` results directly, e.g. `ways.MadeAt(...)->WidthM`)
  hides every claim after it; the run reports `verify-generators: N failed` without a machine-readable
  name per claim; and no claim in it can be run alone. It also holds the tree's only `malloc` outside
  `world/terrain` (`:315`; `clients/SimHost.cpp`'s went with the file in `b83285f`). Right: one translation unit per claim under
  `test/generators/`, each with its own `main`, which is what the suite this file's requirements
  now describe is for.


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
  meaning and bytes and nothing else. The caller that knew the directory and logged it went with the clients on 2026-08-12, so today the
  file that failed is in no record at all. § I.22's *a test that must not reach the network declares zero sources
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




## Stale pointers held with confidence — sites naming a deleted document — **Band 2**

`doc/architecture.md` and `doc/vision.md` were folded into `CLAUDE.md` and deleted. Comments in `src/`
still cite `doc/architecture.md` as the authority for a rule they state. **The count is not restated
here because it has been wrong at three different values across three rounds** — nine, then seven, then
fewer again after the SDL_GPU port took `GeometryStage.h` and `TaaStage.cpp` with it. `grep -rl
architecture.md src/` is the count, it is one command, and a number copied into this file ages the
moment a file is deleted. A reader who follows the pointer finds nothing; a reader who does not follow it takes
the rule on the comment's word, which is exactly the failure mode a citation exists to prevent. This is
the same defect class as a miscited rule number — a confident reference to something that is not there —
and it costs a round the first time somebody tries to check one of these rules against its source.

- `src/core/Material.h:19` — *"nothing in it can switch a pipeline state (doc/architecture.md)"*. The
  rule is live and correct; it is in `CLAUDE.md` under *the core dictates the pipeline*.
- `doc/requirements.md:193` — *"declared in `architecture.md`, not found in `PresentStage`"*: the line's
  own evidence is a document that no longer exists, so the line cannot be checked as written.

Right: each site names `CLAUDE.md` and the sentence there, or states the rule without a citation if the
rule is local. A grep for `architecture.md` returning zero in `src/` is the check.

## German in an English-only repository, and it cites a numbering that is gone — **Band 2**

`CLAUDE.md`'s first rule is that the repository speaks one language. Two comments are in German, and
both compound the error by citing a numbered principle list that the current `CLAUDE.md` does not have —
it carries *the constraints*, *stance* and *setup*, with no numbered principles at all.

- `src/scenario/Animation.h:15` — *"a bespoke format here would be the parser nobody ordered (Prinzip 1)"*.
- `src/core/Keyframes.h:22` — *"(Prinzip 7: a run must …)"*.
- `src/render/Renderer.cpp:770` — *"(CLAUDE.md, Prinzip 5)"* — half-translated, and the cited number
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

## The fetch allow-list refuses two assets the requirements already name

`test/corpus/prep/fetch.py:19-25` lists four subdirectories of `download.blender.org/demo/` —
`cycles/`, `eevee/`, `bbb/`, `asset-bundles/`. Two assets `doc/requirements.md` § I.26 already requires
are outside all four and are therefore unfetchable today:

| Asset | URL | What it is |
|---|---|---|
| Barcelona Pavilion | `demo/test/pabellon_barcelona_v1.scene_.zip` | **rungs 19 and 21** — scene scale, and the film summit |
| Sprite Fright shot | `demo/sprite_fright_030_0020_A.zip` | **the forest rung's motion arm** (§ I.26.7) |

`demo/test/classroom.zip` is in the same position. This is the enumeration-versus-invariant defect
again: the list names *what happened to be needed the day it was written*, and it was already stale
against the same document when it was committed.

**The wrong fix is one more line.** The allow-list's job is *is this host a source at all* — an index
that can be walked, a licence convention that can be read per file, no account — and that is true of
`download.blender.org/demo/` entire. Nothing under it is refused **by path**: `lone-monk`,
`mr_elephant`, `tree_creature` and `loft.blend` are refused by the named-subject table in `licence.py`,
which is the mechanism that reads licences and the only one that can. Widening therefore weakens
nothing, because the bytes are pinned by per-file SHA-256 either way.

**Right:** `https://download.blender.org/demo/` as one prefix. Fixed when the pavilion and the Sprite
Fright archive fetch under an unmodified `fetch.py`, and when `licence.py` still refuses `mr_elephant`.

## `Node`'s *matrix XOR TRS* invariant is enforced by the reader, 250 lines from the type it protects

`src/gltf/Types.h:107-119`. `Node` is a `struct` carrying `bool HasMatrix`, `double Matrix[16]`,
`double Translation[3]`, `double Rotation[4]`, `double Scale[3]`, with the invariant written in a comment
— *"A node carries a matrix or a TRS triple, never both"* — and enforced by an `if` in another file
(`src/gltf/Document.cpp:369-386`).

**The reason stated at the site is that the reader refuses the file that carries both. It is true and it
is not the question.** `C.2` — use `class` if the class has an invariant, `struct` if the members can
vary independently — and `C.40`, define a constructor if a class has an invariant. Here the members
demonstrably cannot vary independently: `Matrix` is meaningless when `HasMatrix` is false and the TRS
triple is meaningless when it is true. **A rule a reader enforces can be broken by the next writer of a
`Node`; a rule a `std::variant<Trs, Matrix4>` carries does not compile.** The "both set" state loses its
spelling, the "neither" state is `Trs{}` and is identity, `HasMatrix` disappears, and the branch that
reads it (`Document.cpp:564`) becomes `node.Local()` — which also deletes the possibility of a *second*
consumer reading `Matrix` without checking the flag and receiving the default identity, i.e. a mesh at
the origin.

**Worth a round now rather than after the format widens, and the argument is consumer count.** Today
`Node` has exactly one branch on `HasMatrix` and one test assertion, both inside `src/gltf/`. The bridge
from the reader to `core/ChunkVtx.h` is the next round (`doc/requirements.md` § I.26) and is a second
consumer; materials, skins and animations bring more. The edit costs a type, a `Transform Local() const`,
and one line of `test/unit/gltf/AMatrixNodeAndItsTrsAgree`, and it never costs less than it does now. It
also shrinks the record from 26 doubles (208 B) to a variant of 136 B (`Per.16`, `Per.18`).

**The caveat, sought and answered.** Is the invariant really exclusive? A node carrying neither is legal
and means identity — that is the `Trs{}` alternative with its own defaults, so the variant is exhaustive
and nothing is lost; and glTF forbids animating a `matrix` node, so no later arm needs to decompose one
back into a TRS triple.

**Two smaller defects in the same declaration, fixed by the same edit:** `double Matrix[16]` and
`double Translation[3]` are C arrays where `std::array` is the rule (`SL.con.1`), and they decay to
`const double *` at the `Transform::FromColumnMajor(step.Matrix)` call (`Bounds.3`).

**Fixed when** a `Node` with both a matrix and a translation does not compile.

## `Document::ReadJson` is 228 lines, and the reason stated for it is refuted 30 lines above it

`src/gltf/Document.cpp:208-435`. Measured: **228 lines · 27 `if` · 18 `for` · 7 ternaries → ≥ 53 logical
paths · 23 `return Refuse` sites · eight top-level arms** (buffers, bufferViews, accessors, meshes,
cameras, nodes, the parent pass, scenes). `F.3`'s own enforcement note asks for a function that fits a
screen — *"try 60 lines"* — and *"more than 10 logical paths through"*: this is **3.8× the line budget
and 5.3× the path budget**. `F.2` as well, since eight arms is eight logical operations.

**The stated reason — one linear pass beats six functions sharing a refusal channel — is refuted by the
file itself.** `ResolveBuffers` (`:176-206`) *is* one of those six: a private member returning `bool`,
calling `Refuse`, invoked from `ReadJson:222`. It shares the channel through `Error_` with no ceremony,
it reads better than the arm it replaced, and it is the counter-example to its own argument sitting 30
lines above it.

**Ranked below the two entries above, because it admits no wrong value** — every arm's refusal is
correct today. It is a structure defect, and it is the one that decides how the next 400 lines land.

**Right:** six more members of `ResolveBuffers`' shape. **Timing:** not a round of its own — the
**opening edit of the round that widens the format**, because materials, textures, images, samplers,
skins, animations and extensions are seven more arms and roughly 400 more lines, so the split costs the
same before or after and is worth strictly more before. **What splitting does not buy, said plainly:**
the arms have a real order dependency — views need buffers, accessors need views, meshes need accessors,
nodes need meshes and cameras, scenes need nodes — which is today implicit in statement order and would
still be implicit in call order. The shape that would carry it is each arm taking what it depends on as
a parameter instead of reading the member; that is the version worth writing.

## A decode failure has no sentence, and the header says it does

`src/gltf/Document.h:6` — *"`Error()` is the sentence"*. `Error_` is written only by `Refuse`, which only
`Read` and its helpers reach, so `ReadElements`, `ReadIndices`, `WorldTransform` and `ViewTransform` all
return a bare `false` and leave `Error()` **empty** — measured on a successful read followed by a failed
`ReadElements` (`AFileThatCannotMeanAnythingIsRefusedByName:139-144` exercises exactly that path and
asserts only the `false`). `Camera::Projection` has no channel at all.

**This is a diagnostic defect and not a data one, and the distinction is why it ranks last here:**
`[[nodiscard]]` means no caller can spend the failure without deciding, and a refused `ReadElements`
leaves `out` empty. The caller knows *that* it failed and cannot say *why*, in a reader whose whole
stated contract is naming what was missing.

**Right:** the decode path refuses through the same channel, which means it must be able to write —
`accessor 4 spans [0, 96) of a bufferView of 4 bytes`, `accessor 4 is a VEC3 of floats and an index
accessor must be a scalar unsigned integer`. **Fixed when** the overrun subject in
`AFileThatCannotMeanAnythingIsRefusedByName` asserts a wording, like the other 13 do.

## A unit test reports an absent prepared subject as a reader defect

`test/unit/gltf/TheTriangleProjectsToTheOraclesArea.cpp:104-109`. The subject is
`test/render/coverage/triangle/scene.gltf`, which § I.26.10 rules **untracked by design** — `manifest.json`
is the only tracked file in a case directory. Run against a tree carrying only tracked files (measured
2026-08-12, the manifest copied alone into an empty tree):

```
FAIL test/unit/gltf/TheTriangleProjectsToTheOraclesArea.cpp:105  the Khronos Triangle reads as a .gltf with its buffer beside it
       test/render/coverage/triangle/scene.gltf: cannot be opened
```

**The harmless reading is real and does not cover it:** the preparer is meant to have run, and the
refusal sentence does name the missing file, so nobody is misled for long. What is wrong is that *my
subject was never prepared* is being spent as *the reader failed*, in the one test in this tree that
checks anything against an outside answer — and it is the test whose red will be read hardest.

**Right** is a tier and not a skip: `doc/requirements.md` § I.20 now carries a `corpus` tier for a test
whose subject is a prepared artefact. Until it exists, the test's own first claim is *the subject is
present*, distinct from *the subject reads*. A `--allow-skip` entry is the wrong answer — it makes the
test green forever, which is the defect class this harness was built to close.

## The plan digest does not cover everything that can move a pixel, and three things move one behind its back

`src/render/plan/RenderPlan.cpp:239-254`. The digest's material is the stage set, the derived order, the
passes, the merges, the aliases, the held resources with their formats, the display transfer and the
exposure. § I.27 requires it to cover *everything whose change can move a pixel*, because a baseline is
keyed by it and the alternative is a one-token hash edit that looks like maintenance.

- **The frame extent is not in it.** `Renderer::Init(int width, int height, plan)`
  (`src/render/Renderer.cpp:68`) takes the resolution beside the plan, so the plan never learns it.
  A run at 1280 × 720 and a run at 320 × 180 produce **the same digest**, and 320 × 180 is the rung the
  picture is compared at. This is the same missing field as the residency ledger's: the catalogue has no
  extent, so `Renderer::Create` carries `256, 64` and `192, 108` as literals (`:242-246`), the AO buffer
  is silently half-resolution (`stages/AoStage.h:6`, `:24-25`) and the shadow atlas is 4 × 1024²
  (`stages/ShadowSample.h:16-17`) — four resolution classes, none of them expressible.
- **A picture-changing branch sits at a creation site, which is the one thing § I.27 forbids by name.**
  `Renderer::Create` for `Resource::VegetationTable` reads `if (VegRows.empty()) return;`
  (`src/render/Renderer.cpp:238`), so `Plan_->Holds(Resource::VegetationTable)` is **true while the
  buffer does not exist**. *The terrain shader that branched on it went with the SDL_GPU port, so the
  instance is gone and the shape is not: a `Holds()` that is true while the resource does not exist is
  still spellable, and the next resource with a data-dependent creation will re-create it.* Right: the vegetation table is a declared input of the
  plan or the plan does not hold it; the branch belongs in the declaration, not in `Create`.
- **`FB_TAA=0` retires a declared stage from an environment variable.**
  `src/render/Renderer.h:327` — `const bool TaaOn = [] { const char *e = getenv("FB_TAA"); ... }();` —
  and `Renderer.cpp:723,777` disarm the jitter and the history from it. `TemporalResolve` is now a stage
  a consumer declares; this is a second, undeclared way to turn the same thing off, it changes the
  picture, and it changes neither the digest nor `SettleFrames()`. The tree already states the rule
  against itself in the header that carried the rule until the port deleted it — *"NO ENVIRONMENT GATE.
  An environment variable is not an interface"*. `I.2`, `I.3`.

**Fixed when** two declarations that produce different pixels produce different digests, demonstrated by
the three cases above, and `getenv` appears nowhere under `src/render/`.

## The catalogue is not the whole truth about what a stage touches, so the assertions prove less than they read as proving

`src/render/plan/RenderCatalogue.h`, `src/render/plan/RenderPlan.cpp:36-42`,
`src/render/Renderer.cpp:324-376`. The six `static_assert`s are the strongest thing in this design and
two holes let an edge past them.

- **`Pull::Hold` pulls a stage's `Reads` and its `Contributes` and never its `Writes`**
  (`RenderPlan.cpp:40-41`). A held stage's `Derived` output is therefore marked held only if some other
  held stage happens to read it, and `Renderer::OnDevice` creates only what `Holds()` reports. Every
  stage in today's catalogue writes at most one resource and that resource is always read, so nothing is
  wrong on the screen — **and nothing enforces either half of that**. A stage with two outputs, or one
  whose output only leaves through a readback, silently gets an uncreated resource and a null binding.
  Right: `Hold` wants what the stage writes, which also makes the resource appear in the digest.
- **`Configure(Stage::TemporalResolve)` binds two resources the row does not declare** —
  `View(Resource::AoBuffer)` and `MeterBuf` (`Renderer.cpp:362-366`), while the row's `Reads` are
  `{SceneHdr, SceneVelocity, SceneDepth, LinearSampler, AtmosphereUniform}`
  (`RenderCatalogue.h:219-222`). It is correct today because R2 fuses the resolve with the tonemap and
  the tonemap declares both, and because the bindings are guarded by `display.HasOcclusion`
  (`stages/TaaStage.cpp:287`). But `TopologicalOrderHolds` only constrains what a row **declares**: the
  ordering that keeps `Occlusion` before the resolve is carried by `Tonemap`'s row, and if the fusion is
  ever unwound or re-aimed the compile-time proof quietly stops covering the real read set. Right: the
  fused pair's read set is a union the compiler computes, and `Configure` receives the pass's resources
  rather than a hand-picked list per stage.

**Fixed when** a stage cannot be handed a resource its row does not name — the shape, not a review rule:
`Configure` takes the plan's resolved bindings for that stage, so an undeclared one has no spelling.

## `Renderer` still constructs sixteen stage objects unconditionally, and `RenderFrame` is 170 lines

`src/render/Renderer.h` — sixteen members of the form `std::unique_ptr<T> X = std::make_unique<T>();`.
The plan now decides what is *created on the device* and what is *configured*, which was the expensive
half; the object graph still does not follow the plan, so a renderer that draws a depth buffer for a
coverage mask holds a `TaaStage`, a `StarsStage` and a `MoonStage`. `R.5`, `C.41`. The consequence is not
bytes: it is that `View(Resource::SceneLinear)` → `Taa->Output(FrameNo)` and `Light()` →
`Shadow->AtlasView()` (`Renderer.cpp:298,446`) are **callable on stages the plan does not hold**, and
answer with a null view instead of failing to compile. The catalogue's read edges are what makes every
such call correct today; nothing in the type system does.

`RenderFrame` is 170 lines, down from 310 — camera basis, jitter, ephemeris, atmosphere update,
frame-context assembly, caster collection, the pass loop and the history swap. `F.3`, `F.2`.

**And `View()` still creates a texture view per call**, at eleven sites (`Renderer.cpp:290-300`), so
every colour and depth attachment of every pass allocates one per frame — `Per.14`, `Per.15`. The count
fell sharply when `AttachmentSet` replaced the per-stage loop (the walk-like scene pass went from 24
views a frame to 3), which is why this is now a shape finding and not a cost: **no measurement of it
exists and `Per.6` forbids claiming one.** Right: create each view once, where the plan says the
resource exists, and let `View()` return a handle.

**Band 3 — waits for the SDL_GPU port**, which rewrites every one of these sites; repairing them first
would be repairing code about to be deleted. **Fixed when** a stage object exists because the plan holds
its stage, so a call into an unheld stage does not compile.

## Frame alpha is derived from depth, so a translucent body over nothing is absent from our picture and present in the oracle's — **Band 1**

`src/render/stages/Resolve.h:47,61`. The whole of the coverage predicate is

```wgsl
fn covered(sceneDepth : f32) -> f32 { return select(0.0, 1.0, sceneDepth > 0.0); }
...
let a = covered(sceneDepth);
```

and `displayed` returns that as the frame's alpha (`:65-66`). A `BLEND` surface **writes no depth** —
`core/SurfaceState.h:63-67`, `SurfaceKind::Blended` sets `WritesDepth_ = false`, correctly and for the
reason stated on the line — so a translucent surface with nothing opaque behind it contributes radiance
to `SceneHdr` and **zero** to alpha. Against a `filmTransparent` Cycles render, which carries the
surface's own alpha, the pixel is present on one side and absent on the other.

**Found by a case that passes, which is why it needs writing down.** `AlphaBlendModeTest` is green, and
it is green on a property of the **asset**: its manifest records the measurement —
*"behind all of them stands the OPAQUE Bed, a box spanning x [-4.3, 4.3], y [-0.1, 2.3], z [-0.75, 0.55]
— MEASURED from the node transforms — so every blended pixel of this subject has an opaque surface
behind it"* (`test/render/materials/alpha-blend-mode/manifest.json:31`). The predicate is well defined
**there** and undefined in general, and nothing in the engine says so.

**The harmless explanations, sought.** *No case fails today* — true, and it is the reason this is a
latent defect rather than a red: the one asset that could expose it happens to carry its own backdrop.
*Alpha is only for the readback, not the picture* — no: `Resolve.h:16-21` states alpha is the channel
that separates a black surface from the background (*measured, the oracle's sphere carries 46 101 of
46 151 covered pixels at exactly 0.0 RGB*) **and** that it *"is also the channel blending will need"*,
so it is load-bearing for the thing that breaks it. *It is scope, not a bug* — the code computes a
coverage value and claims in its own comment that this is *"the whole of the coverage predicate"*; it
answers the question and answers it wrongly for a class of input the engine already admits.

**Right:** alpha comes from what was drawn, not from what wrote depth — the scene target's own
accumulated alpha for blended contributions, composited with the depth predicate for opaque ones, so
`covered` stops being the sole source. **Fixed when** a case whose subject is a single `BLEND` quad over
empty background — nothing behind it, `filmTransparent` on both sides — agrees on alpha. **That case
does not exist and is owed with the repair**, because the defect is invisible to every asset that
supplies its own bed.

## Five camera manifests aim at a point their own stated derivation does not produce — **Band 2**

`test/render/coverage/{cube,index-widths,sphere,matrix-node,trs-hierarchy}/manifest.json`,
`scene.camera.lookAtM`. Each states its derivation as *"the framing rule of `doc/requirements.md`
I.26.10 applied to this subject's own bounds"*, and § I.26.10 aims at the bounds' centre. The declared
aim is not that point.

| case | subject bounds centre | declared `lookAtM` | offset |
|---|---|---|---|
| `cube` · `index-widths` | origin (`halfExtentM 1.0`) | `(0.00186938763, 0.000549409433, −0.00301839697)` | **3.5927e-3 m** |
| `sphere` | origin (`radiusM 1.0`) | the same triple | the same |
| `matrix-node` · `trs-hierarchy` | not the origin — a nested chain | `(1.06417013, 0.625490847, −0.00269665869)` | the same tail on `z` |

**Measured structure, and it is what makes this a defect rather than a rounding artefact**: the offset is
a **pure image-plane displacement** — its dot product with Forward is exactly 0 — in the **same direction
in the camera basis** across all eight cases that carried it (`0.809724 · Right + 0.586811 · Up` under
the declared roll), with world magnitude proportional to the subject's distance, so **the pixel value is
identical to nine digits: 0.435660418 px**. A quantity that is constant in pixels across subjects at
different scales was applied in pixels, once, by something.

**Nothing has been found that produces it.** It first appears **hand-written at `c5275c1`**, a commit that
added no camera-generating script, and none has existed since. It is not a float32 round trip of the
centre.

**The harmless explanations, sought.** *It is the framing rule's own output* — no: the rule aims at the
bounds' centre and these are not it, and the three cases constructed at `8f0ecce` carry either the centre
or an aim § I.26.14 derives. *It is too small to matter* — 0.4357 px is **87× the oracle's 0.005 px filter
half-width** and these are coverage cases whose acceptance is a sub-pixel distance to an edge. *It is
harmless because it is consistent* — consistency is what makes it a rule somebody applied, which is
exactly the thing that must have an origin.

**Note the instrument, because it decides how this is found again**: `grep` for `0.435660418` over the
tree returns **nothing**. The number is not a literal anywhere; it is a **derived** property of the
declared `lookAtM`, so only computing it from the manifests finds it. That is the same lesson as
`doc/requirements.md` § I.25.1's *a grep proves a string is absent, never that a capability is*, reaching
a number instead of a feature.

**Right:** the aim is the bounds' centre, as the derivation says, or the offset carries a derivation of
its own — `derived`, `measured` or `[SET]` per `CLAUDE.md`. **Fixed when** every `lookAtM` in the suite
either equals its subject's bounds centre or names why it does not. **Decides it:** recomputing the aim
from the declared bounds and refusing a mismatch, in the runner that already recomputes the margin.

*Not a defect, and recorded here so the same investigation is not run twice: the clip range's origin
**was** found. `blender --factory-startup` reports `clip_start = 0.10000000149011612` and
`clip_end = 100.0` — Blender's factory camera, the same source those manifests already cite for the lens.*

## The preparer and the runner hold two closed sets over one declaration, and they disagree on eight of twenty-six manifests — **Band 1**

`test/corpus/prep/manifest.py:495` — `_fields("manifest.scene.material", value, ("source", "kind"),
("note",))` — is a **closed** field set that does not know `carriedBy`. `test/render/Parity.cpp:433`
**reads** `material["carriedBy"]` and refuses by name when it is wrong (`:254`). Eight manifests carry
the key. So `python3 test/corpus/prepare.py dry-run` refuses **8 of 26** on
`manifest.scene.material.carriedBy`, while the runner requires it.

**Pre-existing, verified by stashing** — unrelated to the `acceptanceClass` key added at `8f0ecce`.

This is § I.20's duplicated-`INC_*` shape in a third place, and the third time this tree has written one
fact twice and had nothing fail when the two drifted: **what a manifest may contain is the schema's, and
the runner restates it by reading keys the schema has never heard of.** A closed set is the right
mechanism and two of them is the defect — the preparer's refuses what the runner needs, and a key the
preparer accepts but the runner never reads would fail in neither.

**Right:** one declaration of the manifest schema that both sides read, so a key exists once. **Fixed
when** `dry-run` accepts every manifest the runner accepts and refuses every one it does not, checked by
running both over the whole corpus — which is a test, not an inspection. **Band 1**: the Khronos work
adds manifests, and every one added under a split schema is added twice.

## Three node-transform cases measure an ambient-occlusion estimator at one sample and report the answer as a placement

`test/render/coverage/trs-hierarchy/manifest.json`, `matrix-node/`, `sphere/` — the material block,
`"kind": "diffuse"`, with `samples: 1` and `bounces.diffuse: 0`. Measured at `124504a`:
**`trs-hierarchy` vs `matrix-node` differ by 5 899 px, of which 5 896 are colour-only**, in the contact
regions where their three cubes touch; `sphere` differs by 62, of which **60** are the same thing from
shading-normal self-occlusion.

**The mechanism, and it is exactly diagnosable rather than inferred.** The oracle's departures from
`ρ·L` are **binary — `ρ·L` or exactly 0, never between**. At 1 spp with `diffuse_bounces = 0` a pixel
takes one cosine-weighted direction; it escapes to the environment and returns `ρ·L`, or it meets
geometry and returns 0. The pixel is a **Bernoulli draw whose mean is the visible sky fraction**, so
these cases carry an ambient-occlusion integral that § I.26.13's four reductions do not remove and that
no seed makes deterministic.

Each manifest states the material is not read — *"Nothing in a coverage comparison reads it; it is here
so the picture a person opens is not black"* — and the comparison reads it. That is the defect: a case
whose declaration and whose acceptance disagree about what it measures.

**The harmless explanations, sought.** *It is noise and belongs under a tolerance* — no: 5 896 px is a
contact **region**, not a boundary, and § I.26.13's own rule is that a non-reducing oracle is lowered and
never accommodated. *It is a difference between our renderer and Cycles* — no: `trs-hierarchy` and
`matrix-node` are compared **against each other**, and the same subject placed two ways cannot differ in
its geometry; the 5 896 are two independent draws of one estimator. *The three colour-free pixels are the
real finding* — yes, and they are the only part of this measurement that is about node transforms.

**Right:** the material becomes `emission` (`doc/requirements.md` § I.26.13), one colour per node where a
case has more than one, geometry untouched. **Not** separating the cubes — that repairs the oracle by
changing the subject, and it cannot repair `sphere` at all. **Fixed when** two renders at two seeds are
bit-identical for these three cases, which is a stronger statement than the pixel count falling.


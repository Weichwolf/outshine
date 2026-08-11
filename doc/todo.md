# Todo

What is next, in order. A step names what must be true when it is done, not how — short enough to scan.
**A step that is done leaves this file**: it is then in the code, and its measurements are in `git log`.
What survives a finished step is only what a later step needs to know.

Order is not preference: 1 and 2 are preconditions for anything that places anything, and 4 is the gate
that makes 5 onwards enforceable rather than intended.

**Quality work does not start until this file is empty.** The skeleton is best practice first; after that
everything further is generators and shaders, and step 4 is what makes that true — a generator that
includes the renderer is then a compile error. Until then a picture finding is recorded and ranked, never
graded against the references: the answer is known and the round is spent. Then, in two stages:

| | |
|---|---|
| **1** | one scenario, scenes at places the owner knows well. Render, look, pull generators and shaders after it, loop until the quality stands. **Judged by eye, not by metric** — a number decides whether the frame floor holds, never whether it looks right. Ortho­graphic diversity matters here and nowhere earlier: a structural step only has to prove it did not move the picture, and one meadow shows that |
| **2** | the webcam scenario, several cameras, times of day and night, weather. Against those the whole lighting and weather model is fitted. It renders at the declared size and downscales **both** sides to the comparison rung; camera poses are resected against buildings, and `git log` holds six already fitted |

## Where it stands

| | Step | State |
|---|---|---|
| 1 | classification off the render thread | **done** · `262e5fc` |
| 1.5 | heap and stack telemetry, with the instrument's own range | **done** · `9379f6f` |
| 1.6 | the scenario declares the internal render resolution | **done** · `023c407` |
| 3 | move what is misfiled, delete what is dead | **done** · `81b828c` |

| 2 | the height oracle evaluates the drawn surface | **done** · three slopes, 0 violations in 48 standpoints |
| 1.7 | the fixed heap, over all of the client's memories | **done** · client reserved 512 → 200 + N×56 MiB |
| 4 | the server target, and the checker falls | **done** · `World` no longer holds a renderer pointer |
| 4.5 | fold the tile worker into the client | **done** · one module, one cap, one priority key |
| 4.6 | the GPU readback stops blocking the frame thread | **done** · unwinding off, render p99 6.1 → 2.1 ms |
| 5 | `generators/` | **done** · the include set is the enforcement, both ways |
| **6** | **the forest becomes a generator** | **in the tree** |





| 7 | one geometry stage | open |
| 8 | regionalise | open |
| 9 | buildings, water surface, infrastructure | open |
| — | the scenario interface and its JSON loader | open — **designed**: headers, schema and acceptance stand |

A step that is **done** has left this file: it is in the code and its measurements are in `git log`. What
survives below a finished step is only what a later step needs to know.

**What the list is for, named by what one sees at the end of it:** a working plumb (2) · trees placed,
randomised and instanced properly (5, 6) · many trees **and houses** drawn fast (7, 9) · **correct in
motion** (8, and the impostor silhouette) · and only then appearance, tuned by eye.

## The acceptance instruments

**Four steps below accept on a distribution and two instruments are refuted.** These are not limits;
they are missing tools with a cost, and each blocks a specific "done when".

**The counterbalanced block design is not an instrument.** ABBA's arm contrast is `(+1, −1, −1, +1)` —
exactly the quadratic orthogonal-polynomial contrast over four ordered positions. It removes linear drift
and aliases *curved* drift into the treatment effect at unity gain, so **more blocks make a false positive
more significant**. Measured: two identical arms, 12 blocks, p99 difference −1.86 ms at p = 0.043, and the
p99 means by position (34.10 / 36.36 / 35.71 / 34.26) *are* the effect. Fix, one line: randomise order
within the block — 20 randomised blocks put all three quantiles at |t| < 0.8. Two things go with it:
publish the resolvable floor beside every result (≈1.0 ms p50, ≈2.1 ms p99 at 20 blocks), and **stop using
a run's p50 as the statistic** — inside one 240-frame run `frameMs` climbs from ~11 to ~25 ms as the world
streams in, so it mostly measures how far streaming got. Frame-index-matched on a declared path halves the
noise for nothing.

**The per-pass GPU timer does not partition the frame, natively as well as in the browser.** Native:
compute 0.43 + shadow 3.11 + scene 7.61 + ao 5.72 + taa 6.43 = 23.3 ms against p50 9.38. A begin/end
timestamp pair measures a span on the GPU timeline; pipelined passes overlap and must not be summed. It
licenses exactly one statement — *did pass P's span move between two builds of the same declared scene* —
and never "pass P costs N ms". **Step 7 accepts on "the scene pass stays flat within noise" and cannot be
evaluated with it.** The missing tool is one column: a single pair spanning the whole encoder,
`gpuFrameMs`. Then `Σpass / gpuFrameMs` says which regime holds — above 1 means overlapping spans and
attribution is forbidden. Two query slots, no new code path. Also `FrameTelemetry.cpp:66-72` publishes each
pass as a **mean** against `CLAUDE.md`'s "never a mean", and compares it to a frame percentile.

**Nothing compares the picture across the two clients.** `verify-clients` proves both are entry points
over one scene builder and no more; measurably they differ, and byte-identity is dead as an instrument
here as everywhere. The tool, and it is cost rather than a limit: **the wasm client must execute a `runs`
block and return the still and the depth**, the product `SceneRunner` already writes natively — `fb-sim`
collects log and telemetry over HTTP, so the sink exists and the missing piece is a readback and a POST.
Both sides pinned: declared size, `jitter` pinned, `settleFrames` declared, same instant, same scene.
**Publish the self-noise floor first** — native against native and browser against browser — because a
tolerance without a zero point decides nothing. The statistic is **coverage, not colour**: the fraction of
pixels whose sky/not-sky classification differs against a mask frozen on one side, which separates a
wedge of thousands from TAA edge noise of hundreds by an order of magnitude. And it runs in **motion**: a
seam that appears and vanishes as the camera walks is a cell-seam pop, one welded to a stand is a card
defect, and one frame cannot tell them apart.

**Geometry has invariants, and they are decidable without any reference at all.** Not consistency, not
plausibility, not taste — true or false, and cheap. Nothing checks them today, and a wrong normal makes
the shading wrong everywhere without ever looking like a shading defect. What a mesh check answers, per
mesh, on the generator's own output:

| | |
|---|---|
| **normals** | unit length within tolerance · agreeing in sign with the triangle winding · agreeing with the geometric normal within a stated angle. A vertex normal that disagrees with every face it belongs to is a defect regardless of how it looks |
| **welding** | no two vertices at one position carrying the same attributes — a split vertex is legitimate **only** where a seam is declared (uv, material, hard crease), so the check is against the declaration, not against the count |
| **closure** | every edge shared by exactly two triangles where the body **declares itself closed**. A building is closed; a terrain patch, a leaf card and a water surface are not. So closure is a **declared property of the yield**, and the check enforces what was declared rather than one rule for everything |
| **degeneracy** | zero-area triangles, NaN or infinite coordinates, indices past the end, a winding that flips within one surface |

It belongs to the generator contract: the yield declares `Closed`, and the check runs on what a generator
produced — which is where it is cheapest and where a violation names its author. Until then it can run on
what exists: the terrain mesh, the building footprints, the bark.

**The drawing steps have hard instruments and none of them is taste.** What decides whether many trees
and houses draw fast is measurable, and most of it is not measured here yet:

| | What it answers | Today |
|---|---|---|
| **screen-space error, measured** | the ladder declares τ pixels; nobody has measured the actual pixel deviation between the chosen cut and the finest. Render both, difference the silhouette. It decides whether the ladder is too tight (triangles wasted) or too loose (popping) — and popping is then a *predicted* defect rather than a discovered one | not measured; the tool is two renders and a difference |
| **triangle size distribution** | the number for foliage. A triangle smaller than a 2×2 quad wastes three quarters of the shading, so a scatter that looks cheap in triangles can be ruinous in fragments. p50/p95 of projected triangle area in pixels | not measured |
| **overdraw** | fragments shaded per output pixel. Alpha-tested cards shade many times what they keep, and this is where a forest actually costs | not measured |
| **culling yield** | primitives submitted against primitives that produced a visible fragment, per stage — frustum, backface, small-triangle, occlusion. A stage submitting what it cannot see is doing arithmetic for nothing | not measured |
| **early rejection** | fragments killed before the shader runs. Its counterpart is the depth order the passes are submitted in | not measured |

Each is a number a GPU or a readback answers, none needs a reference outside the tree, and none is a
matter of opinion. **Appearance is the exception and it is judged from the screenshot, at the image** —
not from a metric standing next to it.

**When performance work happens is a trigger, not a schedule.** At 720p60 nothing is optimised. When
720p30 can no longer be **held** — the floor, p99 under 33 ms, not the mean — it is optimised back up to
720p60. The steps below that carry millisecond acceptance numbers are architecture and run in order
regardless; the trigger governs work that is *not* on this list.

## Carried, for the steps that need it

**The browser's tile pool does not scale, and that is the whole load regression.** Measured on one
binary by varying the thread count, `demo/frame`, ms per tile:

| pool threads | wasm | native | wasm tiles/s | native tiles/s |
|---|---|---|---|---|
| 1 | 316.1 | 153.2 | 3.04 | 6.29 |
| 3 | 537.6 | 214.2 | **5.29** | 13.44 |
| 4 | 724.7 | 232.9 | **5.24** | **16.41** |

**Wasm saturates at three threads — the fourth costs a core and returns nothing** — while native reaches
16.4 tiles/s, which *is* the 16–18 of the 8.3 s era. Single-threaded wasm is only **2.06×** native; all
the rest is scaling loss. The load is 95 % mesher: 130 × 0.726 / 4 = 23.6 s against 24.8 measured.

Refuted on the way: the link level (`-O1` → `-O2` buys 3.0 %, kept anyway — faster, 17 % smaller, and it
removes an asymmetry with the pre-fold worker) and the unwinding mode (3.6 %). What remains is that the
pre-fold mesher compiled **without `-pthread`** into its own heap and now compiles with it into a shared
one. **The discriminator, not built: pace the loading loop to 60 Hz and re-measure at four threads.** If
ms/tile falls toward 316 it is scheduling; if it does not, it is the allocator.

**The loading loop renders ~2 200 unpaced frames per second.** The old 4 ms nested-timeout clamp paced it
to 174 by accident. A progress bar needs 60, and on the declared ceiling that is a core taken from the
mesher during the phase this is trying to shorten — which makes it the first half of the discriminator
above. It also emits 2 000-fps telemetry rows, so any later step pooling frame samples must filter on
`resident`.

**The log sink is written from pool threads with no lock.** `Log::Sink_` is process-wide, nothing calls
`SetThreadSink`, `TilePool` logs from workers, and `ServerLog::Write` appends to a bare `std::string`
that `Flush()` slices on the main thread. `CP.2` — and it is the instrument every remaining step accepts
on.

**Everything that waits became a stepper, and the pattern was not named.** Five enumerations for one
three-state answer — `ReadState`, `Bring`, `Delivery`, and two `Progress`. One `Turn { Waiting, Done,
Failed }` in `core/` deletes four, before step 5 needs it for region jobs. `SceneRunner`'s flat 16-state
`Stage` is three nested sequences pretending to be one, with `CompareClasses()` reading `Stage_` inside
itself to decide which half of itself to run.

**Step 8 — one watermark couples every tile.** `BuildingField`'s `Consumed_` is a single watermark and
`Build` refuses to pass a deferred tile, so one permanently-Pending tile stalls **every later tile's**
buildings where before it lost that tile and moved on. Defensible while there is no timeout on the load;
it needs per-tile deferral once regions arrive. `WaterField.cpp:54,90` still folds Pending and Hole into
one counter with the watermark advancing — the same defect, now visible in the type.

**Step 8 — the class structure cannot be appended to.** The grid is anchored on the camera and dense, so
a re-anchor changes what every index means; features are laid down in ascending rank, so a later one
rewrites cells an earlier one won; and a cell's seeds must be contiguous because the fragment reads them
as a range. Append-only needs an absolute region grid, non-contiguous seed lists in the fragment and rank
resolution at evaluation time — step 8's structure, not a variant of this one. A crossing's cost is the
8 MB class upload, one per publish; streaming and ingest contribute nothing measurable.

**Step 5 — the discard path is deliberately absent.** Every reader runs on the render thread inside the
frame that asked, so there is no stale result to discard. The version is published; it is the hook when
a reader becomes asynchronous.

**The memory ledger, after 1.7.** Reserved is **`200 + N×56` MiB**, `N = clamp(hardwareConcurrency − 2, 1, 6)` — one host's 424 is not the client's number, and the **ceiling case is 536**, above the 512 the fixed heap was meant to beat. The A18-Pro-class check belongs at N = 6, not N = 4.

**Main is 200 MiB because of a run product.** `demo/classes` holds one `spanM 400 / stepM 0.05` raster — 8000² + header = 61.04 MiB of a 159.13 high-water — and the sizing rule was applied to it literally. Sized off the largest *play* scene it would be **120 MiB**, and the client 344. The fix is not a rule change: write the dump in row bands through a chunked artifact sink. `HttpPost` takes one buffer today, so the sink is the real work — ~70 lines for **80 MiB**.

**Two instruments are three lines each and are worth more than the next measurement.** `workerHeapPeakMaxKB` — the row publishes `Σ` over worker modules and throws the **max** away, so one module peaking at 50 while another sits at 37 reads as 43.5 and nothing in the archive can tell you. And a **`verify-memory` gate** that parses both binaries' memory sections and fails when `initial ≠ maximum`, or when a declared cap disagrees with the newest telemetry peak by less than the stated margin — ~30 lines, and it turns the whole step from a claim into a gate.

**The ≈0.15 ms guard cost stands unrefuted.** A frame-level p50 at n = 4 per arm is underpowered by about 25× — detecting 0.15 ms at that spread needs ~97 runs per arm — so it says nothing either way. "Four runs cannot resolve it", not "this host cannot".

Beside the main module
the tile pool runs `N = clamp(hardwareConcurrency − 2, 1, 6)` further wasm modules, growth-enabled.
Reserved is 256 + N × 64 MiB; main's in-use is **97.8–108.6 MiB measured**, the workers unmeasured. **The
144 MiB between main's reserved and used is the step's first move.** Stack high-water, browser, KiB
(peak · floor · limit · capacity): frame **18.6** · 4.2 · 516 · 4096; class **4.2** = its own floor, so it
reads "≤ 4.2, unresolved below". The probe publishes the break and the ceiling but not the linear memory's
current size — one call, one column. The per-access guard costs **≈ 0.15 ms of CPU**, measured per stage
because `frameMs` cannot resolve it.

**Step 5 — the include set must bound real generator sources, not three named files.** `verify-generators`
compiles exactly three paths and `INC_GENERATORS` is used nowhere else; real sources would land in
`SIM_SRCS`, which the `world` target compiles with `-Isrc/world` on the line, so `generators/Forest.cpp`
could include `World.h` and every target would build. The gate must compile the wildcard minus its two
negatives, and `generators/` needs its own compile group in the real targets — otherwise step 5's
"impossible rather than prohibited" is prohibited by a three-file list.

**Enforcement lives in one of three targets.** `walk` and `wasm` compile the same `world/` sources with
`-Isrc/render` present, so an upward include fails in `make world` only. The shape that closes it pays
for itself: compile `SIM_SRCS` once with the simulation include set and link those objects into all
three, which also removes two full recompiles of ~25 translation units per round.

**Step 4.5 / 4.6 — the oracle builds a tile on the frame thread.** `Outshine::Look` calls
`fb_stream_ground` on **every camera update**, so a z14 crossing during play means five synchronous PNG
decodes (≈8.6 ms at 1.71 ms each) with a possibly synchronous fetch behind `fbs_size`. Not created by
step 2 — the cache was 6 and is now 5, so the in-play stall moves by about one decode — but the
"it runs once inside the loading phase" justification is false for this caller and must not travel
forward.

**Step 4.5 — the byte budget is unserved where it is needed.** The tile byte caches read 1.3 MiB in the
browser against 33.8 MiB natively: the browser's decoded tile bytes sit in the worker modules, so the
platform with the eviction problem is the one with no number. **And there are two byte caches natively
holding the same keys** — `fbp_cache` in the pool, `fbs_cache` on the main thread, identical key space,
zero sharing.

**No device ceiling is declared.** The device holds 234.3 MiB, 208.9 of it tile geometry; which budget a
device allocation is charged against is the open question `architecture.md` names.

**A canvas costs frame time that no pass explains.** `demo/crossing`, 900 frames, 14 runs: p50 of
per-second p50s **18.297 ms at 640×360 against 19.606 at 1280×720** (Δ +1.309, se 0.284, t = 4.61,
p ≈ 0.002), monotone in canvas pixels — but the excess sits in **every** pass including ones that cannot
see the canvas, while the present pass carries 0.33 of the 1.98, and the renderer's work is provably
identical across all 14 runs. So the canvas moves device or compositor state, not render work.

## 6 — The forest becomes a generator

Renderer reference out, camera knowledge out, callback and `void*` become the ground view, mutable
counters move into the yield. **Growing a prototype is not a generator call** — it happens once per run at
bring-up. This is the cut most easily got wrong.

Behaviour-neutral: same picture, different call chain.

**The budget number, and it did not exist before step 5.** A class probe costs ≈70 ns — a 12.4 ns
geodetic hop plus the evaluator — so the forest's **204 304 candidates per z14 region are 14.3 ms**.
That is the design number for this step and for 8, measured native at `-O2`.

**The Makefile risk that is not fail-loud, and this step walks into it.** `walk` and `wasm` compile
clients, world and render in one command with one include set, so giving the client `-Isrc/generators`
grants it to every `world/` file in the same breath and peer isolation goes silently. Compile
`SIM_SRCS` once with the simulation include set and link the objects into all three — the pattern step 5
invented for `core/` and `generators/`, applied where it is next needed.

**`DrawSink::Add` returns a `bool` with `Full()` beside it** — the exact shape step 5 removed from
`OccupancySink`. Nothing writes into it yet and its causes are step 7's to name, so it is named here
rather than guessed at.

## 7 — One geometry stage

Tiles, trees, buildings and water merge into one stage over one cluster cut. The renderer loses every
field naming a part of a plant.

Done when: the pass count is unchanged and the scene pass stays flat within noise.

## 8 — Regionalise

Ring of regions around the viewer, request / collect / release / cancel, generation off the render
thread. **Measure milliseconds and bytes per region** — that number does not exist today.

Done when: a region crossing is invisible in the frame distribution at the highest declared speed, over
repeats, and popping is judged from a moving capture.

## 9 — Buildings, water surface, infrastructure

Footprints and the water surface become generators; the water *level* stays in the core. Then
infrastructure.

**Night city lighting is owed and exists nowhere.** `/t/lights` and its 587-line producer are gone
with the client half that never had a caller; OSM street lamps are genuine vector data and the picture
target has a night. It comes back here, placed by a generator, and the endpoint is rebuilt with it.

## Later

- GPU emitter for scattering, with the C++ generator as its oracle. **Not before 8** — without the
  per-region number every move is guessed.
- Split the tile loader: the cache and height half is server-side, the mesh and DAG half is picture-side.
- **The wasm link's optimisation level is an artefact, not a decision.** Everything else builds at the
  higher level; the browser link and one translation unit do not, with no reason written anywhere.

## Traps if they wait

- The tone-mapping slot in the pass enumeration is empty since the fold. A dead slot is where a new pass
  hides without the count moving.
- The comment on the vegetation row claims a size the structure no longer has, and it is uploaded verbatim
  with its field meanings pinned against the shader.
- `scenarios/` is the decided name; the tree still says `mods/`.
- Comment density is far above the rule, and the worst file is more than half prose.
- **A pinned binary does not reproduce its still.** Bounded: two of three states are the same picture to
  within one code, the third differs in two pixels of 921 600 on four scanlines at one silhouette edge.
  Byte-identity dies as an instrument; tolerance comparison does not. It is a determinism violation —
  the result is coupled to tile arrival order. **Likeliest cause, and cheap to test:** a temporal pass
  whose history length depends on pace. Shoot once with that pass off; if byte-identity returns, the
  coupling is the history.
- **Seen from directly above, all 15 995 stands vanish.** `demo/ortho` shows sparse dark dots where a
  forest stands — camera-facing cards seen edge-on. The same representation that gives the bow-tie from
  the side gives nothing from above, and a world sandbox has a bird's eye. Step 6.
- **`subject-meadow` writes its product and the product is empty.** All 57 frames are bare substrate,
  grid and grey card, `fillPct=0` in both clients. `SubjectBench::Select` sets `Bucket_` and `Kind_ =
  Herb` and `Bucket_` is never read again: there is no herb geometry path in the bench. `subject-beech`
  fills 17.2 %, so the rig is sound and the subject is missing.
- **The mid-distance crowns read as hourglass / bow-ties, and this is the one to fix first.** Silhouette
  is the only currency at the comparison rung and a bow-tie is not a shape a tree has — the impression
  dies in well under a second. Signature: a two-quad cross seen near edge-on, or a card whose alpha cut
  leaves the waist. What is right: the cross never survives to the range where its own geometry is
  legible; SpeedTree practice and Guerrilla's *Horizon* hand that band to a camera-facing or octahedral
  impostor first. Step 6.
- **The near crown reads as scattered dark flakes rather than one mass.** A beech stand carries LAI
  4.5–5.1 m²/m²; at any value in the measured 1.7–7.5 range a crown occludes essentially all sky through
  its own depth and must read as one mass with a lit top and a shadowed underside. Separated dark
  elements mean card coverage an order of magnitude under the species' own leaf area — density and
  self-shadowing, not detail.
- The foreground sward is a tonal field inside 10 m, and at the comparison rung that is legitimate:
  blade structure does not speak there. Recorded, not owed.
- **The mid-distance impostor trees are not tree-shaped.** Looked at directly in `demo/walk` at 1280×720:
  five to six of them carry a large angular zigzag of foliage — folded-ribbon or bowtie silhouettes with
  right-angle corners, not a crown with a bite out of it. An octahedral-impostor cell seam, in both
  clients. It is by a distance the most damaging thing in the frame at one second of looking. Recorded,
  not worked: the vegetation goes through the generator cut at 6 and 7 anyway.
- `GpuTimer::Pass::Cloud` is the enumeration's dead slot.
- **"Pending" and "hole" are the same `double`, and the two clients resolve it oppositely.**
  `kFBElevationUnresolved = -1e9` with `ElevationResolved(m) { return m > -1e8; }` — and **six** bare
  literals test it by hand (`WaterField.cpp:57,94`, `BuildingField.cpp:54`, `TreeField.cpp:81,89`).
  Natively the oracle blocks, so pending never occurs; in the browser a pending stand is **silently
  dropped and never retried**, indistinguishable from "nothing grows here". The shape that fixes it is a
  return type: `GroundSample { enum class State { Resolved, Pending, Hole }; double AslM; }` — then a
  caller cannot place on a sentinel, deferral must be written deliberately, and the six literals cannot
  be spelled.
- **`World::Center` puts every tile centre at `alt = 0`**, so `WantSplit`'s distance at a standpoint of
  altitude *A* is at least *A*: a pedestrian at 2688 m gets a coarser tile under his feet than one at
  −75 m. `AddWork` already prefers the node's real ECEF origin when a mesh exists — the quantity is not
  missing, it is used in one of three places. Same function, same class of error as the next line, and
  they are one fix: **`WantSplit`'s distance is measured to a point that is not on the terrain, and its
  focal length is not the projection's** — for `demo/ortho` the true on-screen tile edge is
  `SpanM × Height/orthoM`, the ladder computes `SpanM × 443.4/2500`, so it stops splitting at ≈2.4 ×
  `kEdgeTau`. Under `orthoM > 0` the honest metric is distance-free.
- **The height bound is stated in two places with no bridge.** `ChunkSurface.h`'s
  `kSurfaceAgreementM = 9.17e-4` and `tools/surface_budget.py`'s `sum(envelope())` are one statement;
  nothing compares them, so step 7 moving `kGrid` or the ladder makes the header lie silently. Three
  lines of Python: regex the constant out of the header, compare, non-zero exit.
- **`TerrainLoader.h:49` still says the DEM pointer is "owned by the byte cache — do NOT free".** It is
  now `fbs_hold.data()`, valid only until the next `fbs_size`. One caller today and it copies
  immediately; 4.5 adds callers to this island.
- **The chain is faithful to its source; the error is in the tile data.** `fb_world` against the z14
  terrarium texel at the same point: demo +0.09 m · Ardèche +0.70 · Preikestolen +1.96 · Badwater −0.03.
  So the gap to the published 604 m and the surveyed −85.5 m is the **source's** vertical accuracy, not
  the engine's — and Badwater is the sharp one, because a salt pan has no smoothing alibi: **10.9 m on
  flat ground**. Two confounds first: whether the declared scene coordinate is the landmark, and what
  the server's native maximum zoom is at each place (`/t/terrain/15/…` returned non-PNG at both, so z14
  may be the finest served).
- **Water depth can be negative and the type permits it.** `LevelM` is the 5th percentile of the ring's
  own **shore** samples, so inside a gorge the DEM sits above it and Ardèche answers −4.371 m. Depth
  should be non-negative by construction, with "the level model and the DEM disagree here" a state the
  return type carries rather than a negative number a caller must remember to check. Step 9.
- **`Sim &Simulation()` is non-const and makes a new mistake spellable.** `App.Simulation().Look(s)`
  compiles and moves the eye without the camera basis and scene state that `Outshine::Look` sets. All 20
  call sites are reads; drop the non-const overload.
- **Three names for one sentinel** — `World::kNoSurfaceAslM`, `Standpoint::kNoRoof`, and bare `-1.0e30`
  in `WaterField`, `BuildingField` and `TreeField`. The producers cannot name the constant because it
  sits in `World.h` and peers do not include peers; it belongs in `core/`, or it dies the way its
  elevation twin does — as a state in the return type.
- **The negative gate does not assert why it fails.** Any compile error passes `verify-generators`; one
  clause fixes the class: `2>&1 | grep -q "file not found"`.
- **`core/ClusterDag.h:75` reads `FB_TAU` from the environment into a function-local static.** The
  picture depends on an undocumented environment variable, and the value carries no origin at the call
  site. Principle 7 — if the environment decides the result, the coupling is a bug.
- **The transmittance LUT cannot answer how much blue the atmosphere removes on the path to a lit
  surface.** Tapped for that purpose it returns a 4.7 % blue lift where the same model's own extinction
  gives 19.6 % — the wrong instrument for the question rather than a mis-tuned one. March instead, when
  the deck base gets a source.
- **`osmDefault` is one global `"meadow"`.** Unmapped land anywhere on Earth grows 700 blades/m²: Death
  Valley's floor is a sward, and there is no arid template among the thirteen. The fix is a per-place
  default and it belongs to the vegetation generator.
- White limestone and rock patches read as snow at 36 N in August.
- `WaterField.cpp:57,92` and `BuildingField.cpp:54` test the unresolved-elevation sentinel with a bare
  `<= -1e7` while `core/ElevationProvider.h` declares `ElevationResolved()` as `> -1e8` — one predicate,
  three literals, two thresholds.
- The near-field ground is a shader and nothing else — no ground cover is built at all, so "no blades" is
  an absence, not a defect. It belongs with undergrowth after 9.
- German comments from earlier rounds. The history stays; what is touched gets translated as it is
  touched.
- **Naming needs a pass of its own.** A name that needs a comment is the wrong name. Borrowed jargon and
  magic sentinels where the type system has an answer are the two patterns. New identifiers are held to
  this as they are written; the existing ones are a separate sweep.

## Open, owner's decision

- A persistent server is declared in `vision.md` and not built. When it is, either the web host absorbs it
  or a third container appears — both touch a principle that says everything runs in the client.

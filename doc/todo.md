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
| **2** | **the height oracle evaluates the drawn surface** | **in the tree** — built, rejected on one clause of three, being fixed |
| 1.7 | the fixed heap, over all of the client's memories | open |
| 4 | the server target, and the checker falls | open — **the gate**; 5 onwards is enforceable only after it |
| 4.5 | fold the tile worker into the client | open |
| 4.6 | the GPU readback stops blocking the frame thread | open |
| 5 | `generators/` | open — **designed**: headers, enforcement and acceptance stand |
| 6 | the forest becomes a generator | open |
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

**When performance work happens is a trigger, not a schedule.** At 720p60 nothing is optimised. When
720p30 can no longer be **held** — the floor, p99 under 33 ms, not the mean — it is optimised back up to
720p60. The steps below that carry millisecond acceptance numbers are architecture and run in order
regardless; the trigger governs work that is *not* on this list.

## Carried, for the steps that need it

**Step 8 — the class structure cannot be appended to.** The grid is anchored on the camera and dense, so
a re-anchor changes what every index means; features are laid down in ascending rank, so a later one
rewrites cells an earlier one won; and a cell's seeds must be contiguous because the fragment reads them
as a range. Append-only needs an absolute region grid, non-contiguous seed lists in the fragment and rank
resolution at evaluation time — step 8's structure, not a variant of this one. A crossing's cost is the
8 MB class upload, one per publish; streaming and ingest contribute nothing measurable.

**Step 5 — the discard path is deliberately absent.** Every reader runs on the render thread inside the
frame that asked, so there is no stale result to discard. The version is published; it is the hook when
a reader becomes asynchronous.

**Step 1.7 — the client has N + 1 linear memories and the ledger measures one.** Beside the main module
the tile pool runs `N = clamp(hardwareConcurrency − 2, 1, 6)` further wasm modules, growth-enabled.
Reserved is 256 + N × 64 MiB; main's in-use is **97.8–108.6 MiB measured**, the workers unmeasured. **The
144 MiB between main's reserved and used is the step's first move.** Stack high-water, browser, KiB
(peak · floor · limit · capacity): frame **18.6** · 4.2 · 516 · 4096; class **4.2** = its own floor, so it
reads "≤ 4.2, unresolved below". The probe publishes the break and the ceiling but not the linear memory's
current size — one call, one column. The per-access guard costs **≈ 0.15 ms of CPU**, measured per stage
because `frameMs` cannot resolve it.

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

## 1.7 — The fixed heap, over all of the client's memories

The main module still grows and still carries the per-access guard for a growth that never happens.
Fixing it alone would settle the smaller of two heaps and leave the growing one untouched, so the tile
worker is part of this step and not of a later one — twenty lines of `sbrk(0)` and one stack purpose over
the `postMessage` channel it already has. Step 4.5 folds that module away for good; until it does, the
client's largest growing heap is the one nothing measures.

Done when: no wasm module in the client grows, a failed allocation aborts naming the item and its bytes,
every module and every thread publishes its own **heap and stack** high-water, the row states the client
total as `main + Σ workers` in both reserved and in-use terms, **each module's initial memory is derived
from its own high-water with the margin stated**, and the per-purpose stack sizes are set from that
measurement instead of from the toolchain default.

## 2 — The height oracle evaluates the drawn surface · IN THE TREE

The oracle interpolated the DEM a second time instead of evaluating the surface that is drawn. Anything
placed on that height stands wrong, and no generator can repair it.

Done when: oracle and drawn mesh agree to a **stated bound**, same posting indices, **same triangle split**
— and "the drawn surface" means **the finest tile of the ladder**, not the tile currently drawn: a
placement that followed the LOD would move as the camera walks, which principle 7 forbids.

Measured, three declared plumb scenes at 6.7° / 25.0° / 69.9° of slope: **+0.076 · −0.098 · +8.99 m**
before. The nine metres at the fjord wall are why a salt pan cannot test this. The residual was a
near-cancellation of three terms each about twice its size — source zoom +0.143, chunk decimation −0.140,
interpolant +0.098 — and the diagonal contributed nothing.

Open against the clause: the evaluator does not read the winding array its header says it reads, so the
diagonal is still stated twice; the bound is a three-point maximum and not a budget; the two-pixel tree
line is unattributed while the counters that would attribute it are already emitted.

The **4.35 → 18.84 ms** at the forest rebuild is **not** a stream-in hitch and the framing is struck:
`Forest::Scatter` latches on `Scattered_` and fires once, inside the loading phase, where a hold is what
the loading screen is for. The finding that survives is the cost itself and the duplicate byte cache
behind it, and both belong to the loader split.

## 4 — The server target, and the checker falls

Split the object that owns world and renderer into a simulation half and a picture half. Add the target
that builds everything except `render/`. Invert the remaining renderer-driving calls in `world/` until it
links. **Delete the layer checker in the same commit** — two truths about the structure side by side is
the state that ruined the first one.

**`/t/elev` is a second truth about ground and this step inherits it.** It answers its own z13 bilinear
— 100.6 m where the engine draws 100.883 — and step 4's target answers "what stands at a place". Either
it is deleted or it becomes a thin caller of the same oracle. Name it in the step's "done when".

**The tile loader's C interface stops being one in the same step.** A dozen free functions over global
state with hand-written lifetime cannot express "one pool, two products", which is exactly the split this
step makes. An object owning the pool, with a simulation view and a picture view, can.

Done when: the server target builds and answers, without a device, what stands at a place, how big it is,
how deep the water is and where the sun is. A test `#include` of the renderer inside `generators/` is a
compile error.

## 4.5 — Fold the tile worker into the client

The saving is not a module and not a copy across a heap boundary — it is that **the tile scheduler exists
twice, in two languages**, with the same priority key written out by hand in both. The comment justifying
the separate module states a constraint that stopped being true when the worker began fetching for
itself.

Folding also erases three defects that live only in the browser half: byte caches that are **never
evicted**, four independent in-flight caps that know nothing of each other against one connection limit,
and a retry ceiling that turns a slow server into **permanently missing terrain**.

It is the execution environment the generators need, and the prerequisite audio needs anyway.

Done when: no scheduler in an embedded-JavaScript block remains, the terrain sources compile once, there
is exactly one in-flight cap, every thread is created at bring-up, and the moving-camera distribution is
no worse.

## 4.6 — The GPU readback stops blocking the frame thread

An unbounded wait on a GPU completion, on the thread that presents the picture, is the stall this project
forbids everywhere else. It does not hurt today only because it runs when a product is written — luck in
call frequency, not structure.

Callback-driven readbacks with a state machine around them. The prize is larger than the fix: it is the
only remaining reason for the stack-unwinding build mode, which instruments two thirds of all functions.

Own step, own binary — never folded into a concurrency change.

Done when: no unbounded wait remains on the frame thread, the unwinding mode is off, every declared run
still produces its product, and the distribution is no worse.

## 5 — `generators/`

Region, ground view, occupancy, draw, material row, schedule, generator, set, pool. Built inside the
server target, so the forbidden edge is impossible rather than prohibited.

## 6 — The forest becomes a generator

Renderer reference out, camera knowledge out, callback and `void*` become the ground view, mutable
counters move into the yield. **Growing a prototype is not a generator call** — it happens once per run at
bring-up. This is the cut most easily got wrong.

Behaviour-neutral: same picture, different call chain.

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

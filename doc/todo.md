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
| **1** | one scenario, scenes at places the owner knows well. Render, look, pull generators and shaders after it, loop until the quality stands. Ortho­graphic diversity matters here and nowhere earlier — a structural step only has to prove it did not move the picture, and one meadow shows that |
| **2** | the webcam scenario, several cameras, times of day and night, weather. Against those the whole lighting and weather model is fitted. **The rig deleted in 1.6 was wrong** — it rendered 320×180 and downscaled only the photograph; `architecture.md` asks for the declared size with **both** sides downscaled. It is in `git log` with its six fitted camera poses and gets rebuilt, not restored |

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

**When performance work happens is a trigger, not a schedule.** At 720p60 nothing is optimised. When
720p30 can no longer be **held** — the floor, p99 under 33 ms, not the mean — it is optimised back up to
720p60. The steps below that carry millisecond acceptance numbers are architecture and run in order
regardless; the trigger governs work that is *not* on this list.

## Carried from step 1

**A crossing still costs about +3 ms and lands near p95** — not invisible, and the residual is the 8 MB
class upload, one per publish. Streaming and ingest contribute nothing measurable, so pulling them off
the thread would buy nothing. It shrinks when the structure becomes per-region instead of one
camera-anchored grid, which is step 8.

**Why that structure cannot simply be appended to:** the grid is anchored on the camera and dense, so a
re-anchor changes what every index means; features are laid down in ascending rank, so a later one
rewrites cells an earlier one won; and a cell's seeds must be contiguous because the fragment reads them
as a range. Append-only needs an absolute region grid, non-contiguous seed lists in the fragment and rank
resolution at evaluation time — step 8's structure, not a variant of this one.

**The discard path is deliberately absent.** Every reader today runs on the render thread inside the
frame that asked, so there is no stale result to discard, and a path with no consumer is a dead path. The
version is published; it is the hook when a reader becomes asynchronous (step 5).

**Measured on the way past:** the scheduler always offers the fine grain first and could starve the
coarse one. Over the declared scene it does not, but that scene tests staleness and not drift — a run
past the coarse slack decides the drift case.

## Carried from step 1.5

**The client has N + 1 linear memories, and the ledger measures one of them.** Beside the main module
the tile pool runs `N = clamp(hardwareConcurrency − 2, 1, 6)` further wasm modules, growth-enabled;
N = 4 on the measuring host, 6 on an eight-core one. Reserved and in use are different quantities and a
`WebAssembly.Memory` never shrinks, so both belong in the account:

| | measuring host, N = 4 | eight-core, N = 6 |
|---|---|---|
| **reserved** — what the tab costs from instantiation | 256 + 256 = **512 MiB** | 256 + 384 = **640 MiB** |
| **in use** | main **97.8–108.6** measured · workers **unmeasured** | — |

**The gap between main's 256 reserved and its 108.6 used is 144 MiB, and it is the step's first move** —
the largest single item here, and the only one already fully measured. A fixed size taken from the main
module alone would be wrong by more than the size itself; a fixed size taken from what is reserved today
would be wrong by that gap.

The instrument cannot yet say it: the probe publishes the break, its peak and the toolchain ceiling, but
not the linear memory's current size. One call, one column.

**The byte budget is unserved where it is needed.** The tile byte caches read 1.3 MiB in the browser
against 33.8 MiB natively: both correct for what they can reach, but the browser's decoded tile bytes
sit in the worker modules, so the platform with the eviction problem is the one with no number.

**Stack, with the probe's range beside every figure** (peak · floor · limit · capacity, KiB). Browser:
frame **18.6** · 4.2 · 516 · 4096; class **4.2** · 4.2 · 516 · 4096 — the class figure *is* its floor, so
it reads "≤ 4.2, unresolved below". Native: frame **205.0** · 15.4 · 527 · 8176; tile **12.7** · 4.9 ·
517 · 524; class **4.5** = its floor. **The four browser tile-worker threads are not measured at all.**

**The device holds 234.3 MiB**, 208.9 of it tile geometry. No device *ceiling* is declared: which budget
a device allocation is charged against is the open question `architecture.md` names, and a number with no
derivation is worse than none.

**The frame-time argument for the fixed heap is struck.** It priced the threading change at p50
18.05 → 19.67 ms. Over 34 runs of 11 pinned builds that band is `gpuMs` — `frameMs` p50 against `gpuMs`
p50 is r = 0.98, and with that term out the band's residual is +0.08 ms against −0.07 ms for every other
run. Measured directly since, two builds of one source differing in nothing but the guard's 395 call
sites, eight counterbalanced blocks, per stage where the guard actually runs: `renderMs` **+0.104 ms**
(sd 0.155, p = 0.10), `worldMs` **+0.045** (p = 0.19) — **≈ 0.15 ms of CPU, an order of magnitude below
the struck figure.** `frameMs` p50 gives +0.50 at p = 0.19 and cannot resolve it: its sd of 1.00 is
`gpuMs` noise, worth power ≈ 0.35 at that population. The band was a host state that lasted half an hour.

## Carried from step 1.6

**A canvas costs frame time that no pass explains.** The step's third clause — two canvas sizes produce
the same frame distribution — was **wrongly specified and is struck**, twice over. By construction: the
present pass writes the swapchain, so its cost scales with the canvas and a correct implementation must
fail the clause. By measurement, worse: `demo/crossing`, 900 frames, 14 runs, p50 of per-second p50s
**18.297 ms at 640×360 against 19.606 at 1280×720** (Δ +1.309, se 0.284, Welch t = 4.61, p ≈ 0.002),
monotone in canvas pixels — but the excess sits in **every** pass including ones that cannot see the
canvas (`computeMs` +3.7 %, `shadowMs` +6.2 %, `sceneMs` +3.6 %), while the present pass carries 0.33 ms of
the 1.98. The renderer's work is provably identical across all 14 runs (`1280×720`, `meshStands` 96,
`impostorStands` 15 899, `treeTris` 533 856). So the canvas moves device or compositor state, not render
work. **What is checkable and is met: the declared size reaches the render target and the renderer's work
is invariant.** The frame-time part is not the step's to hold.

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

## 2 — The height oracle evaluates the drawn surface

The oracle interpolates the DEM a second time instead of evaluating the surface that is drawn, and
disagrees with it by up to metres. Anything placed on that height stands wrong, and no generator can
repair it.

Done when: oracle and drawn mesh agree to a stated bound, same posting indices, same triangle split.

## 3 — Move what is misfiled, delete what is dead

The value and algorithm headers with no renderer dependency belong in `core/`. The entity and effect path
in `world/` draws nothing and goes, and `units/` goes with it.

**The residue is a cone with two roots, not a web.** `units/Unit.h`, included only by `world/World.cpp`,
carries `core/Store.h` (713 lines of stores stations), `Countermeasure.h`, `NetReport.h`,
`WeaponUplink.h`, `VisualContact.h`, `Emitter.h`, `Flight.h`, `AvionicsBlocks.h`, `Mode.h`, `State.h`,
`UnitRegistry.h`. `render/Renderer.h` and `world/World.h` carry `UnitsStage`, `SpritesStage`,
`OverlayStage`, `UnitDraw`, `UnitModel` and with them `Glb.{h,cpp}`, the glTF loader nothing else uses.
**3252 lines**, and cutting two edges frees all of it.

**`verify-types` goes with the cone, not green.** It counts how much the engine knows about named
aircraft; once the cone is gone there is nothing to count, and a counter that always reads zero is
ceremony. Deleting it is the step's proof, not a shortcut around it.

**150 lines in `sim/src/` cite 27 `doc/` files that do not exist** — `doc/world/terrain.md`,
`doc/clients/clients.md`, `doc/core.md`, `doc/render/*`. A citation to a deleted document is a claim
nothing can check. They go with this step.

**The 16 species files are named in German** (`birke.json`, `saeulenpappel.json`). One language in the
repository, and it is English.

Mechanical, low risk, precondition for 4.

Done when: `world/` names `render/` only through the calls that drive it, and that count is published.

## 4 — The server target, and the checker falls

Split the object that owns world and renderer into a simulation half and a picture half. Add the target
that builds everything except `render/`. Invert the remaining renderer-driving calls in `world/` until it
links. **Delete the layer checker in the same commit** — two truths about the structure side by side is
the state that ruined the first one.

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
- **`World::WantSplit` applies a perspective pixel focal length to an orthographic scene.** For
  `demo/ortho` the true on-screen tile edge is `SpanM × Height/orthoM = SpanM × 0.419 px/m`; the ladder
  computes `SpanM × 443.4/2500 = SpanM × 0.177`, so it stops splitting at ≈2.4 × `kEdgeTau`. The picture
  is byte-identical today, so nothing is due — but the ladder measures a quantity the projection does not
  have, and under `orthoM > 0` the honest metric is distance-free.
- `GpuTimer::Pass::Cloud` is the enumeration's dead slot; `Tonemap` was filled this round.
- The near-field sward is a smeared wash with horizontal banding — no blades, no change of structure with
  distance. Second most damaging, same reason for waiting.
- German comments from earlier rounds. The history stays; what is touched gets translated as it is
  touched.
- **Naming needs a pass of its own.** A name that needs a comment is the wrong name. Borrowed jargon and
  magic sentinels where the type system has an answer are the two patterns. New identifiers are held to
  this as they are written; the existing ones are a separate sweep.

## Open, owner's decision

- A persistent server is declared in `vision.md` and not built. When it is, either the web host absorbs it
  or a third container appears — both touch a principle that says everything runs in the client.

# Todo

What is next, in order. A step names what must be true when it is done, not how — short enough to scan.
**A step that is done leaves this file**: it is then in the code, and its measurements are in `git log`.
What survives a finished step is only what a later step needs to know.

Order is not preference: 1 and 2 are preconditions for anything that places anything, and 4 is the gate
that makes 5 onwards enforceable rather than intended.

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

## 1.6 — The scenario declares the internal render resolution

**720p60 is the target and 1280×720 is the subject of the budget** — that has been the number all along.
What is missing is that anything actually says so: today the picture comes out at whatever size the output
medium happens to be, so no two machines measure the same thing and the resolution appears in no
telemetry line.

The scenario carries it, the canvas upscales bilinearly to its own size and keeps the aspect ratio with
bars rather than distortion, and the bench stops overriding it by flag — a size is part of the
declaration, not of the observation. A scenario that renders at another size, like a comparison against a
photograph, states a different subject and says why.

Done when: no code path takes a render size from a window, two runs at different canvas sizes produce the
same frame distribution **measured paired and counterbalanced** — the host drifts by more than a
millisecond between sessions, so an unpaired A/B decides on execution order — and the declared size
stands in every telemetry line beside the scenario.

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
- German comments from earlier rounds. The history stays; what is touched gets translated as it is
  touched.
- **Naming needs a pass of its own.** A name that needs a comment is the wrong name. Borrowed jargon and
  magic sentinels where the type system has an answer are the two patterns. New identifiers are held to
  this as they are written; the existing ones are a separate sweep.
- **A still is not reproducible run to run**, so byte-identity is not an acceptance instrument: over 14
  runs of `demo/frame` on three pinned builds, three md5 states, two of them the same picture to within
  one code and the third differing in **2 pixels above 2 codes, max 34, on 4 scanlines at one silhouette
  edge**. Small, bounded, and still pace deciding a result — principle 7. Compare within a tolerance
  until it is found; `tools/determinism.py` already computes the bound.

## Open, owner's decision

- A persistent server is declared in `vision.md` and not built. When it is, either the web host absorbs it
  or a third container appears — both touch a principle that says everything runs in the client.

# Todo

The next steps, in order. A step names what must be true when it is done, not how. Done means committed
and correct — there is no second place where correctness is claimed.

Order is not preference: 1 and 2 are preconditions for everything that places anything, and 4 is the gate
that makes 5 onwards enforceable rather than intended.

## 1 — Classification off the render thread

The grid is laid down on its own thread and arrives as an immutable published structure. **Not
copy-on-write:** a rebuild touches one of the two grids, the other is handed on by pointer, and the
outline arrays are LENT to the job by move and handed back with the result, so a rebuild copies
nothing. A reader takes one `shared_ptr<const>` and holds it; no non-`const` handle to a published
structure exists anywhere, so a half-swapped buffer is not expressible rather than unlikely. The
builder's stage runs one way — `Building` is left only by the worker, `Done` only by the render
thread — and the render thread schedules on its own bookkeeping, never on a queried state.

The discard half of the original design — a yield carrying a stale version is thrown away and
re-requested — is **deliberately not built**: every reader today runs on the render thread inside the
frame that asked, so there is no stale yield to discard, and a path with no consumer is a dead path.
The version is carried and published; it is the hook when a reader becomes asynchronous (step 5).

**Why the structure cannot simply be appended to:** the grid is anchored on the camera and dense
(`cell = j*W + i` against its origin), so a re-anchor changes what every index means; features are laid
down in ascending declared rank, so a later one rewrites cells an earlier one won; and a cell's seeds
must be contiguous because the fragment reads `seedFirst + s`. Append-only needs an absolute region
grid, non-contiguous seed lists in the fragment and rank resolution at evaluation time — a different
structure and a different shader, which is step 8.

### The crossing is not yet invisible

`gpuMs` drifts across a run (per-100-frame medians 12.8 · 20.1 · 16.3 · 16.8 · 16.9 · 15.9 · 13.9 ·
12.3 · 11.6), and the crossings do not sit evenly in that drift, so **a run-wide mean charges the drift
to the crossing.** The zero point below is therefore local: the median of the ±6 neighbouring frames
with every publish frame removed from the window. 4 runs per build, 20 publish events each,
`demo/crossing` (900 frames, 150 m/s, 2250 m), Chromium 151.0.7922.34.

| local excess at a publish, 20 events | before `19725c35051d7cd4` | after `837479557bc3cee6` |
|---|---|---|
| `frameMs` median · mean · max | **+9.16** · +13.04 · +41.91 | **+3.97** · +5.04 · +20.07 |
| `worldMs` median · mean · max | +9.02 · +11.72 · +28.91 | +1.51 · +2.70 · +12.08 |
| `classMs` median · mean · max | column did not exist | +1.56 · +1.76 · +4.92 |
| `gpuMs` median · mean · max | +1.35 · +1.52 · +12.98 | +1.90 · +2.13 · +15.02 |

**One of the twenty events is confounded and stays in the table above only to keep it comparable.**
Frame 377 carries a real building decode of 3.3–7.7 ms in all four runs, and it owns both maxima in the
after column. Strike it from both sides, 16 events: `frameMs` median **+3.22**, `worldMs` **+0.84**,
`classMs` **+0.87**. That last figure is the one to trust — it is the only one consistent with the
upload decomposition below (0.89 + 0.11 + 0.00), and it says a publish costs **under a millisecond of
class work**, not one and a half.

`classMs` is measured inside `worldMs`, not beside it: the two rows are containment, not addends.

**The `gpuMs` rise is not explained, it is undecidable at this population.** +1.35 before against +1.90
after is a permutation-test difference of +0.55 ms at **p = 0.42**; with the confounded event removed
the estimate moves the other way (+1.35 against +2.21, p = 0.087). Per-event spread runs from −3.4 to
+13.0. The honest statement is that the upload's device cost sits in both builds and this step did not
move it measurably — not that it was already there.

Pooled, same runs: `frameMs` p50 18.11 → 18.35, p95 30.66 → 31.09, p99 37.63 → 37.28; `worldMs` p99.5
7.91 → 3.36 and max 30.02 → 13.67. **`frameMs` max 64.84 → 45.20 is not a class result** — the worst
frame of either build is a several-frame regional disturbance that a publish happens to fall into.

**A publish still costs about +4 ms and lands near p95. That is the open number.** What it is made of,
and none of it is the grid:

| | |
|---|---|
| +1.90 ms | `gpuMs` — the device consuming the 8 MB class buffer written that frame. It was **+1.35 ms before**, so this is the upload's device cost and always was |
| +1.56 ms | `classMs` — the render thread's own share, dominated by the `WriteBuffer` call |
| — | `bDecodeMs` is **not** a term. At exactly one of five publish events per run (frame 377 in all four runs) a real building decode of 3.3–7.7 ms coincides and dominates that frame by itself; the other four carry none |

Inside `classMs`, over the 29 uploads that write the **full 8 MB** buffer — the ones a crossing during
play produces, as against all 78 uploads of a run, most of which are the small buffers of the initial
load: `WriteBuffer` mean **0.89 ms** (median 0.58, max 3.79), `OsmField::Build` mean **0.11 ms**,
`ClassField::Ingest` mean **0.00 ms**. Over all 78 the same numbers are 0.48 / 1.40 / 0.22, and the
1.40 is tile fetching during the load, not a crossing. **The residual is the upload, not the
streaming** — pulling streaming or ingest off the thread would buy nothing.

`classMs` pooled over the four pinned runs: p50 0.01 · p95 0.02 · p99 0.32 · **p99.5 1.08** · max 4.93.
The jump between p99 and p99.5 is the finding: **36 of 3600 frames carry the whole distribution.**

Left to close it: the 8 MB write is one per publish and the only per-region cost with nowhere to go but
the queue thread. It shrinks when the structure becomes per-region instead of one camera-anchored grid
(step 8). Until then the honest statement is p95, not invisible.

**Measured on the way past, so step 8 does not have to guess:** the scheduler always offers the fine
grain first and returns on the first due one, which could starve the coarse one. Over 2250 m it does
not — 12–14 fine submits against 6–7 coarse per run. That is below `Coarse.SlackM` = 3800 m, so it
tests staleness and not drift; a run past 4 km is what decides the drift case.

## 1.5 — Heap and stack telemetry

There is none, and its absence has already been used as a reason not to decide something. That is the
wrong way round: a missing measurement is a task, not a constraint.

Heap size and its high-water mark per second into the telemetry; per-thread stack high-water from the
stack base against the current pointer. Roughly twenty-five lines together, and they unblock two numbers
that are otherwise set rather than measured — the memory budget and the per-purpose stack sizes.

**Every pool reports its bytes, or it is a leak with a name.** One structure already does; the caches and
the class grids do not, and the largest resident item on the device side has no budget at all.

The fixed heap `architecture.md` requires is blocked on exactly this measurement, and it is not free to
wait for: with `-pthread` and a growing heap the generated glue guards every heap access (395 call sites
in `web/gpu.js`), which costs `frameMs` p50 **18.05 → 19.67 ms** over 3 × 900 frames. Correct, and paid
every frame. The toolchain setting that would delete the guard cannot be used — see `architecture.md`,
*The machine* — so the budget is the only route.

Done when: a full moving-camera run publishes heap high-water against the declared ceiling, and every
thread publishes how much stack it actually used.

## 2 — The height oracle evaluates the drawn surface

The oracle interpolates the DEM a second time instead of evaluating the surface that is drawn, and
disagrees with it by up to metres. Anything placed on that height stands wrong, and no generator can
repair it.

Done when: oracle and drawn mesh agree to a stated bound, same posting indices, same triangle split.

## 3 — Move what is misfiled, delete what is dead

`Json.h`, `ClusterDag.h`, `SpriteDraw.h`, `ChunkMesh.h`, `Mips.h` are value and algorithm headers with no
renderer dependency and belong in `core/`. The entity and effect path in `world/` draws nothing and goes,
and `units/` goes with it.

Mechanical, low risk, precondition for 4.

Done when: `world/` names `render/` only through the calls that drive it, and that count is published.

## 4 — The server target, and the checker falls

Split the object that owns world and renderer into a simulation half and a picture half. Add the target
that builds everything except `render/`. Invert the remaining renderer-driving calls in `world/` until it
links. **Delete the layer checker in the same commit** — two truths about the structure side by side is
the state that ruined the first one.

**The tile loader's C interface stops being one in the same step.** It is a dozen free functions over
global state with hand-written lifetime, in a place that is not the language island the rules allow — and
a free function list cannot express "one pool, two products", which is exactly the split this step makes.
An object owning the pool, with a simulation view and a picture view, can.

Done when: the server target builds and answers, without a device, what stands at a place, how big it is,
how deep the water is and where the sun is. A test `#include` of the renderer inside `generators/` is a
compile error.

## 4.5 — Fold the tile worker into the client

**Decided by the architect, and the reason is not the one this list first gave.** The saving is not a
module and not a copy across a heap boundary — it is that **the tile scheduler exists twice, in two
languages**: as JavaScript embedded in a C++ file for the browser, and as C++ for the native path, with
the same priority key written out by hand in both. The comment justifying the separate module states a
constraint that stopped being true when the worker began fetching for itself.

Folding also erases three defects that live only in the browser half: byte caches that are **never
evicted**, four independent in-flight caps that know nothing of each other against one connection limit,
and a retry ceiling that turns a slow server into **permanently missing terrain** — which already
violates the rule that the load has no timeout.

It is the execution environment the generators need, and the prerequisite audio needs anyway.

Done when: no scheduler in an embedded-JavaScript block remains, the terrain sources compile once, there
is exactly one in-flight cap, every thread is created at bring-up, and a moving-camera frame distribution
is no worse than before.

**Not in the same round:** the unwinding mechanism. It hangs on GPU readbacks on the main thread, not on
the network, so folding does not remove it — and two changes in one round cannot be attributed.

## 4.6 — The GPU readback stops blocking the frame thread

An unbounded wait on a GPU completion, on the thread that presents the picture, is the stall this project
forbids everywhere else. It does not hurt today only because it runs when a product is written — that is
luck in call frequency, not structure.

Callback-driven readbacks with a state machine around them. The prize is larger than the fix: it is the
only remaining reason for the stack-unwinding build mode, which instruments two thirds of all functions.
With it gone, that mode goes.

Own step, own binary — never folded into a concurrency change, or a regression cannot be attributed.

Done when: no unbounded wait remains on the frame thread, the unwinding mode is off, every declared run
still produces its product, and the moving-camera distribution is no worse.

## 5 — `generators/`

Region, ground view, occupancy, draw, material row, schedule, generator, set, pool. Built inside the
server target, so the forbidden edge is impossible rather than prohibited.

## 6 — The forest becomes a generator

Renderer reference out, camera knowledge out (the eye-clearance rule belongs to the consumer), callback
and `void*` become the ground view, mutable counters move into the yield. **Growing a prototype is not a
generator call** — it happens once per run at bring-up. This is the cut most easily got wrong.

Behaviour-neutral: same picture, different call chain.

## 7 — One geometry stage

Tiles, trees, buildings and water merge into one stage over one cluster cut. The renderer loses every
field naming a part of a plant. Count and publish the pipelines.

Done when: the pass count is unchanged and the scene pass stays flat within noise.

## 8 — Regionalise

Ring of regions around the viewer, request / collect / release / cancel, generation off the render
thread. **Measure milliseconds and bytes per region** — that number does not exist today.

Done when: a region crossing is invisible in the frame distribution at the highest declared speed, over
repeats, and popping is judged from a moving capture rather than a still.

## 9 — Buildings, water surface, infrastructure

Footprints and the water surface become generators; the water *level* stays in the core. Then
infrastructure.

## Later

- GPU emitter for scattering, with the C++ generator as its oracle. **Not before 8** — without the
  per-region number every move is guessed.
- Split the tile loader: the cache and height half is server-side, the mesh and DAG half is picture-side.
  Fat, not a blocker.
- **Replace the unwinding mechanism.** It is instrumented into two thirds of all functions, and it exists
  only for GPU readbacks waiting on the main thread. Either a newer browser primitive with the same code
  size, or callback-driven readbacks that remove the need entirely. The second is the end state. Measure
  one against the other with pinned builds — never in the same round as a concurrency change.
- **Heap telemetry.** There is none. Until it exists, no statement about the memory budget has provenance.
- **The wasm link's optimisation level is an artefact, not a decision.** Everything else in the tree
  builds at the higher level; the browser link and one translation unit inside it do not, and no comment
  or measurement says why — it rode in with a pivot commit. Measure the levels against build time, module
  size and the moving-camera distribution, then set it deliberately. A development build may differ from
  what ships, but then it says so.

## Small things that become traps if they wait

- The tone-mapping slot in the pass enumeration is empty since the fold. A dead slot is where a new pass
  hides without the count moving.
- The comment on the vegetation row claims a size the structure no longer has. It is uploaded verbatim
  and its field meanings are pinned against the shader.
- `scenarios/` is the decided name; the tree still says `mods/`.
- Comment density in the tree is far above the rule, and the worst file is more than half prose.
- German comments and German commit messages from earlier rounds. The repository speaks English; the
  history stays as it is, everything touched from here on gets translated as it is touched.
- **Naming needs a pass of its own.** A name that needs a comment to be understood is the wrong name —
  the comment is the evidence, not the fix. Borrowed jargon that says nothing about the thing, and magic
  sentinel values where the type system has an answer, are the two patterns to look for. A new identifier
  is held to this the moment it is written; the existing ones are a separate sweep.

## Open, owner's decision

- A persistent server is declared in `vision.md` and not built. When it is, either the web host absorbs
  it or a third container appears — both touch a principle that says everything runs in the client.

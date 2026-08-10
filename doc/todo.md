# Todo

The next steps, in order. A step names what must be true when it is done, not how. Done means committed
and correct — there is no second place where correctness is claimed.

Order is not preference: 1 and 2 are preconditions for everything that places anything, and 4 is the gate
that makes 5 onwards enforceable rather than intended.

## 1 — Classification off the render thread

The grid is laid down on its own thread and arrives as an immutable published structure. **Not
copy-on-write:** a rebuild touches one of the two grids, the other is handed on by pointer, and the
outline arrays are LENT to the job by move and handed back with the result, so a rebuild copies
nothing. A reader takes one `shared_ptr<const>` and holds it; there is no non-`const` handle to a
published structure anywhere, so a half-swapped buffer is not expressible rather than unlikely.

The discard half of the original design — a yield carrying a stale version is thrown away and
re-requested — is **deliberately not built**: every reader today runs on the render thread inside the
frame that asked, so there is no stale yield to discard, and a path with no consumer is a dead path.
The version is carried and published; it is the hook when a reader becomes asynchronous (step 5).

**The crossing is not yet invisible, and the number is this:** over 2 x 4 runs of `demo/crossing`
(900 frames, 150 m/s, five re-anchors), the crossing frames average **+7.3 ms** over the other frames
and land around the **p95** of their own run; in 2 of 4 runs one of them is still the worst frame.
`worldMs` max fell from 30.02 to 16.48 ms pooled, `frameMs` max from 64.84 to 53.16 ms, and the class
stage left on the render thread is `classMs` p99 0.42 ms, max 6.48 ms.

What the remaining +7.3 ms is, measured, and none of it is the grid:

| | |
|---|---|
| +4.5 ms | `gpuMs` — the device consuming the 8 MB class buffer written that frame |
| +1.7 ms | `classMs` — of which the `WriteBuffer` call is 1.04 ms mean (max 3.51); `OsmField::Build` is 0.10 ms and `Ingest` is 0.00 ms, so **the residual is the upload, not the streaming** |
| +1.6 ms | `bDecodeMs` — the OSM building decode, a different subsystem, coinciding at four of the eight crossing frames |

Left to close it: the upload is one 8 MB write per publish and the only per-region cost that has no
place to go but the queue thread. It shrinks when the structure becomes per-region rather than one
camera-anchored grid (step 8) — until then the honest statement is p95, not invisible.

**Why the structure cannot simply be appended to:** the grid is anchored on the camera and dense
(`cell = j*W + i` against its origin), so a re-anchor changes what every index means; features are laid
down in ascending declared rank, so a later one rewrites cells an earlier one won; and a cell's seeds
must be contiguous because the fragment reads `seedFirst + s`. Append-only needs an absolute region
grid, non-contiguous seed lists in the fragment and rank resolution at evaluation time — a different
structure and a different shader, which is step 8.

## 1.5 — Heap and stack telemetry

There is none, and its absence has already been used as a reason not to decide something. That is the
wrong way round: a missing measurement is a task, not a constraint.

Heap size and its high-water mark per second into the telemetry; per-thread stack high-water from the
stack base against the current pointer. Roughly twenty-five lines together, and they unblock two numbers
that are otherwise set rather than measured — the memory budget and the per-purpose stack sizes.

**The fixed heap is what pays for the threads, and it has a price today.** With `-pthread` and a
growing heap, emscripten guards every heap access in its glue (395 call sites in `web/gpu.js`) because a
growth on one thread leaves another's typed view over the shorter buffer. That guard costs `frameMs`
p50 **18.05 -> 19.67 ms** over 3 x 900 frames. It is a speed cost, not a safety one — the guard is
correct and C++ holds no view at all.

**`-sGROWABLE_ARRAYBUFFERS=2` cannot remove it, and the blocker is named:** with the heap as a
resizable buffer the client throws on the main thread at the first render pass —
`Failed to execute 'setBindGroup' on 'GPURenderPassEncoder': The provided ArrayBuffer value must not be
resizable` — before any growth has happened (zero growth events in the runtime-debug log). WebGPU
forbids a resizable ArrayBuffer as an argument source, so no emscripten setting reaches it. The only
route to dropping the guard is the fixed heap, and the budget for it is what this step measures.

**Every pool reports its bytes, or it is a leak with a name.** One structure already does; the caches and
the class blocks do not, and the largest resident item on the device side has no budget at all.

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

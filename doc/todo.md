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

## 1.5 — Heap and stack telemetry

There is none, and its absence has already been used as a reason not to decide something. A missing
measurement is a task, not a constraint.

Heap size and its high-water mark into the telemetry; per-thread stack high-water. **Every pool reports
its bytes, or it is a leak with a name** — one structure already does, the caches and the grids do not,
and the largest resident item on the device side has no budget at all.

The fixed heap `architecture.md` requires is blocked on exactly this, and one number that has been used
to argue its urgency is **in doubt**: the threading change was priced at a frame-time cost paid every
frame, but the shipped build carrying every guard site is indistinguishable from the state before it, and
the expensive band that did exist went away without the guard going away. Measure what that band was, or
strike the claim.

Done when: a full moving-camera run publishes heap high-water against a declared ceiling, and every
thread publishes how much stack it used.

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
- German comments from earlier rounds. The history stays; what is touched gets translated as it is
  touched.
- **Naming needs a pass of its own.** A name that needs a comment is the wrong name. Borrowed jargon and
  magic sentinels where the type system has an answer are the two patterns. New identifiers are held to
  this as they are written; the existing ones are a separate sweep.

## Open, owner's decision

- A persistent server is declared in `vision.md` and not built. When it is, either the web host absorbs it
  or a third container appears — both touch a principle that says everything runs in the client.

# Todo

The next steps, in order. A step names what must be true when it is done, not how. Done means committed
and correct — there is no second place where correctness is claimed.

Order is not preference: 1 and 2 are preconditions for everything that places anything, and 4 is the gate
that makes 5 onwards enforceable rather than intended.

## 1 — Classification off the render thread

Building the class structure runs on the render thread and costs several frames' worth of time at a
region's first arrival. Today it hides behind the loading bar; the moment a viewer crosses a region
boundary it is the forbidden hitch.

**Riskiest step, therefore first.** The risk is not the thread — it is that packing swaps its buffer
while a worker reads. Needs a versioned, copy-on-write class structure: a reader holds the version it
read, and a yield carrying a stale version is discarded and re-requested.

Done when: a region crossing is invisible in the frame distribution over a moving camera, measured with
repeats, and a worker can never observe a half-swapped buffer.

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

## Small things that become traps if they wait

- The tone-mapping slot in the pass enumeration is empty since the fold. A dead slot is where a new pass
  hides without the count moving.
- The comment on the vegetation row claims a size the structure no longer has. It is uploaded verbatim
  and its field meanings are pinned against the shader.
- `scenarios/` is the decided name; the tree still says `mods/`.
- Comment density in the tree is far above the rule, and the worst file is more than half prose.

## Open, owner's decision

- A persistent server is declared in `vision.md` and not built. When it is, either the web host absorbs
  it or a third container appears — both touch a principle that says everything runs in the client.

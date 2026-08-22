Type: feature
Area: render
Tags: perf, scope

**The engine draws a CITY, and a subject is its degenerate case**

**The owner's ruling, and it is the honest scale:**

> *This is the MINIMAL goal. The final goal is thousands of other traffic participants, aircraft in the
> sky, clouds and vegetation. Please be realistic. Our target is a game engine at RAGE and Unreal
> level.*

**Being realistic starts with measuring what is there, and three measurements decide this item.**

| What | Where | What it says |
|---|---|---|
| the geometry draw | `src/render/stages/SubjectDraw.cpp:1626` | `SDL_DrawGPUIndexedPrimitives(pass, batch.IndexCount, **1**, ...)` -- the instance count is a **literal 1** |
| the overlay draw | `src/render/stages/OverlayDraw.cpp:265` | `SDL_DrawGPUPrimitives(pass, 6, **Count**, 0, 0)` -- **the engine already instances**, on the other path |
| the field for it | `src/render/draw/DrawList.h:155` and `SubjectDraw.cpp:1540` | `DrawBatch::Draws` **exists** and is summed as a **statistic only**. Declared, never delivered |

**And there is no frustum cull of draw batches.** `World::TargetInViewN` and the `inView` out-parameter
count **tiles for streaming residency**, not visible geometry -- so the one thing that reads like a cull
is a different instrument. The compositor row of `CLAUDE.md` says a compositor *places, **culls**,
quantises the budget, **batches***. Two of those four are architecture that is written down and not
built.

## Why these two are a PRECONDITION and not an optimisation

A thousand cars with no instancing is at least a thousand draws, and a draw is not free. **The honest
statement is that the per-draw cost on THIS device is not yet measured** -- so the first task here is
the instrument, not the fix. `test/render/outshine/frame/` already publishes a frame cost against its
own floor; what it has never been asked is *what does the N+1th draw cost*.

That measurement decides everything downstream: it converts *thousands of entities* from an adjective
into a draw budget, and a draw budget is what says whether instancing alone is enough or whether the
batches must also be merged.

## What must be true

- [ ] **The per-draw cost on this device is measured and published** -- a slope in microseconds per
      draw over a swept draw count, p50/p95/p99, and the draw count at which submission alone spends
      16.67 ms
- [ ] **The geometry path instances**: one batch, N transforms, one draw. `DrawBatch::Draws` stops
      being a statistic and becomes the argument it was named for
- [ ] **The compositor culls before it batches**, against the frustum it was given -- cost answerable
      before the part is made, which is already the rule
- [ ] **A scale case exists and is red before it is green**: thousands of entities, millions of
      triangles, at 720p60, over a moving camera -- because *a case that exists and is red says more
      than a case that does not exist*

## Comments

**What was measured and stays true**: the road drawn at 1280x720 with a worst frame of 3.14 ms. That
number was taken on ONE swept corridor, and quoting it as evidence about a city would be exactly the
defect `CLAUDE.md` names -- *a changed selection reads as the same metric and looks like progress*. It
is evidence that the path works, and it is not evidence about scale.

**`OverlayDraw` already instancing is the good news in this item.** The mechanism is not missing from
the engine, the device or SDL_GPU; it is missing from one call site whose third argument is a literal.
That is a far smaller distance than *the engine cannot draw a city* would suggest.

---

**Owner ruling (2026-08-22): instancing is automatic, always.** The client never asks for it
and cannot ask for it: when meshes are identical, outshine instances -- identity is the content
currency the tree already owns (the store's hash, the part key (kind, params, seed, rung), the
subject's digest), so the draw path groups placements by mesh identity and issues one draw with
a real instance count. There is no instancing API at any door; a client that draws the same
mesh a thousand times has declared a thousand placements and nothing else. This closes the
question of WHO instances; what remains of this item is the mechanism -- the entity store's
placement grouping, the per-instance stream, and the cull that feeds it.

---

**Learned from the reference study (2026-08-22; UE MeshDrawingPipeline, ACU SIGGRAPH 2015,
Unity SRP/BRG, Metal WWDC19/22, Decima placement, Persson/Wihlidal).** The ruling in one
shipped sentence (ACU): hash over the non-instanced data, merge by the hash, keep instance data
persistent beside it. Mapped here:

1. **Identity is already ours.** UE must hash RHI state because identity only exists there; our
   content key (kind, params, seed, rung) / ModelSlot IS mesh identity before the renderer.
   Group key = (SurfaceKind, VertexLayout, MaterialSlot, ModelSlot) -- the fields SameState
   already compares.
2. **The opaque sort key must lift material+model above depth** (Ericson: opaque
   material-major, blended depth-major). Today's key is depth-major for everything, so two
   identical cars at different depths never sort adjacent and no grouping can find them.
   Blended keeps depth-major and DOES NOT instance -- instancing across depth steps breaks the
   blend order.
3. **The instance stream is {placement, tint}.** Tint as instance data keeps the
   five-colour-F31 in ONE batch (UE: PrimitiveId->GPU-Scene; Unity: instanced properties;
   the material-property road breaks batches). Everything touching pipeline state stays batch
   identity; everything per-placement goes in the stream.
4. **Grouping happens at insert, never per frame** (UE caches at AddToScene; ACU keeps
   instance data persistent) -- DrawList::Add knows the group key already; Compile sorts
   groups and writes the stream compactly. Deterministic by value-key, stable tie-break as
   today.
5. **The platform boundary is respected**: SDL3_GPU carries instanced draws
   (num_instances/first_instance + instance-rate stream) -- Apple's own base building block --
   but neither ICBs nor MultiDrawIndirect, so GPU-driven culling/cluster expansion stays out
   of scope by construction; and merge-instancing (Persson) only matters when measured draws
   are wavefront-starved, where the answer is clustering, not drawcall cosmetics.

RAGE remains unreconstructable from primary sources (low confidence, noted); Decima is the
purest shipped form of the ruling -- nobody calls instance(), rules and densities produce the
batches.

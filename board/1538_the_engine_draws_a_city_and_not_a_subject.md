Type: feature
State: open
Area: render
Tags: perf, scope, compositor
Supersedes: 1595

# The compositor places, culls, quantises and batches — and the ground layer spells no camera

`CLAUDE.md` gives a compositor four verbs and the tree builds two. Measured at HEAD:

| what | where | what it says |
|---|---|---|
| the geometry draw | src/render/stages/SubjectDraw.cpp | the instance count is a literal `1` |
| the overlay draw | src/render/stages/OverlayDraw.cpp | `SDL_DrawGPUPrimitives(pass, 6, Count, ...)` — the engine already instances, on the other path |
| the field for it | src/render/draw/DrawList.h | `DrawBatch::Draws` exists and is summed as a STATISTIC only |
| the cull | nowhere | `World::TargetInViewN` counts tiles for streaming residency, not visible geometry |
| the selector | src/ground/World.h | `struct Eye` (:49), `Refine(const Eye &eye, double nowMs)` (:55) — the ground layer chooses LOD per eye, which the layer table forbids |
| the frustum | src/core/Camera.h | `struct Frustum` (:94) sits in core beside the camera and has no consumer |

A thousand cars with no instancing is at least a thousand draws; a city with no cull draws the
half of the world behind the camera. Both are architecture written down and not built, and the
selection that IS built sits one layer too low.

## What will be true

- [ ] A draw list is culled against the declared view before it is encoded, and the cull's
      counts (submitted, culled, drawn) are published per frame.
- [ ] Batches instance: one draw per (mesh, material, layout) with N placements, `Draws`
      delivered rather than counted.
- [ ] `World::Refine` answers a budget the COMPOSITOR set — projected error in px on the one
      quantised ladder — and spells no eye, no camera, no frustum. `Frustum` moves to the
      compositor with its first consumer.
- [ ] Per-draw cost on this device is MEASURED before and after, in the frame suite (board:1457).
- [ ] Proving test: a scene of N identical subjects encodes O(1) draws and the picture is
      unchanged; negative control — the cull disabled, the count rises and the picture does not.

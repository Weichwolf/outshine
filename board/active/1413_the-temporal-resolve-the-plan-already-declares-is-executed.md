Type: feature
Area: render
Tags: perf, instrument

**The temporal resolve the plan already declares is executed**

Every pipe is laid and the stage is not built. `RenderCatalogue.h` carries the row --
`temporalResolve`, reading `SceneComposited`, `SceneVelocity`, `SceneDepth` and writing `SceneLinear`
-- `RenderPlan` adds `kTemporalSettleFrames` when it is held, `SceneLinear` **`FallsBackTo`**
`SceneHdr` when it is not, and the velocity target is written by every subject arm already.

**And `Renderer.cpp` answers it in three places with `this device layer does not execute the stage`,
while `jitter` appears nowhere in `src/` outside the vegetation tables.** So the declaration is real
and the picture it describes has never been made.

## Why it comes before the visibility buffer, and it is not a preference

| | this | `board:1412` |
|---|---|---|
| provenance | **`Content`** -- a case declines it and the alias carries the picture | machinery under the subject unit |
| effect on the 148 corpus cases | **none**, because they declare no temporal stage | every case re-measured |
| gain at 720p on this device | large and immediately visible | small until triangles approach pixel size |
| what already exists | velocity, settle frames, the alias, the stage row | nothing |

**And there is an ordering dependency in the other direction**: `board:1412`'s resolve recomputes
barycentrics from PROJECTED triangle corners, and a temporal jitter moves the projection by a sub-pixel
offset every frame. Built the other way round, the visibility buffer would have to take the jitter in
afterwards.

**At a third of a PS4's bandwidth and more compute, this is the shape the device asks for**: it buys
picture quality with arithmetic and a history read rather than with resolution -- and it is the
precondition for a dynamic resolution that holds a hard frame floor, which is the id Tech 7 line
`CLAUDE.md` already cites.

## What must be true

- [ ] **The jitter is declared and deterministic**, a named sequence with a named length and amplitude
  -- *the mathematics is deterministic*, so two runs of one scenario produce one picture
- [ ] **The history belongs to a frame and is NOT a plan edge.** A read of the previous frame's output
  is a cycle in a per-frame DAG; the renderer owns the history texture across frames and
  `SettleFrames_` already exists for exactly this
- [ ] **The velocity the reprojection uses is the one the geometry pass writes**, so the two cannot
  disagree about what moved
- [ ] **The neighbourhood clamp is stated once** and its colour space is named -- history that is not
  clipped towards the current frame is ghosting, and clipping in the wrong space is a colour shift
- [ ] **A plan that does not declare it pays nothing**: no blit, no history allocation, no settle
  frames. The alias already promises this and the promise is checked
- [ ] **The frame cost is published before and after** by the suite that already runs four arms over a
  moving camera
- [ ] **The corpus does not move.** No case declares this stage, so a case that changes is a case that
  was reading something it should not have

## What it will NOT do in this round

**No dynamic resolution.** That is what this unlocks and it is a separate item with its own
measurement; landing both at once would leave neither attributable.

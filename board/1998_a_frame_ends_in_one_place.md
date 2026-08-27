Type: feature
State: active
Parent: 1953
Area: render
Tags: gpu-driven, determinism

# a frame ends in ONE place, and every render path passes through it

**Benchmark** — Unreal: `FSceneRenderer` ends a frame at a single point; `FScene::Update` carries
the GPU scene's per-instance PREVIOUS transform across it, and `FViewInfo` holds the previous
view matrices beside the current ones. RAGE: the render thread's frame ends with a buffer flip
that advances every double-buffered value at once. **Both agree, and neither leaves the question
to the caller.** Taking that.

Here, "previous" is decided in three unrelated places and two of them answer with the CURRENT
value:

| what | where | what it says |
|---|---|---|
| previous view | `Renderer::RenderFrame` end | the real previous camera |
| previous anchor | `SubjectProxy.cpp`, both sites | `PrevAnchor = Anchor` -- CORRECT, see below |
| previous placement | `SubjectDraw::Encode` | `before[i] = model[i]` -- REPAIRED, see below |

**MEASURED, AND TWO THIRDS OF THE TABLE ABOVE WAS WRONG.** `Renderer::RenderFrame` has exactly
ONE caller, so a frame already ends in one place; the item was filed on a guess that it does not.

And `PrevAnchor = Anchor` is CORRECT, not a defect. Carrying the anchor properly took
`khronos/glTF/AnimatedCube` from PASS to `velocity_pixels_moving = 97468` against a bound of 0 --
97468 px is exactly that case's covered area, so every covered pixel reported motion. The reason:
`PrevVerts` are written into the scratch buffer against the CURRENT anchor, so previous positions
live in today's anchor space and pairing them with yesterday's anchor moves the whole subject.
The field is deleted rather than kept, because it can never differ from `Anchor`.

What was real is the third row. `before[i] = model[i]` in `Encode` meant a rigid subject that
changed PLACE reprojected onto itself, and that is now fixed: the placement row is 32 floats --
current transform then previous -- carried across by `MovePlacement` under a per-slot frame stamp,
with a first-write rule so a slot's first previous is its own current rather than the zeros
`PlacementRows` fills.

**The consequence today**: this tree has no rigid motion vector. A subject that changes place
between frames writes zero velocity, and TAA has nothing to reproject it by. It is invisible
because nothing in the corpus moves a rigid subject without also deforming it.

## What will be true

- [x] board:1989's 32-float placement row lands: current transform then previous, with a per-slot
      frame stamp so two moves in one frame cannot lose the old value, and a first-write rule so a
      slot's first previous is its own current rather than the zeros `PlacementRows` fills.
      `SubjectDraw::CarryFrame()` advances the stamp at the end of `Renderer::RenderFrame`.
      proof: khronos/glTF, outshine/door
- [ ] **A rigid subject that moves between frames writes a non-zero velocity, and the door can
      READ that.** Today it cannot, and the blocker has an address: `Renderer::ReadSceneVelocity`
      exists and `harness/shared/render/Parity.cpp` is its only caller, reaching past `include/`.
      A door case cannot. Declaring `temporalResolve` and `sceneVelocity` into
      `apps/driver/src/f31.scenario`'s plan from a case is ACCEPTED and still leaves `VelTex_`
      null, so the resource is aliased away before it is made -- that is the thing to understand
      before the number is published, because publishing one that reads -1 forever is worse than
      publishing none.
      **MEASURED, not inferred**: with `was = now` forced in `HandPlacements`, khronos/glTF is
      444/444 and `outshine/door/ScoreWhatAMovingSceneResends` reads the same 57600 px and the
      same 0.695012 ndc. NOTHING in this tree reads the previous placement row. The reason is
      structural rather than accidental: `harness/shared/render/Parity.cpp` calls `PoseGeometry`
      and hands the engine BAKED vertices with `prevP`, so even a node-rotation case like
      `AnimatedCube` reaches the renderer as a static transform over moving vertices. The corpus
      cannot exercise a placement's previous transform by construction.
      The row is kept anyway, and the reason is stated rather than assumed: the alternative is
      `before[i] = model[i]`, which is provably wrong -- a rigid subject that changes place
      reprojects onto itself and TAA fetches history from where the subject is NOT. Correctness
      no case covers is still correctness; what is missing is the case, and it has an address:
      a scenario with a FIXED eye over a subject whose PLACEMENT moves, declared through the door
      rather than baked by the harness. Then `was = now` drops the moving-pixel count to zero
      while the picture stays put, and this predicate closes.
- [ ] a claim counts the callers of the frame's end and refuses when there is more than one, so
      the single caller `RenderFrame` has today is a fact rather than a coincidence

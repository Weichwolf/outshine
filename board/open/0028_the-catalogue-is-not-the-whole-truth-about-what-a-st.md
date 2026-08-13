Type: bug
Area: render
Tags: scope

**The catalogue is not the whole truth about what a stage touches, so the assertions prove less than they read as proving**

`src/render/plan/RenderCatalogue.h`, `src/render/plan/RenderPlan.cpp:36-42`,
`src/render/Renderer.cpp:324-376`. The six `static_assert`s are the strongest thing in this design and
two holes let an edge past them.

- **`Pull::Hold` pulls a stage's `Reads` and its `Contributes` and never its `Writes`**
  (`RenderPlan.cpp:40-41`). A held stage's `Derived` output is therefore marked held only if some other
  held stage happens to read it, and `Renderer::OnDevice` creates only what `Holds()` reports. Every
  stage in today's catalogue writes at most one resource and that resource is always read, so nothing is
  wrong on the screen — **and nothing enforces either half of that**. A stage with two outputs, or one
  whose output only leaves through a readback, silently gets an uncreated resource and a null binding.
  Right: `Hold` wants what the stage writes, which also makes the resource appear in the digest.
- **`Configure(Stage::TemporalResolve)` binds two resources the row does not declare** —
  `View(Resource::AoBuffer)` and `MeterBuf` (`Renderer.cpp:362-366`), while the row's `Reads` are
  `{SceneHdr, SceneVelocity, SceneDepth, LinearSampler, AtmosphereUniform}`
  (`RenderCatalogue.h:219-222`). It is correct today because R2 fuses the resolve with the tonemap and
  the tonemap declares both, and because the bindings are guarded by `display.HasOcclusion`
  (`stages/TaaStage.cpp:287`). But `TopologicalOrderHolds` only constrains what a row **declares**: the
  ordering that keeps `Occlusion` before the resolve is carried by `Tonemap`'s row, and if the fusion is
  ever unwound or re-aimed the compile-time proof quietly stops covering the real read set. Right: the
  fused pair's read set is a union the compiler computes, and `Configure` receives the pass's resources
  rather than a hand-picked list per stage.

**Fixed when** a stage cannot be handed a resource its row does not name — the shape, not a review rule:
`Configure` takes the plan's resolved bindings for that stage, so an undeclared one has no spelling.

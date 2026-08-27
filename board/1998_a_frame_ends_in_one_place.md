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
| previous anchor | `SubjectProxy.cpp`, both sites | `PrevAnchor = Anchor` -- the current one |
| previous placement | `SubjectDraw::Encode` | `before[i] = model[i]` -- the current one |

**MEASURED, and this is the item's whole point.** Building the previous placement without this
put `SubjectDraw::CarryFrame()` at the end of `Renderer::RenderFrame`, which is where the camera's
previous value is already carried. The corpus said that point is not reached on the path the
cases take: `khronos/glTF/AnimatedCube` went from PASS to `velocity_pixels_moving = 97468` against
a bound of 0, and 97468 px is exactly that case's covered area -- every covered pixel reporting
motion is what a previous transform frozen at its first move looks like. 62 more animated cases
went with it. So the camera's carry point is NOT the frame's end; it is one path's end.

**The consequence today**: this tree has no rigid motion vector. A subject that changes place
between frames writes zero velocity, and TAA has nothing to reproject it by. It is invisible
because nothing in the corpus moves a rigid subject without also deforming it.

## What will be true

- [ ] **One method ends a frame** and every path that renders -- windowed, offscreen, headless
      picture-taking -- passes through it exactly once. Named rather than assumed: a claim counts
      the callers and refuses when there is more than one.
- [ ] The previous view, the previous anchor and the previous placement are all carried THERE,
      and nowhere else assigns a previous value.
- [ ] A rigid subject that moves between frames writes a non-zero velocity, and one that does not
      writes zero. The negative control is the case that found this: freeze the carry point and
      `velocity_pixels_moving` reads the whole covered area rather than 0.
- [ ] board:1989's 32-float placement row lands on top of it -- current transform then previous,
      with a per-slot frame stamp so two moves in one frame cannot lose the old value.

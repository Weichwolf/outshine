Type: feature
State: open
Progress: gpu-driven
Area: render
Tags: benchmark, target

# Nothing on the frame path scales with geometry, lights or pixels on the CPU

**Benchmark** — Unreal: the same answer as 1926 for the whole frame path, not only lights. RAGE: per-batch. **Taking Unreal**.

This is the area where the distance to both benchmarks is largest. Unreal drives its passes from
`FGPUScene`: instances live in GPU buffers, culling runs in compute, and the draw is indirect --
the CPU issues one call for a pass, not one per batch. Lights are assigned to clusters in a
compute pass and the shading stage reads the grid. RAGE is reconstructed and less certain, but
its drawlists are likewise fed from resident buffers.

outshine issues a uniform push per batch, twice per frame.

- [x] the tone chain is checkable end to end against a closed form, so a factor of two cannot
      hide in it
      proof: outshine/door/ScoreWhatALitSurfaceReads
**THE GEOMETRY HALF OF THIS INVARIANT IS BUILT IN board:1989**, which carries the measurement,
the five mechanisms of a GPU-driven path, the seven-step route and its oracles. This item keeps
the RULE; that one keeps the work. What stays here is everything the rule covers that is not
geometry: lights, shadows, the queries the frame makes, and the distribution it publishes.

- [ ] the shadow pass issues ONE indirect draw from the resident instance buffer, not one
      `PushGPUVertexUniformData` per batch (board:1926)
- [ ] lights are assigned to clusters or tiles by a compute stage, and `kMaxSubjectLights` stops
      being the light budget (board:1926)
- [ ] the shadow atlas carries cascades and the shader picks one (board:1926)
- [ ] the shadow centre folds INSTANCE bounds, not every vertex (board:1926)
- [ ] no query the frame path makes blocks, allocates, locks or touches disk (board:1937).
      **IT ALLOCATES, AND THE NUMBER IS 1520 BYTES A FRAME.** This predicate had no instrument
      until board:1574's refit proof needed one: `Heap::Tagged` has counted bytes per named tag
      since it was written and nothing outside the library could read the count, so "the frame
      path allocates nothing" was a sentence. The engine publishes every written tag as
      `heap taken under <tag>`, cumulative, and the CONSUMER differences it across frames.
      Measured over a posed subject, eight frames:

          mesh-bvh      0 bytes   -- the refit, and board:1574 proves it
          render-frame  12160 bytes over 8 frames = 1520 a frame

      So the violation is real, it is in the frame recording rather than in the visibility
      structure, and it is now a number a case can hold to. What it is NOT yet: attributed. 1520
      bytes is small enough to be one container growing once per frame and large enough to be
      several, and naming which is the next step -- the tag is `render-frame` and everything
      inside `Renderer::RenderFrame` shares it.
      proof: outshine/door/ScoreWhatAFramePathTakes reads the counter; the predicate stays OPEN
      because the case asserts zero for `mesh-bvh` only.
- [ ] the frame publishes p50/p95/p99 over a moving camera, not a mean (board:1457)
- [ ] geometry carries its own detail ladder and the pool holding it is a slot table
      (board:1512)

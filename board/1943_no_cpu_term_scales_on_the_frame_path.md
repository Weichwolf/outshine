Type: feature
State: open
Progress: gpu-driven
Area: render
Tags: benchmark, target

# Nothing on the frame path scales with geometry, lights or pixels on the CPU

This is the area where the distance to both benchmarks is largest. Unreal drives its passes from
`FGPUScene`: instances live in GPU buffers, culling runs in compute, and the draw is indirect --
the CPU issues one call for a pass, not one per batch. Lights are assigned to clusters in a
compute pass and the shading stage reads the grid. RAGE is reconstructed and less certain, but
its drawlists are likewise fed from resident buffers.

outshine issues a uniform push per batch, twice per frame.

- [x] the tone chain is checkable end to end against a closed form, so a factor of two cannot
      hide in it
      proof: harness/outshine/door/ScoreWhatALitSurfaceReads
- [ ] the shadow pass issues ONE indirect draw from the resident instance buffer, not one
      `PushGPUVertexUniformData` per batch (board:1926)
- [ ] lights are assigned to clusters or tiles by a compute stage, and `kMaxSubjectLights` stops
      being the light budget (board:1926)
- [ ] the shadow atlas carries cascades and the shader picks one (board:1926)
- [ ] the shadow centre folds INSTANCE bounds, not every vertex (board:1926)
- [ ] no query the frame path makes blocks, allocates, locks or touches disk (board:1937)
- [ ] the frame publishes p50/p95/p99 over a moving camera, not a mean (board:1457)
- [ ] geometry carries its own detail ladder and the pool holding it is a slot table
      (board:1512)

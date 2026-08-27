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
## Which parts of Nanite this tree wants, and the one it does not

Nanite is six mechanisms and they are separable. Measured against what stands here:

| mechanism | here | take it? |
|---|---|---|
| cluster DAG with self/parent error bounds | `src/base/spatial/ClusterDag.h` -- `SelfErr`, `ParentErr`, bounding sphere per cluster, exactly Nanite's shape. Built for TERRAIN only (`TilePool`) | **yes**, and it must reach subjects |
| GPU culling of clusters in compute | absent | **yes** -- it is what makes the DAG worth having |
| persistent instance buffer (`FGPUScene`) | absent; `ModelSlot` is part of the batch key | **yes** -- see the predicate below |
| ONE indirect draw per pass | absent | **yes** |
| visibility buffer (write cluster+triangle, shade later) | absent | **partly** -- it decouples geometry from material, but most of its win is paired with the next row |
| COMPUTE RASTERISER | absent | **NO** |

**Why the compute rasteriser is the one to refuse.** Its reason is sub-pixel triangles: hardware
rasterises in 2x2 quads, so a triangle smaller than a pixel shades up to four times over, and
film-scale assets are made of them. That is a real cost for content nobody authored for the
budget. This engine generates its own geometry and CHOOSES the density -- a cluster DAG whose
error bound is tuned to 720p never asks the hardware to draw a triangle that small. Paying for a
second rasteriser to fix a problem the LOD selection already prevents is buying the cure for a
disease we do not have.

The other five are not Nanite-specific at all -- they are what "GPU-driven" means, and RAGE
reaches the same place from the other side with its own draw-list culling. Refusing one row of a
famous design is not refusing the design.

- [ ] the cluster DAG reaches SUBJECTS, not only terrain -- it is built and proven for one and
      unreachable for the other
- [ ] **TWO IDENTICAL SUBJECTS COST TWO DRAW CALLS, and the instancing is already declared.**
      Measured on board:1574's case: `one subject draws 1 batch(es), two draw 2`. `DrawList`
      merges by `SameState` -- material, layout, kind AND `ModelSlot` -- so two subjects can
      never merge however identical, because their placement rows differ. And `DrawBatch` already
      carries `uint32_t Draws = 1`, an instance count that nothing ever raises.
      Unreal's answer is `FGPUScene`: the model slot is read PER INSTANCE out of a GPU buffer
      rather than being part of the batch key, so identical meshes merge and `Draws` counts them.

      **MEASURED, and it is four coupled changes rather than one.** The placement is folded into
      the MVP on the CPU and pushed as a UNIFORM -- `SDL_PushGPUVertexUniformData` once per
      ModelSlot change (`SubjectDraw.cpp:715-757`), and the shader reads `s.mvp`
      (`shaders/subject.msl`). A draw carries one uniform, which is exactly WHY the slot has to be
      in the batch key: it is not an oversight, it is the consequence of where the matrix lives.

      What has to move, each step measurable on its own:

      1. the uniform carries `viewProj` (constant per pass) instead of a premultiplied `mvp`
      2. the placements upload as a VERTEX storage buffer -- the tree already uses storage
         buffers, but only in the fragment stage (`kSubjectStorageBuffers = 2`, BVH nodes and
         triangles)
      3. `subject.msl` reads `placements[first_instance + instance_id]` and does the multiply --
         one edit, because all seven vertex arms come from one macro
      4. `SameState` drops `ModelSlot`, and the draw becomes
         `SDL_DrawGPUIndexedPrimitives(pass, IndexCount, batch.Draws, FirstIndex, 0, ModelSlot)`
         -- the signature already carries the instance count and the first instance, both pinned
         at 1 and 0 today

      The oracle at the end is board:1574's case unchanged: `two draw 2` becomes `two draw 1`
      while the picture stays identical, and identical is what `linear_channels_differing_between
      _renders` already measures.
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

Type: feature
State: active
Parent: 1953
Area: render
Tags: performance, shadow, light, measured

# No term on the light and shadow path scales with geometry, lights or pixels on the CPU

"Light and shadow are GPU-side" is the intent; it is not measurable as written, because somebody
has to write the uniform and the plan is a CPU declaration. RAGE and Unreal both keep the SETUP
on the CPU -- which light, which cascade bounds -- and it is O(lights) and tiny. What they never
do is touch a batch or a pixel from the CPU on this path.

So the bar is: **per frame, no CPU term scales with geometry, light count or pixels.**

Measured at HEAD against it:

| term | cost per frame | verdict |
|---|---|---|
| `LightVisibilityStage::Build` -- one ortho frustum in `double` | O(1) | correct, and it stays |
| `Live::PlacedBounds` -- folds every vertex for the shadow centre | O(vertices), cached behind `BoundsPlaced_` and invalidated only on restand | tolerable while the cache holds; a moving caster would break it |
| `SubjectDraw::PackedLights` -- packs lights into one fixed uniform | O(lights), `kMaxSubjectLights` slots | **RED.** No clustering, no tiling, no per-tile list. A handful of lights is the ceiling and the ceiling is a constant in a header |
| `LightVisibilityStage::Cast` -- walks every batch, pushes a uniform per batch | O(batches) | **RED.** The shadow pass costs the same CPU as the colour pass, so the frame pays twice for one picture |
| the atlas: one 2048x2048, no cascades | -- | **RED.** Resolution is one fixed compromise; the near field is starved and the far field is wasted |

## What the benchmark does

Unreal assigns lights to clusters in a compute shader and reads the cluster list in the shading
pass; the shadow projection is entirely GPU; cascade selection is a shader term. RAGE is
reconstructed and less certain, but its shadow pass is likewise fed from resident buffers rather
than from a per-batch CPU walk. Neither engine's CPU sees a batch on this path.

## What will be true

Every row above whose verdict is not "correct, and it stays" has a box here, and every box names
the mechanism the benchmark uses rather than an outcome.

- [ ] **`Cast` issues ONE indirect draw, not one per batch.** The shadow pass reads the same
      resident instance buffer the colour pass reads and draws it with
      `SDL_DrawGPUIndexedPrimitivesIndirect`, with per-instance transforms in a storage buffer
      the GPU indexes by instance id -- never a `PushGPUVertexUniformData` in a loop. This is
      Unreal's GPU-driven shadow path (`FGPUScene` + instance culling into indirect args) and
      the shape RAGE's drawlists take. The CPU cost of the shadow pass becomes O(1).
- [ ] **Lights are assigned to clusters by a compute stage.** A froxel grid over the view
      frustum, one compute pass writing a per-cluster light index list, and the shading pass
      reading its own cluster -- Unreal's clustered deferred (`LightGridInjection`), which
      replaced its own fixed forward light slots for exactly this reason.
      `kMaxSubjectLights` stops being the light budget; the grid's list capacity is, and it is a
      GPU allocation rather than a constant in a header.
- [ ] **The atlas carries cascades and the shader picks one.** Three or four view-aligned
      cascades packed into the one atlas, split by a practical distribution
      (`lambda`-weighted logarithmic/uniform blend, the PSSM form both engines use), each
      snapped to its own texel grid so the near field stops crawling. Which cascade a fragment
      reads is a depth comparison in the shader, not a CPU decision.
- [x] **The shadow centre comes from a volume per part, not from a vertex fold.** `PlacedBounds`
      folds a bounding volume for each part -- built once over every animation frame, transformed
      by that part's placement on every call. `O(parts * 8)` corners instead of `O(vertices)`, and
      `BoundsPlaced_` is gone, so nothing can go stale. The drive is unchanged.
      The centre FOLLOWS a caster that moves, proven now that a body can move without a route:
      the crate falls a further 3.697924 m between two frames and the frustum descends by
      3.697924 m. That is `LightVisibilityStage::Build` reading placements every frame, which is
      board:1951's work rather than this fold's.
      proof: harness/outshine/door/ScoreWhatABodyWithNoRouteDoes
      `Live::PlacedBounds` walks every vertex and is correct only while `BoundsPlaced_` holds:
      the day a caster MOVES under a still camera the cache is stale and the frustum follows the
      wrong body. Both benchmarks keep a bounding volume per instance in the scene structure and
      fold the volumes, never the vertices -- so the centre costs O(instances) and updates when
      an instance moves. Until it does, a moving caster is a defect waiting on a scenario that
      declares one.
- [ ] Proving case: a scenario with N casters and M lights holds its CPU frame time flat as N
      and M grow, measured p50/p95/p99 over a moving camera. Negative control: the per-batch
      walk restored, and the CPU time tracks N.

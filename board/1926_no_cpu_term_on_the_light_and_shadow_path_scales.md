Type: feature
State: open
Parent: 1953
Area: render
Tags: performance, shadow, light, measured

# No term on the light and shadow path scales with geometry, lights or pixels on the CPU

**Benchmark** — Unreal: `FGPUScene` holds instances GPU-side, Nanite culls in compute, a pass is ONE indirect draw. RAGE: per-batch draw calls. **Taking Unreal** — no CPU term may scale with geometry or lights.

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

**WHAT IT WOULD BUY, COMPUTED, BEFORE IT IS BUILT.** SDL_GPU carries what the change needs:
`SDL_DrawGPUIndexedPrimitivesIndirect` with `SDL_GPUIndexedIndirectDrawCommand`, whose
`first_instance` is exactly where a model slot rides, and the shaders here are hand-written MSL so
`[[base_instance]]` is available without a cross-compiler in the way. So the obstacle is not the
API.

The obstacle is that the term does not yet bind. Measured:

    the drive          10 batches per frame
    the many-caster case   64 batches per frame

A draw call costs on the order of 1 to 5 microseconds of CPU. Sixty-four is about 0.3 ms of a
16.7 ms budget -- two percent. It binds at a thousand batches, which is 5 ms and a third of the
frame, and nothing this tree can currently declare comes near that: the widest scene it can stand
is one subject's parts plus a ground ring.

So this is CORRECT and NOT YET URGENT, on the same reading board:1960 got and for the same reason.
`ScoreWhatManyCastersCost` already reads the number, so the day a scenario stands a thousand
casters the case says so by that number tracking the scene into the thousands -- and THEN this is
built, against a measurement rather than against Unreal's conclusion.

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
      proof: outshine/door/ScoreWhatABodyWithNoRouteDoes
      `Live::PlacedBounds` walks every vertex and is correct only while `BoundsPlaced_` holds:
      the day a caster MOVES under a still camera the cache is stale and the frustum follows the
      wrong body. Both benchmarks keep a bounding volume per instance in the scene structure and
      fold the volumes, never the vertices -- so the centre costs O(instances) and updates when
      an instance moves. Until it does, a moving caster is a defect waiting on a scenario that
      declares one.
**MEASURED FIRST, WHICH CHANGED WHAT IS WORTH BUILDING.** A scenario with many casters did not
exist until a client's generator could make one; it does now, and the readings are:

    ONE CASTER            1 shadow batch(es), drawing left    128 bytes
    SIXTY-FOUR CASTERS   64 shadow batch(es), drawing left    128 bytes
    proof: outshine/door/ScoreWhatManyCastersCost

So the ALLOCATION half of this item already holds and is now guarded: 128 bytes at one caster and
128 at sixty-four, constant rather than zero, which is what CLAUDE.md's *bounded* asks for. The
DRAW half is exactly as this item describes -- one draw per batch, tracking the scene one for one --
and it is measured rather than asserted, so the day `Cast` becomes one indirect draw the case says
so by that number leaving N behind.

- [ ] Proving case: a scenario with N casters and M lights holds its CPU frame time flat as N
      and M grow, measured p50/p95/p99 over a moving camera. Negative control: the per-batch
      walk restored, and the CPU time tracks N.

Type: feature
State: open
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

- [ ] The shadow pass draws INDIRECTLY from a resident instance buffer -- the CPU issues one
      call, not one per batch.
- [ ] Lights are assigned to clusters or tiles by a compute stage, and the shading pass reads
      that assignment. `kMaxSubjectLights` stops being the light budget.
- [ ] The atlas carries cascades, and which cascade a fragment reads is decided in the shader.
- [ ] Proving case: a scenario with N casters and M lights holds its CPU frame time flat as N
      and M grow, measured p50/p95/p99 over a moving camera. Negative control: the per-batch
      walk restored, and the CPU time tracks N.

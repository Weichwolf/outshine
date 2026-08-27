Type: feature
State: active
Parent: 1995
Depends: 1574
Area: render
Tags: benchmark, target, gpu-driven

# Instances live in a GPU buffer, and a draw reads its placement per instance

**Benchmark** — Unreal: `FGPUScene` keeps per-instance transforms in a structured buffer; the vertex shader indexes it by instance and the CPU pushes no per-object uniform. RAGE: per-instance data is written into the draw list the device consumes. **Taking Unreal**, because the buffer is what every later step reads from — culling writes into it, the indirect draw counts out of it. **Reference**: Haar & Aaltonen, *GPU-Driven Rendering Pipelines*, SIGGRAPH 2015 Advances in Real-Time Rendering — the instance buffer is step one there too, for the same reason.

## What

The placement of a drawn thing is a row in a GPU buffer, addressed by instance index. Not a
uniform pushed before each draw.

## Why

**Measured**: two identical subjects cost two draw calls.

    one subject draws 1 batch(es), two draw 2      outshine/door/ScoreWhatASecondSubjectDoes

The cause is structural, not an oversight. `SubjectDraw.cpp:715-757` folds the placement into the
MVP on the CPU and pushes it as a UNIFORM, once per `ModelSlot` change; `shaders/subject.msl`
reads `s.mvp`. **A draw carries one uniform, so the slot HAS to be in the batch key** —
`DrawList.cpp:10` merges on material, layout, kind and `ModelSlot`. The batch key is a
consequence of where the matrix lives.

So a CPU term scales with the number of subjects, which is exactly what board:1943 forbids.

## How

1. the uniform carries `viewProj` rather than a premultiplied `mvp`, and the shader multiplies by
   the model matrix it ALREADY receives (`S::model`, uniform floats 40..55)
2. placements upload as a VERTEX storage buffer — the tree uses storage buffers already, but only
   in the fragment stage (`kSubjectStorageBuffers = 2`: BVH nodes and triangles), so the pipeline
   shape gains a vertex slot
3. `subject.msl` reads `placements[first_instance + instance_id]` — ONE edit, because all seven
   vertex arms come from one macro
4. `SameState` drops `ModelSlot`; the draw passes `batch.Draws` and `ModelSlot` as first instance.
   `SDL_DrawGPUIndexedPrimitives` already takes both, pinned at 1 and 0 today

- [x] the uniform carries `viewProj` and the shader multiplies. `S` is now
      `{viewProj, prevViewProj, model, prevModel, lightFromModel}` -- five matrices, 80 floats,
      where it was 72 with two dead `float4` anchors board:1990 emptied. The PREVIOUS frame gets
      the same treatment in the same commit, so the two frames cannot drift apart again.
      **`subjectDepthOnly.msl` keeps its own minimal `S { float4x4 mvp; }`** and its own uniform,
      which the gate found by refusing to compile it -- a second binding with a second layout,
      and it will need the same per-instance read at step 3 rather than being left behind.
      proof: khronos/glTF/WaterBottle and BoxAnimated 3/3 each, `picture_p99_delta_code` 1
      against a bound of 6.435, unmoved; gate GREEN in 37s
- [ ] placements upload as a vertex storage buffer whose rows equal the uniforms they replace
- [ ] the shader reads its placement per instance
- [ ] `SameState` drops `ModelSlot` and two identical subjects are ONE draw: board:1574's case
      reads `two draw 1` while `linear_channels_differing_between_renders` stays at zero
- [ ] the PREVIOUS placements move into a buffer too, or TAA loses every rigid motion vector.
      `S::prevMvp` and `S::prevAnc` are pushed per draw today; a per-instance current row without
      a per-instance previous row ghosts every moving subject, and it would look like a TAA bug
      rather than a missing buffer
- [ ] frame time over `apps/driver` before and after, so a slower result is visible rather than
      assumed away

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
   shape gains a vertex slot.

   **Two things measured before writing it, both of which change the shape of the step:**

   **The shift is constant per PASS, not per slot.** `Anchor` belongs to the subject and
   `PreViewTranslation` to the frame, so neither varies across the batches of one pass. It
   therefore stays in the UNIFORM and the buffer carries the raw placements from `Placed_`. That
   matters because it means the upload needs no `FrameContext` — which is the difference between
   uploading before the pass and not being able to.

   **The upload needs a copy pass, and `Encode` is already inside a render pass.** SDL_GPU can
   only write a buffer inside `SDL_BeginGPUCopyPass`, and `SubjectDraw::Encode(ctx, into)` runs
   with `into` already recording. The methods that upload today — `SetMesh`, `SetPose`,
   `HandStreams`, `HandVisibility` — all run BEFORE the pass and have no `ctx`, which is exactly
   why the placements went through a uniform in the first place. So the step needs a flush point:
   `Placed_` marks itself dirty on `MovePlacement`, and the renderer drains it where it already
   opens a copy pass (`Renderer.cpp:759`).
3. `subject.msl` reads `placements[first_instance + instance_id]` — ONE edit, because all seven
   vertex arms come from one macro
4. **CORRECTED BY STEP 3.** `SameState` cannot drop `ModelSlot` here, and the reason is that
   `ModelSlot` is the PART index -- every part carries its own index range. Merging two parts
   into one batch would issue ONE instance over both ranges and give both the first part's row.
   Merging across slots needs a per-draw index offset, which is the INDIRECT draw of board:1993.
   What step 3 does buy is the precondition: the vertex uniform now carries nothing per-instance,
   so the indirect path has nothing left to thread through it

- [x] the uniform carries `viewProj` and the shader multiplies. `S` is now
      `{viewProj, prevViewProj, model, prevModel, lightFromModel}` -- five matrices, 80 floats,
      where it was 72 with two dead `float4` anchors board:1990 emptied. The PREVIOUS frame gets
      the same treatment in the same commit, so the two frames cannot drift apart again.
      **`subjectDepthOnly.msl` keeps its own minimal `S { float4x4 mvp; }`** and its own uniform,
      which the gate found by refusing to compile it -- a second binding with a second layout,
      and it will need the same per-instance read at step 3 rather than being left behind.
      proof: khronos/glTF/WaterBottle and BoxAnimated 3/3 each, `picture_p99_delta_code` 1
      against a bound of 6.435, unmoved; gate GREEN in 37s
- [x] placements upload as a vertex storage buffer. `SubjectDraw::HandPlacements` converts
      `Placed_` to floats and crosses them through `SubjectResidency` -- the same path the BVH
      streams use -- and `MovePlacement` marks the rows stale so the upload happens once per
      change rather than once per move. The flush point is `SubjectProxy::Placed`/`Moved`, right
      after the loop that sets them and BEFORE any pass opens.
      proof: khronos/glTF/WaterBottle 3/3, `picture_p99_delta_code` at 1 against a bound of
      6.435, unmoved -- the buffer is written and not yet read, so an unmoved picture is exactly
      the right result
- [x] all three lit vertex arms read `placements[first_instance + instance_id]`, and the vertex
      uniform is now PER PASS. **The Metal question is answered by measurement**: with
      `first_instance = ModelSlot` and a one-row table, `khronos/glTF/AlphaBlendModeTest` fell to
      `coverage_fraction_outshine` 4.0e-05 against a floor of 0.001; with the base pinned at 0 the
      same run read 0.0355. A base that changed the picture is a base the shader SAW -- SDL_GPU on
      Metal folds it into `[[instance_id]]`, so no per-draw uniform is needed to carry the slot.
      The crash that led there was the finding: `Placed_.empty() ? Model : ...` gave the single
      subject NO table at all, which is the CPU branch on how-many that GPU-driven rendering
      exists to remove -- one subject is a table with one row, and the table is sized to the
      largest slot the batches name.
      Three terms then left the uniform. `model` went to the buffer; `lightFromModel` became
      `lightFromWorld` and the shader multiplies by the row it already has; `prevModel` was never
      a second matrix -- `before[i] = model[i]` -- so it is the same row under a second shift.
      `S` is `{viewProj, prevViewProj, lightFromWorld, shift, prevShift}`, 56 floats where step 1
      left 80, and every one of them is a property of the VIEW.
      proof: khronos/glTF 444/444, and outshine/door/ScoreWhatASecondSubjectDoes reads
      `one subject draws 1 batch(es), two draw 2` beside `one subject pushes 1 vertex uniform(s),
      two push 1` -- batches scale with geometry, the uniform does not.
      negative control: `place()` restored to per-batch makes that line read `2` and `3`, and the
      case goes RED on the push claim while the picture stays identical -- which is the whole
      reason the number is measured rather than looked at

- [ ] **STEP 3 NEEDS A MEASUREMENT FIRST, and it is the one that decides the shape.** The draw is
      `SDL_DrawGPUIndexedPrimitives(pass, count, 1, first, 0, 0)` -- the last argument is
      `first_instance`. In Vulkan a shader reads `gl_InstanceIndex` WITH the base folded in; in
      Metal `[[instance_id]]` starts at zero per draw and the base applies to vertex fetch, not
      to the id. If SDL_GPU on Metal does not surface the base, the shader cannot derive its row
      from `first_instance + instance_id` and the slot has to arrive another way -- a per-draw
      uniform holding the base, which still merges batches because the uniform no longer holds a
      MATRIX. Measure before writing: a shader that reads `placements[instance_id]` with a base
      of 1 either draws the second row or the first, and that single observation settles it
- [ ] the shader reads its placement per instance
- [ ] `SameState` drops `ModelSlot` and two identical subjects are ONE draw: board:1574's case
      reads `two draw 1` while `linear_channels_differing_between_renders` stays at zero
- [ ] the PREVIOUS placements move into a buffer too, or TAA loses every rigid motion vector.
      `S::prevMvp` and `S::prevAnc` are pushed per draw today; a per-instance current row without
      a per-instance previous row ghosts every moving subject, and it would look like a TAA bug
      rather than a missing buffer
- [ ] frame time over `apps/driver` before and after, so a slower result is visible rather than
      assumed away

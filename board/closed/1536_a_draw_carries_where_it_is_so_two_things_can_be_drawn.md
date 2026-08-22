Type: task
Parent: 1498
Area: render
Tags: scope perf

**A draw carries where it is, so two things can be drawn**

`Clients::Studio` holds ONE `const Gltf::Subject *`, and `Render::DrawItem` carries an index range and
a layout but **no transform** -- vertices reach the draw list already in world space. So the engine
can draw a road or a car, never both, and never a car that moves.

Re-packing the F31's vertices every frame is not an answer: it is 30 MB of geometry at 60 Hz. The
answer is the one every renderer has -- **a draw says where it is.**

## What must be true

- [ ] **`DrawItem` carries a model transform**, pushed per draw, and the vertex stage multiplies by
      it. SDL_GPU push constants are the mechanism; the shader's C++ twin in
      `test/render/outshine/shader` moves with it or the two stop being twins
- [ ] **`Studio` holds 1..N placed subjects**, which is the shape `CLAUDE.md` asks for everywhere
      else -- *a shape is 0 or 1..N*
- [ ] **A static subject pays nothing.** The road does not move; an identity transform must not cost
      a push, so the draw key carries whether it has one
- [ ] **The khronos picture bound does not move.** 181 criteria and 180 within bound today; a vertex
      stage that multiplies by identity must produce the same pixels, and *identical is a finding*

## Comments

**This is the last thing between the driver and the goal's windowed half.** Everything else is done
and measured: the road is swept from the same `StandAt` the wheels stand on and drawn at 1280x720
(`board:1535`), the camera comes from the scenario's declared views in both persons, and a player
takes the wheel from the mind and gives it back.

What is missing is that the F31 itself cannot appear beside the road, because the renderer can hold
one subject and place it nowhere.

**It is deliberately not started at the end of a long round.** It touches `DrawList`, `SubjectDraw`,
a shader and that shader's twin, and a renderer left half-changed would take the whole suite --
1727 tests, 181 khronos criteria -- with it.

## Comments

Closed: Renderer::SetSubjectPlacements places 1..N subjects and a draw stands where its own placement says (commit of 2026-08-21 under board:1543), so road and car are drawn together in the windowed drive.

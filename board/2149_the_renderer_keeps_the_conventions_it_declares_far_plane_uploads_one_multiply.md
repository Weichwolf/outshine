Type: bug
State: open
Area: render
Tags: owner, audit

# The renderer keeps the conventions it declares: the far plane a view asks for, one transfer buffer per stream, one matrix multiply

**Benchmark** -- Filament and Unreal: a view's far plane is the view's; a per-frame upload
goes through a persistent, cycled transfer buffer (SDL3's `cycle = true` on a retained
buffer, exactly what this tree does at GroundLattice.cpp:505); a matrix multiply exists
once. The audit of 2026-09-05 found three places where the renderer says one thing and does
another.

## Where it stands, measured 2026-09-05

```
  SceneRenderer.cpp:93     const float zf = 60000.0f  -- the orthographic branch ignores
                           Viewpoint::ZFarM; the plan view over Zurich clips at a number
  SceneRenderer.cpp:99-105 a hand-rolled 4x4 multiply beside Mat4::operator*, in the function
                           whose own comment warns that a transpose mirrors the scene
  SceneRenderer.cpp:86,    the jitter's sign differs between the perspective (-ndc) and the
  :93 (ortho q[12])        orthographic (+ndc) branch
  OverlayDraw.cpp:109-181  a transfer buffer created, mapped, copied, submitted and released
                           TWICE PER FRAME; SceneRenderer.cpp:864 and GroundLattice.cpp:350
                           once per upload
  Mat4.h:80                Row() returns the column-major storage
  tonemap.msl:13 +         no encode on the path where the target is linear (Temporal)
  TonemapStage.cpp:55
```

## The solution

- the orthographic projection takes `ZFarM` like the perspective one; one jitter sign
- `Mat4::operator*` is the multiply; the loop goes
- one retained transfer buffer per upload stream, cycled, the way GroundLattice already does;
  the create-per-upload sites are rewritten to it and the fence count per frame published
- `Row()` renamed to what it returns; the tonemap path encodes where the target is linear,
  measured by a case that reads a known grey back

## What will be true

- [ ] ZurichPlan's plan view is rendered with the declared far plane, measured by a
      declaration that sets it below 60 km and reads the horizon clip move
- [ ] `SDL_CreateGPUTransferBuffer` is called 0 times per frame at every reference place
      (a counter a client reads), and the nine references stay bit-identical
- [ ] Negative control: the jitter sign flipped in one branch moves the temporal
      reference by a measured count

## Measured 2026-09-05: a dark vertical face in shadow reads 1.8 % of its lit value

With board:2148 the road's kerb faces are drawn. A shadowed house wall (albedo ~0.6) reads
70 of 165 lit, 16 % linear -- the sky's share of the sun, right. The kerb (asphalt, albedo
~0.05) reads (16, 15, 16) against a lit top of (118, 113, 109): 1.8 % linear. The same sky
should give the same share; a factor of nine says the vertical dark face is missing a term
(the sky's irradiance for a horizontal normal, a bounce, or an occlusion applied twice).
Filament's IndirectLight gives a horizontal normal half the sky and the ground's bounce;
the number to reach is the wall's 16 %, measured on that kerb.

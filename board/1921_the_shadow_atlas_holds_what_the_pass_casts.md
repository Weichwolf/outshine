Type: bug
State: open
Parent: 1867
Area: render
Tags: measured, picture, shadow

# The shadow atlas holds what the pass casts into it

The receiving half is wired and INERT, because the casting half writes nothing. Measured at HEAD
by reading the atlas back:

    batches the shadow casts            = 259 batches
    frames the subject drew shadowed    = 31 frames
    the plan keeps the atlas            = 1
    the shadow atlas, least depth       = 0
    its most                            = 0
    texels it wrote                     = 4 194 304 of 4 194 304 below 0.999

**Every texel of a 2048 x 2048 atlas is zero.** 259 batches are encoded into it every frame and
not one fragment survives the depth test.

## What is ruled out, each by a number

- **It is not discarded.** `RenderPlan::Stored(ShadowAtlas)` is 1, so the pass stores rather than
  DONT_CARE -- which it did before anything read the atlas, and would have been the first guess.
- **It is not the convention.** `LightVisibilityStage.cpp:161` compares GREATER and
  `Renderer.cpp:681` clears to 0: reversed-Z, consistently. The projection agrees --
  `z = -forward.p/(far-near) + far/(far-near)` gives 1 at the near plane and 0 at the far one.
- **It is not the binding.** `frames the subject drew shadowed = 31` says the flag and the
  sampler reach the subject shader on every frame.

What is left is the light frustum itself: `Declare(toSun, up, radiusM)` and `Frame(centreM)`, and
whether the radius the subject's extent derives still means anything now that the extent includes
five kilometres of composed ground (board:1890).

## What will be true

- [ ] The atlas carries a depth range: some texels at the near plane, some at the far one, and
      the count below the clear is the silhouette's and not the whole surface.
- [ ] Proving case: a subject over a ground under a declared key writes an atlas whose depths are
      not all equal, and the lit picture is darker where the caster's silhouette falls. Negative
      control: the light frustum as it stands, and the atlas is one value.

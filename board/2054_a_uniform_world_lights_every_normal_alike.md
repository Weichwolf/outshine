Type: defect
State: open
Area: render
Tags: shading, corpora

# A uniform world lights every normal ALIKE, and at the radiance it states

**Benchmark** — Unreal: an ambient cube with a constant radiance integrates to `albedo * L` for a
Lambertian, independent of the normal, and its `SkyLight` in "captured constant" mode is exactly
that. RAGE: the same, its ambient rig reduces to a constant term when the probe is uniform.
**They agree, and so does the physics**: for a Lambert surface under a world of constant radiance
L, the outgoing radiance is `rho * L` and the normal does not appear in it.

## What was measured

The Khronos corpus's six `diffuse` cases, through `outshine-client`, against Blender's own frame.
Blender's world is uniform at `colourLinear 0.05087608844041824`, `strength 1.0`, with `bounces.max
= 0` and no light, so its Diffuse BSDF integrates to exactly `rho * L`.

    Triangle          Blender writes ONE value: 57 of 255
                      sRGB(57/255) is 0.0409 linear, and albedo 0.8 x 0.05087608844 = 0.0407
                      OURS writes 44 of 255, which is 0.0241 linear -- 1.70 times too dark

    BoxInterleaved    Blender's frame holds TWO values, 0 and 57: the background and the box, and
                      every face of the box is the SAME because the world is uniform
                      OURS holds a GRADIENT -- 0, 2, 3, 4, 5 and on -- so our ambient depends on
                      the normal where a uniform world cannot

Two failures in one term and they are separable: a magnitude that is 1.7 low, and a direction
dependence that should not exist at all.

## What will be true

- [ ] a Lambert surface under a uniform ambient outputs `rho * L` and the normal does not enter it
- [ ] `Triangle` and `TriangleWithoutIndices` read 57 of 255 where the oracle reads 57
- [ ] `BoxInterleaved` holds two values in the frame, as its oracle does
- [ ] the six diffuse cases hold at 99.99 per cent of pixels within 8 of 255
- [ ] the negative control: scaling the ambient term by anything but one puts them red again

## What this does NOT cover

The 1.70 is a RATIO and not yet a cause. It is close to neither pi nor 1/pi nor 2, so a guess at
which constant went missing would be a guess; the derivation belongs in the fix. And whether the
same term is what makes nine `metal-rough` cases disagree is UNMEASURED -- those carry a sun as
well, so they may be two faults or one.

Type: bug
Area: render
Tags: oracle, perf, instrument

**Frame alpha is derived from depth, so a translucent body over nothing is absent from our picture and present in the oracle's — **Band 1****

`src/render/stages/Resolve.h:47,61`. The whole of the coverage predicate is

```wgsl
fn covered(sceneDepth : f32) -> f32 { return select(0.0, 1.0, sceneDepth > 0.0); }
...
let a = covered(sceneDepth);
```

and `displayed` returns that as the frame's alpha (`:65-66`). A `BLEND` surface **writes no depth** —
`core/SurfaceState.h:63-67`, `SurfaceKind::Blended` sets `WritesDepth_ = false`, correctly and for the
reason stated on the line — so a translucent surface with nothing opaque behind it contributes radiance
to `SceneHdr` and **zero** to alpha. Against a `filmTransparent` Cycles render, which carries the
surface's own alpha, the pixel is present on one side and absent on the other.

**Found by a case that passes, which is why it needs writing down.** `AlphaBlendModeTest` is green, and
it is green on a property of the **asset**: its manifest records the measurement —
*"behind all of them stands the OPAQUE Bed, a box spanning x [-4.3, 4.3], y [-0.1, 2.3], z [-0.75, 0.55]
— MEASURED from the node transforms — so every blended pixel of this subject has an opaque surface
behind it"* (`test/khronos/glTF/AlphaBlendModeTest/manifest.json:31`). The predicate is well defined
**there** and undefined in general, and nothing in the engine says so.

**The harmless explanations, sought.** *No case fails today* — true, and it is the reason this is a
latent defect rather than a red: the one asset that could expose it happens to carry its own backdrop.
*Alpha is only for the readback, not the picture* — no: `Resolve.h:16-21` states alpha is the channel
that separates a black surface from the background (*measured, the oracle's sphere carries 46 101 of
46 151 covered pixels at exactly 0.0 RGB*) **and** that it *"is also the channel blending will need"*,
so it is load-bearing for the thing that breaks it. *It is scope, not a bug* — the code computes a
coverage value and claims in its own comment that this is *"the whole of the coverage predicate"*; it
answers the question and answers it wrongly for a class of input the engine already admits.

**Right:** alpha comes from what was drawn, not from what wrote depth — the scene target's own
accumulated alpha for blended contributions, composited with the depth predicate for opaque ones, so
`covered` stops being the sole source. **Fixed when** a case whose subject is a single `BLEND` quad over
empty background — nothing behind it, `filmTransparent` on both sides — agrees on alpha. **That case
does not exist and is owed with the repair**, because the defect is invisible to every asset that
supplies its own bed.

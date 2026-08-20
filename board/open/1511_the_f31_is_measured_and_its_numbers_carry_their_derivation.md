Type: task
Parent: 1498
Area: assets
Tags: instrument

**The F31 is measured, and its numbers carry their derivation**

**The subject of `board:1498`'s first drive**, a 2014 BMW 3 Series (F31) by DisneyCars, **CC-BY-4.0** --
*"This work is based on 2014 BMW 3 Series (F31) by DisneyCars, licensed under CC-BY-4.0"* -- 519 nodes,
258 meshes, 23 materials, `KHR_materials_clearcoat`.

```
scene.gltf  c60068fcd0f8c25e73225cd3725a422fca46c00a2a68ca481988a6680cc5fb1d
scene.bin   be46e9c11f5b7f16a2cc01a3a96b92394bff04ed3742a8974de2f9bc093ba453
```

## THE ASSET CARRIES NO ATTACHMENT POINTS AT ALL, and that is the ordinary case

**519 nodes and not one semantic name.** No `wheel_*`, no `steering`, no `door` -- the names are
exporter-generated and grouped by MATERIAL: `interior2_f31_interior2_0`. So `board:1509`'s *the
declaration names the model's nodes* meets reality on its first asset: **there are no nodes to name.**

**The answer is not a convention the asset must satisfy.** It is to MEASURE the asset once, and declare
the numbers with where they came from -- which is what `CLAUDE.md` asks of every number anyway.

## What was measured, and how

**The tyre material `f31_gum` is the handle.** Its points within 3 units of the model's lowest point are
the four contact patches, and they separate cleanly:

| | |
|---|---|
| four patches | **2873 points each, all at the same height** -- the model is built mirrored, which is why they are identical |
| wheelbase | **180.71 u** |
| track, both axles | **99.59 u** |
| wheel centre above the patch | **21.43 u** |

**The model declares no scale, so it is DERIVED from a published dimension** -- the wheelbase, 2810 mm,
because it is the longest baseline between two features measured exactly:

| from | u/m | 1 u |
|---|---|---|
| **wheelbase / 2810 mm** | **64.31** | **15.550 mm** |
| track front / 1543 mm | 64.55 | 15.493 mm |
| track rear / 1583 mm | 62.92 | 15.894 mm |

**The first two agree to 0.4 %**, which is what says the derivation is sound. **The third disagrees by
2.5 % because the model builds both tracks the same width and the real car does not** -- an asset
simplification, named rather than averaged away.

**At that scale the tyre radius comes out 0.333 m against 0.328 m published for a 225/50 R17 -- 1.5 %.**
*That is a fourth independent check nobody chose, and it agreeing is the reason to believe the other
three.*

## The driver's eye, and it is an estimate that must be LOOKED at

**The interior is modelled, which is why this asset was chosen.** The front-left seat carries **13 791
points against the right's 6 442** -- the steering wheel and its column are on the left, so the car is
**left-hand drive**, as a German car should be.

- seat geometry spans **0.251 m to 1.348 m** above the ground plane
- the eye is DECLARED at **1.220 m** above ground and **0.494 m** left of the centreline

- [ ] **The eye height is an estimate and is not yet confirmed.** It is derived as the headrest top less
      0.13 m, cross-checked against a saloon's usual 1.15--1.25 m. *`CLAUDE.md` says appearance is
      judged by eye and in motion, so this number is settled by sitting in it and looking, not by
      arithmetic*

## What must be true

- [ ] **The asset is fetched and pinned like every other corpus subject**, not committed -- it is 30 MB
      and the preparer already has the mechanism, the digests are above
- [ ] **The attribution travels with it**, because CC-BY requires it and the licence file is part of the
      download
- [ ] **Every number in `tools/driver/f31.scenario` traces to a measurement or a published dimension**,
      and the ones that are neither -- spring rates, damping, cornering stiffness -- are marked as
      `[SET]` estimates until a measurement replaces them
- [ ] **The spring and damper rates are DERIVED rather than guessed**: a declared ride frequency and
      damping ratio give both from the corner mass, which is how a suspension is actually specified

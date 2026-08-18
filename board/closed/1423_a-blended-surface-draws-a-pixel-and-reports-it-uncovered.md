Type: bug
Area: render
Tags: khronos, instrument

**A blended surface draws a pixel and reports it uncovered**

**We cover systematically LESS than the oracle, and every case where we do carries a `BLEND` material.**
[MEASURED] over the red silhouette cases:

| case | our coverage | the oracle's | IoU | blended materials |
|---|---|---|---|---|
| `GlassVaseFlowers` | 0.028257378 | **0.040290799** | 0.70128996 | 1 of 3 |
| `CompareAlphaCoverage` | 0.036116536 | **0.040163845** | 0.89923004 | 1 of 4 |
| `IridescenceMetallicSpheres` | 0.062067057 | **0.066912977** | 0.92754751 | 1 of 344 |
| `ClearCoatTest` | 0.033837891 | **0.035729167** | 0.94706633 | 1 of 19 |
| `TransmissionOrderTest` | 0.054801432 | **0.054903429** | 0.99814226 | 1 of 6 |
| `SimpleSkin` | 0.041625434 | 0.041448568 | 0.9591842 | **none** -- and it is the only one that covers MORE |

**And back-face culling is NOT it, which was the first guess.** `CompareAlphaCoverage` is
`doubleSided` on every one of its four materials and still covers 10 % less.

## The mechanism, end to end

1. **`BLEND` must not write depth** and the engine is right about that: *it composites with what is
   behind it, so writing the depth that would hide that is the one thing it must not do*
2. **The frame's alpha IS coverage and it is taken from the depth**: `displayed()` computes
   `a = covered(sceneDepth)`, which is 0 wherever nothing wrote depth
3. **So a blended fragment draws its colour and reports the pixel as empty** -- the engine's own alpha
   channel contradicts the engine's own colour channel
4. **And the scene target's alpha cannot recover it either.** The blend is `over` on alpha --
   `src ONE, dst ONE_MINUS_SRC_ALPHA` -- but the target is CLEARED to alpha **1**, so
   `a_out = a_src + 1 * (1 - a_src) = 1` everywhere. **The clear destroys the coverage before anyone
   can read it**

## Why the depth was used, and it is a real argument that must survive the repair

`Resolve.h`: *without it a black subject and no subject are the same three channels* -- [MEASURED] the
oracle's sphere carries 46 101 of 46 151 covered pixels at exactly 0.0 RGB. **Any repair has to keep a
black OPAQUE subject covered**, which a target cleared to alpha 0 does: an opaque arm writes 1 and
replaces, a blended arm accumulates from 0, and the background stays 0.

## What must be true

- [x] **A pixel a blended surface drew is covered, and by its own alpha** -- glTF's `BLEND` contributes
  coverage and the oracle reports it as the blend factor, not as 1
- [x] **A black opaque subject stays covered**, which is the property the depth was there for
- [x] **The blast radius is measured before the change and after**: [MEASURED] **13 of 145** cases carry
  a `BLEND` material, and every case's alpha channel is compared, so the corpus is the net
- [x] **The composite and the display transfer agree about what alpha means**, or the transmissive pass
  reads a coverage the opaque pass did not write

## What it bought: SEVEN cases from one repair

| case | before | after |
|---|---|---|
| `CompareAlphaCoverage` | IoU 0.89923004 | **green** |
| `ClearCoatTest` | IoU 0.94706633 | **green** |
| `TransmissionOrderTest` | IoU 0.99814226 | **green** |
| `IridescenceDielectricSpheres` | IoU 0.92754751 | **green** |
| `IridescenceMetallicSpheres` | IoU 0.92754751 | **green** |
| `DiffuseTransmissionTest` | 3830 samples apart | **green** |
| `CompareTransmission` | 5282 samples apart | **green** |

**One cause, seven cases** -- and the two iridescence sweeps had reported the identical number all
along, which is what said their geometry was one thing and not two.

## The repair, in two halves that had to agree

**The engine**: `SceneHdr`, `SceneComposited` and `SceneLinear` clear to alpha **0**, and the display
transfer takes coverage as `max(covered(sceneDepth), scene.a)`. The other targets keep their 1 --
`SceneTransmissive` is premultiplied and read by a stage that knows it, and the velocity sentinel means
something else entirely.

**The harness**: `FromDepth` reads the linear tap's alpha beside the depth, because a mask built from
depth alone called every transparent pixel empty on OUR side while the oracle's own alpha counted it.

**Both halves were needed and neither is sufficient**: with only the engine's half the corpus did not
move at all, because the harness never read the picture's alpha.

## What is NOT fixed by it

`GlassVaseFlowers`, `SpecularTest` and `SimpleSkin` are still red, so their disagreement is something
else -- and `SimpleSkin` carries no blended material at all, which `board:1419` had already established.

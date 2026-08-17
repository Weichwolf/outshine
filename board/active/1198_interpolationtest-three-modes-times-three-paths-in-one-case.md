Type: task
Parent: 1128
Area: corpus
Tags: oracle, khronos, instrument

**InterpolationTest: three modes times three paths, in one case**

The second case of the animated tier, and it closes more than its own row. **Read at the pin, not
recalled** — `InterpolationTest` carries **nine animations, each on its own node**, and they are an exact
matrix:

| | scale | rotation | translation |
|---|---|---|---|
| **`STEP`** | node 0 | node 3 | node 6 |
| **`LINEAR`** | node 1 | node 5 | node 8 |
| **`CUBICSPLINE`** | node 2 | node 4 | node 7 |

So one case decides **three interpolation modes across three animated paths**, and it closes **scale
channels**, which `board:1169` recorded as unexercised by anything and which no other asset in the corpus
animates.

Files at the pin `2bac6f8c57bf471df0d2a1e8a8ec023c7801dddf`, digests taken here:

```
InterpolationTest.gltf      4357b9e260f2e7ffb153255da37c20d6c20a03c76cbebf959a0a4f144a873028  8784 B
InterpolationTest_data.bin  f4ab866b1b34a0b0ae345a4ac37f2b13b49f6f4e4068ef345880bfcdcdd4d961  1628 B
InterpolationTest_img0.png  dc89eecc149440e13448baf1434b313b1c81ecd027f6f68f8ba20e32eefcc0b4  1822 B
```

## The engine change this forces, and it is the point of the case

**`Gltf::Pose` applies ONE animation** — `Pose::At(document, animationIndex)` — and this asset's picture is
**all nine playing at once**, each on its own node. A case that declared one animation would render eight
static cubes beside one moving one and would be a third of a third of what the asset states.

**So the pose becomes a set rather than an index**, which is also what the format says: animations are
independent and a client may play any subset. **The subset is declared by the case**, not assumed — *all
nine* is a declaration like any other, and a case that wanted one would say so.

## The oracle's half, under `board:1175`'s rule

**The oracle renders poses and does not interpolate.** Every declared frame is baked to an exact key from
the file's own accessor bytes, in the preparer, against the specification — so Blender's importer never
converts a sampler and the comparison is our sampler against the specification rather than against
Blender's reading of it.

**`CUBICSPLINE` is the arithmetic to get right**: glTF's cubic Hermite over the in-tangent · value ·
out-tangent triples, **with the tangents scaled by the segment duration**. That scaling is the only thing
separating it from a Bézier carrying the same handles, and it is where an implementation looks right and
is not.

## Before it is scored

- [ ] **The three modes must be shown to differ at the sampled frames by more than the picture bound**, per
  path, from the accessors alone. A case that cannot distinguish the modes reports green about them —
  **`STEP` is the one that would pass by accident**, because between keys it agrees with nothing and at
  keys it agrees with everything
- [ ] **The subject moves**, per `board:1169`'s clause: the drawn transform at frame *n* differs from frame
  0 by more than the instrument's floor, published. Nine nodes, so it is nine statements and not one
- [ ] **The frame grid and duration are declared**, and every frame is its own cached product

**Done when** `InterpolationTest` is in the tree and compared frame by frame; `STEP`, `LINEAR` and
`CUBICSPLINE` are each decided over scale, rotation and translation; the pose applies a declared set of
animations; and a deliberately wrong interpolation mode is shown to fail the case.

## The precondition, measured before anything is rendered

Evaluated from the file's own accessors in double, all three modes per path, over an **8 fps grid across
the asset's declared 2.000000 s** (17 frames). Worst absolute disagreement between each pair of modes, in
the units the channel carries:

| path | `CUBICSPLINE`~`LINEAR` | `CUBICSPLINE`~`STEP` | `LINEAR`~`STEP` |
|---|---|---|---|
| scale | **0.0938** | 0.8438 | 0.7500 |
| rotation (quaternion component) | **0.0828** | 0.3698 | 0.2870 |
| translation (metres) | **6.8000** | 3.4000 | 3.4000 |

**Every pair separates on every path**, so a sampler that ignored the declared mode could not pass. And
**the tightest pair is `CUBICSPLINE`~`LINEAR`**, which is the pair the Hermite question turns on — the case
is therefore sensitive exactly where it must be rather than only where it is easy.

**What this does not yet establish**: these are separations in the channel's own units, not in codes. The
translation figure is 6.8 m and obviously visible; the rotation figure is 0.0828 of a quaternion component
and **must be carried through to a picture difference before the case is scored**, or the clause is met in
the wrong currency.

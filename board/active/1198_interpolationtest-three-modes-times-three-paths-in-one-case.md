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
- [x] **The subject moves**, per `board:1169`'s clause — nine statements and not one. Worst departure from
  the frame-0 pose over the 17-frame grid, per channel: **rotation 1.0000 and scale 1.0000 in all three
  modes, translation 4.0000 in all three**. No channel is static, so no node can pass by standing still
- [ ] **The frame grid and duration are declared**, and every frame is its own cached product

## The oracle's half, built

**`baked_channels` replaces `spherical_rotation_curves` and the old path is deleted**, so there is one
mechanism rather than a rule beside a special case. Every channel is evaluated from the file's accessors
and written as an exact key at every rendered frame; Blender interpolates nothing. Checked against the
pinned asset with a stubbed object model, because none of it needs a renderer:

| | |
|---|---|
| decode | buffers reach the declared **1628 B**; **9 channels, 3 modes × 3 paths**, none left alone |
| write | **30 curves**, 17 keys each, one `update()` each, every key `LINEAR` over frames 0..16 |
| refusals | a node the importer did not name back · a component-count mismatch — **both fire** |
| slerp | constant angular velocity to **7.206e-07 rad**, span ends reproduced to **7.855e-13** |
| unit | every sampled rotation unit to **2.853e-08**, which is the file's float32 and not the sampler |

**Two defects were found in the writing, and both would have rendered a plausible wrong picture rather
than failing.** The grid is Blender **frames** and the sampler is in **seconds** — `frame_start` is 0 and
`fps_base` is 1, so frame *f* is *f/fps*, and the first draft was off by one frame. And `_sampled`
interpolated `LINEAR` **component-wise for every path**, which for `rotation` is precisely the defect the
deleted function existed to remove: the generalisation had reintroduced it one layer down.

## What remains

- [ ] **The case manifest**, which needs `scene.animation` to accept a **set** rather than a single
  `index` — schema and `Parity.cpp`. `Gltf::Pose::Build` already takes a set; the declaration cannot yet
  say so
- [ ] **The separation carried into codes.** The table below is in each channel's own units. Translation
  at 6.8 m is obviously a picture; **0.0828 of a quaternion component is not yet a claim about pixels**,
  and until it is, the mode clause is met in the wrong currency
- [ ] **A deliberately wrong interpolation mode shown red, then green**

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

## Comments

The oracle's half cost two defects and neither was a crash. Both had the same shape — **an arithmetic that
runs, produces numbers of the right magnitude, and describes a different pose** — and neither would have
been caught by the case going red, because a wrong oracle and a wrong engine disagree in the same direction
as a wrong engine alone. What caught them was checking the sampler against properties it must hold
regardless of how it is written: constant angular velocity, unit length, exact reproduction at the keys.

**One of those readings was itself wrong before it was right.** Span-end agreement first measured
2.389e-04 rad and that was the metric, not the sampler: `acos` is ill-conditioned near 1, the stored keys
are float32, and `sqrt(2 × 2.853e-08)` reproduces the figure exactly. Componentwise the agreement is
7.855e-13. *An instrument reading near its own singularity reports the singularity.*

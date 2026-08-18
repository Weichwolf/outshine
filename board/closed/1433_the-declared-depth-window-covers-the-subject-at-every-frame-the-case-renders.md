Type: bug
Area: corpus
Tags: oracle, khronos, instrument

**The declared depth window covers the subject at every frame the case renders**

A case's clip range is derived as `distance +- radius` about the framing centre, and the radius was the
subject's **rest** bounds. For a subject that moves, that window stops containing it -- and the two sides
then stop rendering the same thing, because Cycles honours a far plane and this engine's reversed-Z
projection is infinite and has none.

[MEASURED] on `AnimatedTriangle`, whose node turns a half circle at the second frame of its grid:

| vertex, at t = 0.5 s | depth along the view axis |
|---|---|
| (0, 0, 0) | 5.6058 m |
| **(-1, 0, 0)** | **6.3755 m** |
| **(0, -1, 0)** | **5.9478 m** |

against a declared far plane of **5.7570120486026148 m**. Two of three vertices behind it, so the oracle
rendered a **1431 px sliver** hanging off the one vertex still inside, where the whole triangle is
14405 px.

| `AnimatedTriangle`, frame 1 | before | after |
|---|---|---|
| `coverage_fraction_oracle` | 0.0015527344 | **0.015630425** |
| `coverage_fraction_outshine` | 0.015630425 | 0.015630425, untouched |
| `worst_disagreement_px` | 39.843127 against a floor of 0.005 | **0** |
| `iou` | 0.099340507 | **1** |
| `pixels_disagreeing` | 12974 | **0** |

## Which side was wrong, settled by a third thing

Rasterised on the CPU from the file's own POSITION accessor, the file's own quaternion at t = 0.5 --
`(0, 0, 1, 0)`, a half turn about z -- and the manifest's own camera, the mask differs from **ours by 0
pixels** and from the oracle's by 12974. *The same instrument that settled `board:1432`.*

**And the picture said it first.** The reference is a thin red wedge where the subject is a triangle; a
half turn about the view-facing axis cannot foreshorten anything, so no rotation explained the shape and
the remaining candidate was a cut.

## A clip range is a depth window and never a crop

So the framing distance and the aim stay the rest pose's -- re-framing on the swept bounds would shrink
the subject and weaken the very motion the case exists to measure -- and only the window opens. The
radius the window takes is **the union of the subject's bounds over the DECLARED GRID**, which for this
case is `[-1, -1, 0]` to `[1, 1, 0]` and a radius of 1.5811388301 m about the aim, against the rest
pose's 0.7071067811865476.

## The finding lived in a report for as long as the case was red

`triangles outside the depth range, unattributed = 1` was already printed, in the attribution table --
which is reached **only when pixels already disagree**, so a case whose cut part landed where both sides
were empty printed nothing at all. It is now a metric of its own, asked of every case, bounded at zero,
and computed from the geometry and the clip alone.

[MEASURED] exactly **one log in the tree** carried a nonzero count, and it was this case.

## Comments

`board:1421` narrowed this case to *frame 1 covers ten times what the oracle covers* and refuted the
axis-convention reading against six green object-carried rotations. Both stand: the axis was never the
question, and the tenfold ratio is `14405 / 1431`.

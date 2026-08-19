Type: bug
Area: render
Tags: instrument

**A transmissive pass composites over the opaque scene instead of erasing it**

A subject carrying one sheet of glass draws **the glass and everything behind it**. `SceneTransmissive`
carries coverage in its alpha like every other target whose alpha is read, so a pixel no transmissive
fragment touched composites as *nothing in front of it* rather than as *everything behind it is gone*.

## What it was, and it is one entry in one list

`CompositeTransmission` computes `behind.rgb * (1 - front.a) + front.rgb`. `SceneTransmissive` was
cleared to **alpha one**, so at every pixel the glass did not cover, `1 - front.a` is zero and the
opaque scene was multiplied away. **A chess set rendered as sixteen pawn tops floating in the dark**,
because the pawn tops are the only glass in it.

The clear's own comment argued the case out loud and had it backwards -- *`SceneTransmissive` is
premultiplied with its own coverage in alpha and is composited by a stage that reads it*. Premultiplied
is exactly why the uncovered value must be **zero**: alpha is coverage, and nothing had covered it.
`board:1423` had already made this argument for `SceneHdr` and the same sentence was written one target
short.

## What it measured, over a declared population

1280x720, 102 480 samples on a 3x3 stride, counting samples carrying any ink.

| subject | before | after | what appeared |
|---|---|---|---|
| `ABeautifulGame` | **258** | **9213** | a board, thirty-two pieces, their textures and their shadows |
| `GlassVaseFlowers` | 4327 | **9340** | the second vase, the flowers and the leaves |

**The after figure equals a control that zeroed every transmissive row and dropped the pass entirely** --
9213 and 9340 to the sample -- which is what says the repair restored the opaque scene rather than
merely brightening something. **And the glass is still glass**: the stems read through the dark vase in
the same picture.

## Why no corpus case could see it, and that is the finding behind the finding

**Every render case declares `renders.default.bounces.transmission` = 0**, so `DeclarePlan` never asks
for the transmissive pass and the composite never runs. The defect lived in the one arm no oracle
comparison reaches, and 147 criteria over 148 cases could not have found it.

**A scenario run found it in its first hour**, which is the argument for `board:1457` stated as a
measurement rather than as a plan: an instrument that renders what a PLAYER sees, rather than what an
oracle can be asked about, reaches code the corpus cannot.

## The round that found it, because the route is worth keeping

**Seven hypotheses were measured and refuted before the right one.** The lighting (a thousandfold lux
moved the mean from 0.49 to 0.60 and the covered-sample count not at all), the environment, the display
transfer, the base-colour textures, the metallic-roughness default, the shading arm (all 49 parts
reported lit), and the plan's requested outputs. **Each refutation narrowed the population** -- the last
one left was *the visible parts are exactly the transmissive ones*, and that named the composite.

**Two of the refutations were defects in the instrument rather than in the engine**, and both would have
been filed as engine bugs by a round that did not seek the harmless explanation first: a batch count read
after the scenario had already been torn down (0 draws, when 41 batches and 49 draws had stood), and a
picture compared against a different frame of the same animation.

## Comments

`board:1386` records that `TransmissionOrderTest` went from 2.52 px of silhouette disagreement to
60.15 px when the transmissive reader was restored, and reads that as the oracle's recipe drawing glass
opaque while this engine saw through it. **That case renders at zero transmission bounces and therefore
never declared this pass**, so this defect is not an explanation for it -- but the shape of the
complaint is close enough that the next round to touch `board:1386` should re-measure rather than
inherit the reading.

Type: bug
Area: render
Tags: oracle, instrument
Depends: 1138

**The picture bound scores pixels the two sides disagree about the identity of**

The router's rule is *a pixel both sides agree is covered goes to the perceptual tail; a pixel they
disagree about goes to the geometric bound*. **Agreement about coverage is not agreement about what is
there**, and a pixel where both sides draw a different surface is routed to the perceptual tail, where its
difference is scored as a colour disagreement it is not.

**[MEASURED] at `coverage/negative-scale`, pixel (717, 274)** — read from `oracle.raw` and `outshine.raw`
in the case directory:

| | linear | sRGB codes | what it is |
|---|---|---|---|
| oracle | `0.5, 0.5, 0.5` | `187.516` on all three | the manifest's `LabelMat`, verbatim |
| ours | `0.0891927, 0.1792562, 0.64` | `84.247, 117.419, 209.350` | the manifest's `BackgroundMaterial`, verbatim to eight digits |
| alpha, both sides | `1.0` | — | both sides are covered, so the router keeps it |

**Two exact declared material colours is the fingerprint of a surface swap and not of a shading
disagreement.** The 5x5 neighbourhood shows the label strip's edge one pixel further right on the oracle's
row than on ours; every other pixel of the strip agrees exactly. **This single pixel is all three of the
case's failing channels** — `board:1136` counts three, and here they are.

**Why it is a bug and not a missing feature.** The router exists, it claims to route each pixel by its
kind, and it routes this one. Its predicate is too coarse, and the tree already says so: the
`materialIndex` pass was added with the justification *the picture bound asks `is this pixel covered` when
the question is `WHAT covers it`, and a surface swap read as 209 codes for want of this*. The 209 is this
pixel's blue channel. **The pass was produced and the reader was never written**, so the diagnosis has sat
on disk unread since.

**The caveat, sought and cleared.** *Is it simply an antialiasing difference?* No: the oracle renders one
sample per pixel through a 0.01 px box filter, which is a point sampler by declaration, and our rasteriser
samples the pixel centre — so both are answering *which surface covers this centre*, and they answer
differently. That is a **rasterisation/edge** question and belongs to the geometric bound, against the
0.005 px instrument floor, which is where the router was built to send it.

**What would be right instead.** The routing predicate becomes *the two sides agree about the surface*,
with the identity read from the oracle's index passes (`board:1138`) and our own draw. A pixel where the
identity disagrees is a **coverage** finding, scored geometrically and counted, never averaged into a
perceptual tail. **The population must be published before and after**, and it must be shown to be the
same selection — a routing change moves pixels between two metrics, which is precisely how a number can
be broken without moving.

**Done when** the deciding pixel of `coverage/negative-scale` is scored as what it is, the count of
identity-disagreeing pixels is published per case, and no case's perceptual tail is decided by a pixel the
two sides disagree about the identity of.

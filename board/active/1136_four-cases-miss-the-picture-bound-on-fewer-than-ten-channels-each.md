Type: bug
Area: render

**Four cases miss the picture bound on fewer than ten channels each**

The tail is a max, so a picture exact almost everywhere fails on whatever handful is worst. Counted from
the histogram, **four of the fourteen failing cases exceed `6.4354338` codes on fewer than ten channels
in ~2.6 million**:

| case | channels over the bound | what decides it |
|---|---|---|
| `coverage/negative-scale` | **3** | one pixel, all three channels |
| `texture/texture-coordinate-test` | **4** | two pixels — `board:1133` |
| `materials/scifi-helmet` | **6** | two pixels, all three channels |
| `materials/a-beautiful-game` | **9** | three pixels, all three channels |

**Taking these four would move the suite from 20 of 35 within the bound to 24 of 35**, and none of them
is a broad shading disagreement — the rest of each picture is already inside.

## What the deciding pixels look like

`coverage/negative-scale`, one pixel at (717, 274): the oracle is **187.516031 on all three channels**,
a neutral grey; ours is `(84.2470147, 117.418834, 209.349859)` — strongly coloured, and straddling the
oracle's value rather than sitting under it.

`materials/a-beautiful-game`, three pixels at (626,347), (541,368), (583,391): the oracle is
`(181, 157, 112)`, `(178, 165, 112)`, `(178, 165, 112)` — **bright and warm**; ours is `(40.2, 49.7,
47.5)`, `(38.5, 48.5, 47.5)`, `(39.3, 49.3, 48.3)` — **dark and near-neutral, and nearly the same value
at all three**. That is the signature of a **specular highlight the oracle has and we do not**: warm
because it carries a metal's F0 tint, isolated because a sharp lobe covers little, and identical across
the three because what we draw there is the diffuse term alone.

`materials/scifi-helmet`, two pixels: ±14 codes, one darker and one brighter, then a drop to 2.36 for
everything else.

## What is established and what is not

**Established:** the counts, the coordinates, the values, and that the remainder of each picture is
inside the bound. **Not established:** that the `a-beautiful-game` signature is specular — that is read
off three pixels' colour and wants the surface's own roughness, metalness and light at those points
before it is a finding rather than a reading. `negative-scale` and `scifi-helmet` are not yet shown to
share it.

**The first thing to build is the reason this is a reading and not a finding:** nothing in the render
suite can say what a named pixel's surface row and incident light were. Every question here — is it
specular, is the lobe missing or misplaced, is the light reaching it — is one query against a shading
point, and there is no way to ask it.

## Comments

**2026-08-14** — Found with the worst-eight-channels table added under `board:1133`. Before it, each of
these cases showed one worst pixel and nothing about whether the rest of the picture was near or far —
`scifi-helmet` reads identically at 15.457417 whether six channels fail or six hundred thousand do.

**THE SPECULAR READING IS REFUTED, AND SO IS THE INSTRUMENT IT ASKED FOR. NONE OF THESE FOUR CASES
SHADES.** Counted from `outshine.normal.raw`, which this suite already writes: `a-beautiful-game`,
`scifi-helmet`, `negative-scale` and `texture-coordinate-test` each carry **0 non-zero shading normals of
921 600**, against 294 876 for `normal-tangent`. Their manifests declare `light: none`, so `Lit()` is
false on every part and the emitted arm runs; and each lowers the oracle's material to `kind: emission`,
so Cycles has no closure with a lobe either. **Both sides compute `declaredRadiance x baseColour(u, v)`
and nothing else** — there is no lobe to be missing, no roughness, no metalness, no light to arrive and no
occlusion ray. A probe publishing the surface row the BRDF received would have printed an empty frame on
all four, and the round that built it would have learned that afterwards.

**What the deciding pixels actually are, measured from the files in the case directories.**
`negative-scale` (717, 274): the oracle holds the manifest's `LabelMat` verbatim (0.5 on all three) and
ours holds its `BackgroundMaterial` verbatim (0.0891927, 0.1792562, 0.64) — two exact declared colours,
both sides covered, the label strip's edge one pixel apart. That is a **surface swap**, and it is
`board:1144`. `a-beautiful-game` (626, 347), (541, 368), (583, 391): the bright warm value is present one
pixel away **in our own render too** — 0.4621 at (627, 346) on both sides — and 0.44520125 is the oracle's
value at two of the three, so it is **one texel reached by one side and missed by the other**. The uv
field's central difference at all three is 0.043 to 0.277 per pixel, which is tens to hundreds of texels:
the discontinuity population `board:1130` already characterised. `scifi-helmet`'s two deciding pixels sit
at 0.14 and 0.34 per pixel of uv difference; its two **larger** disagreements, 28.263 and 26.885 codes at
(667, 502) and (539, 457), are ours-covered-against-oracle-empty and are already routed away.

**So the question is which surface and which texel, not which term** — and the instrument is `board:1137`
with its children. The shading-point probe survives inside it, corrected: it publishes the two **terms**
the light loop summed rather than the inputs it consumed, and its subjects are the **eight** cases that
declare a light, which is where `board:1126`, `board:1130`, `board:1131` and `board:1132` live.

**The reading was not careless and that is worth recording**: bright-warm-against-dark-neutral at three
isolated pixels really is what a missing highlight looks like, and every step to the refutation went
through a file that was already on disk. What the reading skipped was the case's own declaration.

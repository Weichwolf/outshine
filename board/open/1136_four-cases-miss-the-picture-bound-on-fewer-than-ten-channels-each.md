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

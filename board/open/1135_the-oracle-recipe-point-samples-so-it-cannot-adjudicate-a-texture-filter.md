Type: issue
Area: render
Tags: oracle, instrument, khronos

**The oracle recipe point-samples, so it cannot adjudicate a texture filter**

**The decision is which cases render their oracle with an integrating recipe.** It changes every affected
case's oracle cache key and its render cost, so it is not mine to take.

## What was measured

`render/texture/four-texels-per-pixel` was built for this: the corpus's only case where every covered
pixel minifies, at **exactly 4 texels per pixel** by derivation, 64×64 px, frame fraction `4096/921600`
confirmed by the runner. Rendered against two oracle recipes and with our mip chain both ways:

| ours ＼ oracle | 1 sample / 0.01 px box | 256 samples / 1.0 px box |
|---|---|---|
| **single level** | **0.190 codes — PASS** | 37.864 — FAIL |
| **mip-filtered** | 37.412 — FAIL | **5.340 codes — PASS** |

**And the oracle disagrees with itself by up to 37.880 codes between the two recipes**, over 267 of
12 288 channels — nearly six times the picture bound of `6.4354338`.

## What it means

**Mipmapping is correct and was being judged by a point sampler.** Every case manifest in this tree says
so in its own words: *"One sample per pixel with the box filter at its RNA minimum makes Cycles evaluate
the same predicate a centre-sampling rasteriser does."* That is exactly right for deciding COVERAGE and
exactly wrong for deciding a FILTER — under it, any integration the engine performs is a departure from
the reference.

**So every "the chain makes it worse" number in `board:1130` was measuring the recipe.** `normal-tangent`
229.33018 → 255, `texture-coordinate-test` 10.295625 → 202.0679, `scifi-helmet` 15.457417 → 81.229848:
all cases where our filtering departs from a reference that does not filter. The mip chain, the index
rule and Toksvig's term were each judged against an oracle with no opinion on any of them.

## The options

| | |
|---|---|
| **A — an integrating recipe for cases that minify** | per case, which is what the manifest already is: a delta over declared defaults. Only a handful minify at all — measured, `simple-texture` reads **no** level above 0, `normal-tangent` about **920** px, `scifi-helmet` about **5 791** |
| **B — an integrating recipe everywhere** | one rule, no per-case judgement about what "minifies enough". Costs 256× the oracle render for the whole corpus and re-keys every cached oracle |
| **C — two oracles per case** | the point-sampling one decides coverage, the integrating one decides appearance. Strictly more information; doubles the corpus and adds a second cache, which this tree has one of on purpose |
| **D — leave it and never enable the chain** | keeps today's numbers and permanently declares a correct feature wrong |

## CORRECTION: the constraint is harder than the version first written here

**The paragraph that stood here claimed `four-texels-per-pixel` was "exempt by construction" and cited
`picture_max_delta_code_alpha` **0** and `picture_pixels_routed` **0** under the integrating recipe.
The first number was false and was written before its own output was read.** It is `0.99609375`, and
the metric FAILS. Recorded rather than edited away, because a wrong number published with confidence is
the failure this board exists to catch.

**The true measurement, same case, same subject, only the recipe differing:**

| recipe | picture bound | alpha, bound 0 |
|---|---|---|
| 1 sample / 0.01 px box | 0.190 — PASS | **0 — PASS** |
| 256 samples / 1.0 px box | 37.864 chain off · 5.340 chain on | **0.99609375 — FAIL** |

**And no construction exempts a case.** `0.99609375` codes is an alpha difference of exactly `1/256`:
one sample of 256 landing on the far side of an edge. Halving the filter to a 0.5 px box changes it
not at all. The quad's edges lie at `607.5..671.5`, half a lattice step from every pixel centre — the
best case the family admits — and it still happens, because the quantity is the SAMPLE COUNT and not
the footprint.

**So an integrating oracle cannot produce a binary coverage predicate, and this tree's alpha bound is
exactly 0 on purpose** — *a predicate has one value where the two sides agree it applies*. The two
requirements are incompatible in one render.

## The recommendation, revised by the above

**C, and it is cheaper than it first looks.** The point-sampling recipe decides COVERAGE, an integrating
recipe decides APPEARANCE, and neither is asked a question it cannot answer. **The manifest already has
the shape**: `renders` is a map of NAMED recipes, `prepare.py` takes `--recipe` and repeats, and the
runner already reads named companions beside `oracle.exr` for the quantity passes. A second appearance
recipe is the same mechanism, not a second cache.

**A is now the weaker option** and the reason is worth stating: it would put a case in a state where its
coverage metric fails for a reason that has nothing to do with coverage, which is the instrument-domain
failure this whole item is about, reintroduced one level down.

**D remains the one to reject explicitly.**

**Where this leaves the case today:** `four-texels-per-pixel` carries the integrating recipe and fails
TWO metrics — the picture bound at `37.863681`, which is `board:1130` and correct, and the alpha
predicate at `0.99609375`, which is this item and is the instrument. **Both failures are true and
neither is silent**, which is the state a case should be in while the decision is open.

## Comments

**2026-08-14** — This is the fourth assumption this stretch has killed by measuring it, and the
largest: the previous three were mine, this one was the tree's.

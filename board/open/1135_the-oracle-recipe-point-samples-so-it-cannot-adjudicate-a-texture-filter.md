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

## The recommendation

**A.** The recipe is already per case and per recipe name, the minifying population is measurable rather
than a matter of taste, and the cost falls only where the question is asked. **D is the one to name and
reject explicitly**, because it is what the tree does today by default and it would make an instrument's
blind spot into an engine limitation.

**Worked around, not waited on:** `four-texels-per-pixel` already carries the integrating recipe — it is
a new case and its whole purpose is the filter — so it fails today at `37.863683` and names `board:1130`
as its cause. That is one case's worth of the decision taken where it was unambiguous.

## Comments

**2026-08-14** — This is the fourth assumption this stretch has killed by measuring it, and the
largest: the previous three were mine, this one was the tree's.

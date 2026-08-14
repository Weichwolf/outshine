Type: bug
Area: corpus
Tags: oracle, instrument, khronos

**The index passes name the first camera-ray hit, not the surface that coloured the pixel**

Cycles writes `materialIndex` and `objectIndex` at the **first camera-ray intersection**. Our own material
lowering puts a **Transparent BSDF on `Backfacing`** to implement glTF's front-face rule, so where a face
is culled the ray passes through it and colours the pixel from whatever stands behind — while the index
pass keeps naming the face it passed through. **The index and the picture then describe two different
surfaces at one pixel, and nothing says so.**

**[MEASURED] on `coverage/negative-scale`, over 47 532 covered pixels:**

| | |
|---|---|
| the oracle's index agrees with the oracle's own picture, bit-for-bit | **47 343** |
| the index names `ChecksAndXMaterial` while the picture carries `BackgroundMaterial`'s declared colour | **189** |
| anything in between, at any tolerance down to 1e-6 | **0** |

**The predicate needs no threshold**, which is what makes this a mechanism rather than a tolerance
question. The 189 lie in a **3–4 px strip along the left edges of `PositiveScaleTest` and
`NegativeScaleFront`** — the silhouettes where a culled back face is the first thing a camera ray meets.

**THE COST OF NOT KNOWING IT WAS ALREADY BANKED.** The naive count of disagreeing pixels on that case is
**190**, and **only 1 is about us**. `board:1144` exists to reroute pixels the two sides disagree about
the identity of; had 190 shipped it would have rerouted **189 pixels on a false premise**, and the
reroute would have looked like a repair.

**Why this is ours and not simply the oracle's.** The Transparent-BSDF-on-`Backfacing` arm is **our
preparer's** lowering, not Cycles' own behaviour, so the interaction is between a construction we chose
and a pass semantics we did not. Both halves are ours to declare, and the tree's rule is that the oracle's
limitations are **measured and declared** rather than discovered per round.

**What would be right instead.** The population is exactly identifiable — the index names a surface whose
declared colour is not the pixel's — so it is **excluded by predicate and counted**, never by threshold,
and the count is published beside the attributable one. `board:1138` already publishes both
(`surface_oracle_index_unlike_its_own_picture` beside `surface_identity_disagreeing_attributable`); what
is missing is the **declaration**: the limit stated once, with its mechanism, so a later round reading a
disagreement in an index pass knows what it can and cannot conclude.

**The prediction this makes, which is the next thing to measure.** `materials/alpha-blend-mode` carries
**21 739** identity disagreements and they are **currently unattributable**. A blended surface is
transparent to the camera ray by construction, so the same mechanism predicts most of them are
oracle-side. **If they are not, the prediction fails loudly and the finding is a real disagreement on a
blended case** — which is worth more than the prediction holding.

**Done when** the limit is declared with its mechanism and its measurement, the population it explains is
excluded by predicate and counted, `board:1144` routes only pixels that survive it, and
`alpha-blend-mode`'s 21 739 are attributed one way or the other.

## CORRECTED: the prediction is refuted and the title's mechanism is not what was measured

**The prediction failed.** This item predicted that `alpha-blend-mode`'s 21 739 disagreements were
*largely oracle-side*. Tested over all 21 739 rather than the eight samples the prediction was built on:

| oracle's index | ours | px |
|---|---|---|
| `MatBed` | `MatBlend` | 21 130 |
| `MatOpaque` | `MatBlend` | 575 |
| `MatCutoffDefault` | `MatBed` | 34 |

**21 705 of 21 739 — 99.84 % — are COMPOSITE**, not oracle-side. `MatBlend` is the file's one `BLEND`
material, and where a blended surface covers a pixel the colour is a composite of several fragments while
**neither** single-valued index pass answers the same question. Ours names the last writer; the oracle's
names one intersection; the picture names a weighted sum of both. **Three quantities, one pixel, and the
disagreement is a category rather than a defect on either side.**

**AND THE MECHANISM SENTENCE IN THE TITLE IS WRONG AS WRITTEN.** *The index passes name the first
camera-ray hit* predicts `MatBlend` for those 21 130 — the blended surface is in front. The pass says
`MatBed`, **the surface behind**. On `coverage/negative-scale` the same pass names the **front culled
face**. **One sentence cannot cover both observations**, so it is withdrawn as a mechanism and kept only
as the label of the population it was measured on.

**What survives, and it survives intact**: the `negative-scale` measurement — 47 343 bit-exact agreements
against **189** where the index names `ChecksAndXMaterial` and the picture carries `BackgroundMaterial`'s
declared colour, **nothing in between at any tolerance to 1e-6**. That population is real and its
predicate needs no threshold. **What was wrong was generalising a mechanism from one case to another
without measuring the second** — the failure this item's own first paragraph was written to prevent.

**What Cycles actually writes into an index pass at a transparent or culled surface is UNESTABLISHED, and
it stays unestablished here.** The round that tested this measured that it is *not* the first camera-ray
hit and deliberately did not invent a replacement. **An unknown named as unknown is worth more than a
second sentence that fits two data points.**

**The third category is now counted and it is neither side's defect.**
`surface_identity_disagreeing_composite` is computable from the file's own `alphaMode` alone — a static
property of the declaration — so it is **never NaN and never a threshold**. That is the right shape: a
pixel is excluded because of what the material *is*, not because of what the numbers *did*.

**This item's `Done when` is therefore re-stated**: `alpha-blend-mode`'s 21 739 are **21 705 composite and
34 open**, and it is the 34 that need attributing.

**And the statement was too narrow by one case.** `texture/texture-settings-test` carries **77** identity
disagreements nobody had counted — **73** of the culled-back-face shape this item measured on
`negative-scale`, plus **4** of the shape `board:1144` routed. It belongs in this item's population and is
added here rather than filed as a fourth item about one mechanism.

**Two of thirty-five cases can say nothing at all**: `normal-tangent` and its mirror publish **no**
identity metrics, refused at `provenance.json` — which is `board:1154`, already filed, now with a measured
cost in cases rather than in megabytes.

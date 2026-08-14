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

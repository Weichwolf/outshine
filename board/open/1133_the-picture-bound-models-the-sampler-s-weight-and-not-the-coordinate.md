Type: issue
Area: render
Tags: oracle, instrument, khronos

**The picture bound models the sampler's weight and not the coordinate**

**The decision is whether the sub-texel term of the picture bound is complete.** It changes verdicts, so
it is not mine to take.

## What was measured

`texture-coordinate-test` is outside the bound at `10.295625` codes, and **four channels in 2 672 379
are all that exceed 5 codes.** Their values name the mechanism:

```
worst 1:  10.295625 codes at (582, 210) ch 0, ours 10.2956252 against 0
worst 3:   8.143920 codes at (560, 225) ch 0, ours 18.4395447 against 10.2956252
worst 6:   4.222382 codes at (708, 259) ch 0, ours 33.5323505 against 29.3099681
```

**Both sides produce the same discrete set** — `10.2956252`, `18.4395447`, `29.3099681`, `33.5323505`
appear as ours in one row and as theirs in another. The two renderers agree on the values and disagree
about which pixel gets which, at a texture with hard steps. **There is no systematic offset**: four
channels out of 2.67 million rules that out.

The arithmetic closes exactly:

| | |
|---|---|
| our value at the worst channel | `0.003125` linear = **0.8 × (1/256)**, one full weight quantum on a texel step of 0.8 |
| in codes | `255 × 12.92 × 0.003125 = 10.295625` — the observed number, digit for digit |
| the bound's term | `255 × 12.92 × 2⁻⁹ = 6.43476562` — **a HALF quantum** |
| ratio | **1.6, exactly** |

## Why the term cannot be widened by arguing about the weight

The device's sub-texel precision is not assumed, it is measured by
`test/shader/TheSamplerSnapsSubTexelWeightsToTheDeclaredCount`: **8 bits, 257 distinct weights, uniform
step `0.00390625`, endpoints 0 and 1, at both 2 and 512 texels.** 257 levels spanning `0..1` is
round-to-nearest, whose error is bounded by half a quantum — which is exactly what the bound already
carries. **So weight snapping alone cannot produce what is observed, and the term is right about the
thing it models.**

What it does not model is the **coordinate**. The interpolated `uv` is a float computed from a different
arrangement of operations than Cycles uses, and near a texel boundary a difference far below the weight
grid decides which pair of texels is blended at all. On a smooth texture that is invisible; on a hard
step it is a full quantum. **The bound's domain is the sampler's weight, and the failing pixels are a
coordinate disagreement.**

## The options

| | |
|---|---|
| **A — add a coordinate term, derived** | state the interpolated coordinate's own error and carry it beside the weight term. Honest and it is the missing physics; it needs a derivation nobody has written, and it widens the bound for every case |
| **B — leave the bound and fix the engine** | treat our coordinate as the wrong one and match Cycles' arrangement of operations. It is the top rung of the ladder, and it may be unreachable: two correct float evaluations of one expression need not agree |
| **C — route these pixels like coverage** | a channel where the two sides sit on opposite sides of a texel step is already the *disagree about coverage* situation one level down, and the router exists. Narrow, and it needs a predicate that does not also swallow real error |
| **D — disqualify per (case, metric)** | the last rung, and premature while A has not been attempted |

## The recommendation

**A, and only after the derivation exists.** The evidence says the bound is incomplete rather than
wrong: it models one mechanism correctly and is silent about a second that dominates at hard steps. But
*the number was right and about something else* is this repository's named failure, and answering it by
widening a bound until a case passes is that failure wearing the other face. **So the recommendation is
to derive the coordinate term first and let it land where it lands** — if it comes out below `3.86`
codes, `texture-coordinate-test` still fails and the finding is that our coordinate is genuinely wrong.

**This is filed rather than waited on.** It blocks nothing: the derivation is work that can start now,
and the cases it would affect are failing for this reason today either way.

## Comments

**2026-08-14** — Found by adding two instruments that did not exist: a count of channels where the
oracle's radiance is exactly zero, and a table of the eight worst channels with their coordinates. The
first refuted a hypothesis — the *we add light where the oracle has none* class is real but touches
**0.0 %** of every picture, 2 to 756 channels against ~2.6 million, and is empty in all 20 cases within
the bound. The second is what made the mechanism readable: one worst pixel could not show that both
sides carry the same discrete values.

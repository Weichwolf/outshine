Type: bug
Area: render
Tags: oracle, instrument, khronos

**A self-consistency metric never asks the oracle the same question**

`normal-tangent`'s `region-compare` metrics put two rectangles of **our own render** side by side — a
516-triangle dome cell against the normal-mapped quad that imitates it — and require them to agree to a
p95 relative of `0.1579751`. The bound is well made: the geometric mean of two **measured** populations,
the MikkTSpace basis at `0.02245..0.07423` against the same basis with handedness negated at
`0.33618..0.88663`, separated by 4.53× and overlapping nowhere. Nothing about it is fitted.

**But both populations were measured on an engine that did not filter.** Enabling the mip chain moves
the metric to `0.27571386`, and that is not a defect appearing — it is what filtering a normal map does.
A filtered normal is flatter; the geometry it imitates does not flatten. The two cells are the same
surface stated two ways, and **only one of the two statements is a texture**.

**So the metric cannot currently tell "we broke it" from "this is what filtering does", and the reason
is that its population is one engine.** Cycles renders these same two cells, under its own filtering,
and nothing in this tree has ever asked what ITS two cells measure. That number is the discriminator:

| the oracle's own two cells | what it would mean |
|---|---|
| near `0.0224..0.0742` | the oracle does not flatten, and our filtered quad is wrong |
| near `0.2757` | filtering flattens for everyone, and the bound is stale for a filtering engine |

**What to build:** report every `region-compare` on the **oracle's** pixels as well as ours, beside each
other, `reported` rather than bounded. It is the same rectangle pair and the same arithmetic on an image
already in memory, so the cost is a second call and no second cache.

**Why it is a bug and not a task:** a bound derived from one engine's two populations is presented as a
property of the subject, and it is a property of the subject **and that engine**. That is the
instrument-domain failure this repository names in one sentence — *the number was right and about
something else* — in its **population too small** face. The metric decides `board:1130`'s shipping
question today and has no standing to.

## Comments

**2026-08-14** — Found while measuring `board:1130`. Enabling the chain moves five rows of this metric
from `0.13879225` to `0.27571386` while the picture bound does not move at all: **20 of 34 cases within
it with the chain readable, 20 of 34 with it clamped to level 0**, and no test changes verdict either
way. So the whole shipping decision for mipmaps currently rests on this one metric, which is exactly the
weight it cannot carry until the oracle answers the same question.

## Built, and the answer is the first row of the table

`Evaluate` already took one frame, so asking the oracle the same question is the same call on the other
image: `ScoreStatedInvariants` now repacks the oracle's `RawF32` to the stride `LinearFrame` reads and
publishes every invariant a second time as `oracle_<name>`, **reported and never enforced** — the oracle
is what our pixels are judged against, not a second subject with thresholds of its own.

**Unfiltered, over the fifteen cells that carry radiance** (the third pair of every row is a black metal
and both sides are exactly 0, which the manifest already declares vacuous):

| cells | ours | the oracle |
|---|---|---|
| `normal-tangent` pair1 × 5 | 0.03156 … 0.05676 | 0.05677 … 0.07886 |
| `normal-tangent` pair2 × 5 | 0.09429 … 0.13879 | 0.05547 … 0.08692 |
| `normal-tangent-mirror` left × 5 | 0.07073 … 0.09593 | 0.04859 … 0.07369 |

**Two findings, and the second is the one that was worth building this for.**

**The oracle never reaches zero, and the bound's positive population does not contain it.** That
population was `0.02245..0.07423`, measured on this engine; the oracle exceeds its top end on four of
fifteen cells. The bound at `0.1579751` still separates and is not stale — but it was derived from a
population of one engine, and the reference renderer would have fallen outside the half of it labelled
*correct*. That is the defect this item names, demonstrated.

**And filtering does NOT flatten for everyone.** With the chain readable ours goes to `0.27571386` where
the oracle sits at `0.08692` on the same cell — **3.2×**. Cycles filters this texture too. So the answer
is the table's first row: **the oracle does not over-flatten, and our filtered quad is wrong.**

**The reason is not the kernel, it is the order.** Cycles takes many rays per pixel, shades each against
a normal sampled at its own differential, and averages the RADIANCE. We average the NORMAL and shade
once. `average-then-shade` is not `shade-then-average`, and the gap is exactly the perturbation
renormalising throws away — which is Toksvig's term and nothing else.

**This refutes the objection recorded in `TexelChain.h`**, which is why it is written here rather than
quietly dropped: that comment says carrying the shortfall as roughness *"would move us away from the
thing we are measured against to make a number smaller."* Measured, the oracle is not below us — it is
**3.2× below our filtered result and near our unfiltered one**, so the roughness term moves us TOWARD it.
The reason not to take it is gone.

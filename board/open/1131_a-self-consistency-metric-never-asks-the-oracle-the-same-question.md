Type: bug
Area: render
Depends: 1130
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

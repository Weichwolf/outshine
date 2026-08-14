Type: issue
Area: corpus
Tags: oracle, perf, scope

**How many times an animated case declares**

**The decision.** A still is one frame and one oracle render. An animation has a duration, so a case must
declare *when* it is judged — and the count multiplies the corpus directly: at the measured **8.17 s**
per oracle render, the difference between one time and eight is 8× the Blender cost and 8× the products
for every animated case in `board:1128`.

**The options.**

1. **One declared time per case**, chosen where the motion is most diagnostic. Cheapest, and it catches a
   sampler that is wrong everywhere — which is the common failure. **It cannot catch a sampler that is
   right at the sampled instant and wrong between**, and `CUBICSPLINE` is exactly the mode where that
   happens.
2. **A small declared set — three or four times per case**, at a keyframe, between two, and at the ends.
   Catches interpolation error where it lives, at three to four times the cost.
3. **A sweep over the duration.** Catches everything and costs the duration in oracle renders per case;
   at a hundred animated cases this is hours of Cycles per corpus rebuild.

**DECIDED by the owner: frame by frame, oracle and ours, breaking at the first failing frame.**

**It inverts the cost argument and is better than the recommendation below.** A sweep was priced as the
expensive option because it multiplies the corpus by the duration — but with an early exit **the full
sweep is paid only by a case that passes**, and a failing case stops at its first bad frame. **That frame
is also the most diagnostic**: later frames inherit the divergence and say less about its cause than the
one where it began. And it removes the failure mode option 2 was a compromise about — a sampler right at
the instant sampled and wrong between cannot hide when every frame is checked.

**What it costs, stated rather than discovered**: a green animated case costs its whole duration in
oracle renders at the measured 8.17 s each, and the corpus grows by the same factor. The early exit
bounds only the red ones. That is the trade the owner took, and `board:1128` carries it as acceptance.

**The recommendation that was refused, kept for the record: option 2, and only for the interpolation cases.** The mechanism cases
(`AnimatedTriangle`, `AnimatedCube`) are answered by one time; `InterpolationTest` and the
`CUBICSPLINE` cases are the ones where between-keyframe error is the whole point, and a single sample
there would be an instrument whose domain is narrower than its claim. **Everything else takes one.**

**Not blocking.** `board:1128` can build its mechanism cases at one declared time under option 1 and add
times to the interpolation cases when this is decided — so this issue is filed and worked around rather
than waited on.

Type: bug
Area: render
Tags: oracle, instrument, khronos

**A picture verdict that turns on a sub-pixel tie**

`coverage/negative-scale` is decided by a **coverage tie at 0.00026 px**. It has now flipped **twice in
one day for two unrelated reasons** — FAIL 103.269 → **PASS 0** with `picture_pixels_routed` 2 — and
neither flip was caused by anything about the picture.

**A verdict that turns on one sample landing on an edge is not a measurement of the engine.** The
quantity being decided is *which side of a boundary a pixel centre falls on*, at a quarter of a
thousandth of a pixel; the answer is a property of the arithmetic path, not of the renderer's
correctness. **The case can be made to change its verdict by anything that perturbs a float in the
projection chain**, and two such things arrived on one day.

**This tree already knows the class and declares it unenforceable elsewhere.** `board:0088` names *a
sampling-policy difference between a rasteriser and a path tracer* among the criteria that are **reported
and not enforced**, for exactly this reason: our rasteriser answers *is this pixel centre inside* and
Cycles answers *did this ray hit*, and at an exact edge the two are a coin toss with no correct side.

**So the routing is the repair and not the threshold.** `board:1144` established that a pixel the two
sides disagree about the identity of belongs to the **geometric** bound against the 0.005 px instrument
floor, not to the perceptual tail. **A tie at 0.00026 px is two orders below that floor** — it is not a
disagreement the instrument can resolve, and scoring it in either direction manufactures a verdict.

- [ ] **A coverage difference below the geometric instrument floor is routed and counted, never scored.**
  The floor exists precisely to say what the comparison cannot resolve, and a quantity beneath it must not
  decide a case
- [ ] **The count is published**, as every routed population here is — a case whose verdict rests on two
  routed pixels should say so in the same line that reports it green
- [ ] **What must NOT happen is a tolerance.** Widening a bound until this case stops flipping would be a
  number fitted to a case, and the population is already the right instrument: these pixels are a
  **coverage** question and the coverage router exists
- [ ] **Check whether other cases sit on the same knife edge**, measured rather than assumed: the smallest
  coverage margin per case is one pass over the existing masks, and a case within an order of the floor is
  the same fragility whether or not it has flipped yet

**The caveat, and it clears.** *Is the flip evidence the repairs were wrong?* No — both changes were
independently correct and both moved this number as a side effect. **That is the point: a verdict a
correct change can flip in either direction is measuring the change and not the picture.**

**Done when** no case's verdict is decided by a coverage difference below the geometric floor, and the
population beneath it is counted where the verdict is published.

Type: task
Parent: 1442
Area: harness
Tags: instrument, scope

**A reftest pair is decided bitwise, and an empty renderer cannot pass**

The runner renders both declarations of a pair with this engine and compares them **exactly**. There is
no oracle in this suite: both sides are ours, so a difference is a difference and no threshold, no
percentile and no perceptual bound has any business here.

- [ ] a `match` pair holds when the two pictures are bit-identical, and the count of differing pixels is
      published when it is not
- [ ] a `mismatch` pair holds when they are **not** identical -- *this is the half that a renderer
      drawing nothing fails*, and it is why the suite is not built from `match` alone
- [ ] a handful of **anchored** cases, ours and declared in this tree, state where a box lands in pixels;
      they are what keeps a self-consistent renderer from being called correct
- [ ] the two counts are published side by side -- **pairs held** and **how much of the suite the subset
      reaches** -- and quoting either as "the suite is green" is the defect this shape exists to prevent
- [ ] one process per pair and a real verdict per pair, which is `test/run.sh`'s own rule

## It is red before the renderer exists, and that is the point

The harness is built first and every pair fails. **A corpus that arrives after the capability measures
nothing on the way**, and this tree already knows what that costs: the picture corpus is what drove the
renderer, not the other way round.

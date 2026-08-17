Type: bug
Area: render
Tags: perf, instrument

**No change is ever priced, because nothing holds a frame-cost baseline**

`board:1182` took the pipeline count from **30 to 48**, added two interpolants and one `mix` per texture
tap, and **no p50/p95/p99 was taken before or after.** `test/outshine/frame/TheVisibilityTermIsPricedPerRay` is
green — and it is a **pass/fail against its own declared bound**, not a comparison across a change.

**So the suite produces a distribution and keeps no baseline, which means no change in this engine has
ever been priced.** Three feature rows landed this phase and the cost of all three together is unknown.
**The fourth constraint is already the least measured of the four** (`board:0058`), and this is the
mechanism by which it stays that way: every round can be green and the frame can drift indefinitely.

**The gap is not an instrument — it is a comparison.** The clock, the moving camera, the 240 timed frames
and the sanitiser-free path all exist. **What is missing is the previous run's numbers**, and this
repository already refuses the obvious wrong answer to that: **a stored baseline file is a second place
for the truth to live**, and a threshold that moves without a diff is the failure it names on its front
page.

- [ ] **The distribution is published per run in a form a later run can be compared against**, and the
  comparison is made **by a person or by a declared gate**, never by a number the suite quietly updates
- [ ] **The population is stated with it, as everywhere else here**: which subject, which path, how many
  frames, which arm. A frame number without its subject is `board:1146`'s defect in the time domain
- [ ] **The instrument's own floor first.** Two runs of one unchanged binary differ by some amount, and
  **until that is published no difference between two commits can be called a regression.** That
  measurement does not exist and it is the first thing this item owes
- [ ] **`board:1157`'s source digest is what identifies the code a distribution belongs to** — the binary
  hash quoted beside frame numbers this session was a timestamp, so **a baseline keyed on it would have
  been keyed on nothing**

**Why now rather than at the world case.** `board:1162` will measure a world scene against 16.67 ms, and
it will be the first number anybody argues about. **A baseline that starts then has nothing behind it**;
one that starts now covers the rows phase 1 has left to deliver, which is where the cost is being
incurred.

**Done when** a run publishes its frame distribution with its population and its instrument floor, and a
change of the pipeline count or the shading path can be shown to have cost something or nothing.

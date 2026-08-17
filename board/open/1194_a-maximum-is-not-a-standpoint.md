Type: bug
Area: render
Tags: perf, instrument

**A maximum is not a standpoint**

`test/outshine/frame/TheVisibilityTermIsPricedPerRay` **searches for the coverage maximum** along its camera path
and reports a distribution over what it finds. [MEASURED]: at the scale it lands on, **the cheapest frame
of the path costs 0.389 ms against a p50 of 2.031 ms — a factor of five inside one arm.**

**So the arm's own population is not one population.** A p50 over frames that differ five-fold in cost is
a statistic about a search, not about a standpoint, and a change that moved *where the maximum is* would
move the number without moving the cost of anything.

**`board:1187` records the refutation in its own text and repaired its own instrument. The sibling is
unrepaired**, and it is the one the fourth constraint has been quoted from all phase — *10.0 % of budget
for one light on a 23 358-triangle subject* is a number taken from this arm.

- [ ] **A frame cost is a distribution over a DECLARED path, not over a searched one.** The path is the
  declaration; the maximum is a property of the subject and moves when the subject does
- [ ] **If the maximum is genuinely what is wanted**, then it is one frame and it is reported as one —
  *the worst standpoint on this path costs X* — with no percentile over a set the search assembled
- [ ] **The within-arm spread is published either way**, because a five-fold range inside one arm is the
  finding whether or not the statistic changes: **a reader who saw 2.031 ms had no way to know the same
  arm contained 0.389 ms**
- [ ] **`board:0058`'s quoted numbers are re-derived after the repair or marked as taken from a searched
  population.** They are the only frame numbers this project has, and they must not silently mean
  something else than they did

**Why it is filed rather than folded into `board:1187`.** That item is closed and its subject was the
*baseline* — the ability to compare two commits. **This is a different defect in a different instrument**:
the population inside one measurement, not the comparison between two. Same family, and `board:1146` is
the same shape one level up — *the number was right and about something else*, in its **input set too
wide** face.

**Done when** the frame arms report over a declared path, the within-arm spread is published beside the
percentiles, and no number quoted for the fourth constraint comes from a population a search assembled.

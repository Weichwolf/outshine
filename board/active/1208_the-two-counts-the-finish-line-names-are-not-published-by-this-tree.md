Type: bug
Area: harness
Tags: instrument, khronos, oracle

**The two counts the finish line names are not published by this tree**

The goal this run is judged by states its finish line as **two counts published side by side and neither
quotable as the other** — *Khronos criteria met* and *cases within the picture bound*. **Neither exists.**

## What the tree publishes, measured

| | |
|---|---|
| a case | one `VERDICT`, `COMPARED` or `NOTHING-TO-COMPARE`, plus `CHECKS n FAILURES m` |
| `test/run.sh` | a test count — `N tests: … PASS … FAIL` |
| anywhere | **no criterion count and no picture-bound count.** `git grep -i 'criteria met'` over `test/` and `src/` returns nothing |

**So a case cannot say *the asset's criterion was met* separately from *my pixels are within the
bound*.** It has one verdict, and the run has one number over tests — which is exactly the conflation
the two-count shape was written to prevent: *quoting either one as "the suite is green" is the defect
this shape exists to prevent*.

## Why this is a bug and not a missing feature

**A number has been reported all run that this tree cannot produce.** *Khronos criteria met at 30 of 35*
appears in the goal and has been repeated in every round's summary, and **no instrument here derives
it**. `CLAUDE.md` names this failure exactly: *over that length a claim nobody re-measured becomes a
fact*, and *a number an agent reports is re-measured before it is committed*.

**And the denominator does not match the tree either.** [MEASURED] today: **33 Khronos case directories**
and 11 grown ones, with criterion kinds **13 `numeric`, 16 `self-describing`, 4 `stated-invariant`**.
Thirty-five is not the count of cases, of criteria, or of any population this tree currently holds — so
even the fraction's bottom half is unattributed.

## What must become true

- [ ] **A case publishes both verdicts, separately**, on its own line and in its own words: whether the
  ASSET's stated criterion was met, and whether its pixels are within the picture bound. Today a
  `self-describing` criterion — 16 of 33 — is *readable from the picture* and has no numeric verdict at
  all, so the first of the two counts needs a shape before it needs a sum
- [ ] **`test/run.sh` sums them into the trailer, side by side**, and neither replaces the test count.
  Three numbers, none quotable as another
- [ ] **The denominator is derived from the tree**, not carried in prose — the population is what the
  corpus holds, and it moves when a case is added
- [ ] **Every round's report quotes the tree's numbers and not the goal's.** *That is the point: the
  reason to build this is that the alternative is what has been happening.*

## Comments

**This was found by trying to check the thing being reported.** Every round of this run has closed with a
count of tests and a restatement of *30 of 35 criteria*, and the two were never the same instrument. The
tests are measured; the criteria are quoted. **A round spent building the second instrument is worth more
than a round spent moving the first**, because the first cannot answer the question the goal asks.

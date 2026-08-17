Type: bug
Area: harness
Tags: instrument, khronos, oracle

**The two counts are published per case and summed nowhere**

The goal this run is judged by states its finish line as **two counts published side by side and neither
quotable as the other** — *Khronos criteria met* and *cases within the picture bound*.

## The first version of this item was WRONG, and the mistake is the one this tree warns about most

It said **neither count exists**. It does not: **every render case already prints both**, and has for a
long time —

```
KHRONOS-CRITERION red
PICTURE-BOUND outside
```

— from `SayBothVerdicts`, over a partition that is a **field of the metric** (`Count::Criterion` or
`Count::Picture`) precisely so that a reporter does not have to match names by hand. It even refuses the
escape hatch: a case whose oracle still estimates prints `not-enforced` rather than `within`, *because a
case nobody can count either way would otherwise be counted as a pass*.

**The claim was made from `git grep -i 'criteria met'`, which is a PHRASE and not the tree's spelling.**
`CLAUDE.md`: *a grep proves a string absent, never a capability*, and *any negative existence claim names
the enumeration it is drawn from*. The enumeration here was one guessed phrase. **The instrument for a
capability claim is to exercise the capability** — one case log would have answered it, and did.

## What is actually missing, measured

| | |
|---|---|
| a case | prints `KHRONOS-CRITERION met\|red` and `PICTURE-BOUND within\|outside\|not-enforced` |
| `test/run.sh` | **matches neither string — `grep -c` returns 0.** The trailer carries the test count alone |

**So the two counts exist per case and are summed nowhere**, which is why no round of this run has been
able to quote them: the numbers are in 33 log files and in no total.

## Done when

- [x] **`test/run.sh` sums both across a run and prints them in the trailer**, beside the test count and
  never instead of it — three numbers, none quotable as another. **One case votes once**, not once per
  arm: `~sanitised` and `~validated` are instruments about the same picture rather than two more
  pictures
- [x] **`not-enforced` is its own column**, the same refusal `SayBothVerdicts` makes one level down
- [x] **A partial run cannot report a total it did not measure.** The counts accumulate from THIS run's
  logs as each case finishes and are never scanned off disk, and the line prints only where a case
  reported one — so a run of the unit tree says nothing about a corpus it never touched, and a run of
  two cases says *of 2* rather than *of 33*
- [x] **The label names its own population.** The first number this instrument ever printed was
  `khronos criteria: 38 met of 44` — 44 being 33 Khronos cases **plus 11 grown ones**, under a word
  that claimed only the first. Split into two lines and caught by reading the number rather than
  quoting it

## What the tree says, the first time it has been able to say it

```
214 tests: 147 PASS  66 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  1 UNPREPARED
khronos: criteria 27 met of 33   picture bound 14 within, 18 outside, 1 not-enforced of 33
grown:   criteria 11 met of 11   picture bound 11 within, 0 outside, 0 not-enforced of 11
```

**And the two counts are visibly different instruments, which is the whole reason for the shape**: 27
criteria met against 14 pictures within the bound. *Criteria count features and do not fall because our
picture is not the reference's; the picture bound counts pictures.* A single number would have hidden
thirteen cases that implement what Khronos asks and do not yet draw it to the bound.

**The prose this run has been quoting is replaced by measurement.** It said *30 criteria met of 35* and
*20 render cases failing*; the tree says **27 of 33** and **18 outside plus 1 not-enforced**. Neither the
numerator nor the denominator was right, and now both are derived from the population that ran.

## Comments

**What made the first version wrong is worth more than what makes this one right.** Every round of this
run closed by quoting *30 criteria of 35* — a number from the goal's prose — while the tree held its own
answer in every case log. **The reflex to check whether a number can be re-measured was correct; the
instrument used to check it was a phrase search, and it reported absence where there was a different
name.** *That is the same failure as the round that nearly consumed the atmosphere chain because a
resource's NAME suggested what it was.*

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

- [ ] **`test/run.sh` sums both across a run and prints them in the trailer, beside the test count and
  never instead of it.** Three numbers, none quotable as another
- [ ] **`not-enforced` is counted as its own column and not folded into either**, which is the same
  refusal `SayBothVerdicts` already makes one level down
- [ ] **A partial run cannot report a total.** The trailer is what says a run happened; a count of
  criteria drawn from whatever logs survive the last run is the `board:1181` hazard in a new place

## Comments

**What made the first version wrong is worth more than what makes this one right.** Every round of this
run closed by quoting *30 criteria of 35* — a number from the goal's prose — while the tree held its own
answer in every case log. **The reflex to check whether a number can be re-measured was correct; the
instrument used to check it was a phrase search, and it reported absence where there was a different
name.** *That is the same failure as the round that nearly consumed the atmosphere chain because a
resource's NAME suggested what it was.*

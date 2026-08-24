Type: bug
Area: test
Tags: gate, performance

**The gate bound states its current population, and the creep is visible before the red**

The gate is green and inside its bound — this item is about the number's honesty and the
trend, filed before the trend files the red.

- **The derivation beside kFastGateBoundMs is stale twice over** (test/run.sh:621-624):
  it says "measured 48.6 s tests + 1.4 s library, 119 tests, 2026-08-22". 1656's own
  close measured 55.7 s / 130. Today [MEASURED, worktree on this machine, no parallel
  load, warm, 2026-08-23]: 132/132 in 62.2 s (library 1.4 s, test wall 40.7 s of it).
  Under parallel review/board agents the same gate measured 86.5 s (round-9 queue
  report) — 3.5 s of headroom against 90 000. The [SET] carries a population that no
  longer exists and NO load condition, though load is worth 24 s on this machine.
- **The trend**: 43.6 s / 118 (1601) → 55.7 s / 130 (1656 close) → 62.2 s / 132 (today,
  idle). Nothing charges a new gate member for its seconds; each lands free and the bound
  pays. Named warm outliers today: TheOraclesExrReadsAsItsRaw 8.2 s,
  EveryDeclaredSuiteResolvesItsOwnSymbols 6.3 s — the latter was 2.1 s at its 1656 cure
  and has TRIPLED since, ADerivedCameraIsTheFramingRuleAndNotAQuotation 5.5 s,
  TheNestRefusesASecondRunner 1.7 s (two nested run.sh audits at ~1.3 s each by design).

Demanded: (a) the comment restates the measurement with its true population AND its load
condition — a bound whose stated basis is false teaches readers to distrust every [SET]
in the tree; (b) the gate prints its headroom on every green run (elapsed vs bound, one
line) so the creep is a published number, not an archaeology exercise across board
closures; (c) the two multi-second claims outliers get looked at once — the Exr raw
proof and the re-grown link audit cost — before the next dozen tests spend the margin.

---

Closed: the bound's derivation is re-[SET] against the present population (55-65 s warm idle
at 133 tests, ~1.5x headroom, 2026-08-23) with the load condition NAMED -- a parallel
reviewer gate in a worktree shares the machine even though the lock keeps it out of the
nest; and every green gate now PUBLISHES its headroom ("gate headroom N ms of 90000"), so
the creep 1656 caught by accident is a number every run states. First print: 25092 ms.

---

## Sharpened by the hourly review, 2026-08-24 -- the outlier this item named has crossed the budget

This item tracked `EveryDeclaredSuiteResolvesItsOwnSymbols` at 2.1 s (1656's cure) then 6.3 s,
warm, and asked that the two multi-second claims be looked at *"before the next dozen tests
spend the margin"*. Measured now, in the cold worktree the hourly review is REQUIRED to run in:

```
TIMEOUT harness/claims/EveryDeclaredSuiteResolvesItsOwnSymbols  120390 ms   (0-byte log)
run.sh: ... MEASURED NOTHING -- it was killed at 120 s before it finished
```

Same worktree, warm, second run: 28/28 PASS in 89 s. The case spends the per-test budget
compiling every declared suite before it can measure anything, so the gate's verdict depends on
whether objects from a previous run happen to be present. Carried forward as **board:1823**.

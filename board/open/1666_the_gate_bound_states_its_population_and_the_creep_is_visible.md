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

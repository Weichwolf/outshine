Type: bug
Area: test
Tags: gate, performance
Regresses: 1601

**The fast gate stands inside its own bound**

board:1601 closed with kFastGateBoundMs = 90000, [SET] at ~2x the then-measured 118-test
baseline of 43.6 s, and the rule "an overrun is a red run". The gate is red now:
[MEASURED] three consecutive warm runs on this machine (2026-08-23, review round 5, no other
load from the reviewer; parallel board agents possible): 130 tests in 91166 / 92940 /
95506 ms, run.sh exits 1 each time. The delta's own closing notes ("129/129 warm",
"68.9 s" in 1580 slice 2) predate the growth and no longer describe the tree.

The cost concentrates: per-test times of the third run name
harness/claims/EveryDeclaredSuiteResolvesItsOwnSymbols at 20070 ms (001c7660's link audit,
landed 2026-08-22 late) and harness/claims/TheOraclesExrReadsAsItsRaw at 10150 ms as the two
outliers; the next test is 6.8 s. A 20-second single test inside a 90-second gate is the
same defect class as a slow frame — run.sh says so itself.

Demanded, one of, on record: (a) the link audit gets cheap enough to stay in the gate
(audit the declaration against the build's own object lists instead of relinking every
suite, or cache by suite-source hash), or (b) it moves to the named-only set and the gate
keeps a cheap declaration-consistency check, or (c) the bound is re-measured and re-[SET]
with its derivation — but not silently ridden over: today every committer sees a red gate
and learns to ignore it, which kills the bound's whole point.

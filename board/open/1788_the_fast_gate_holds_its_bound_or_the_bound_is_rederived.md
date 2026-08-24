Type: bug
Regresses: 1735
Area: test
Tags: gate, bound, slow-test

# The fast gate holds its bound, or the bound is rederived in the open

`test/run.sh` on an otherwise idle machine, 2026-08-24:

```
235 tests: 235 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  0 UNPREPARED  in 455953 ms
run.sh: THE FAST GATE OVERRAN ITS BOUND -- 277572 ms of RUN over the declared 230000 ms
        (builds 178381 ms stood beside the bound)
```

The bound's own derivation stands in `test/run.sh:740-744`:

> Measured: 100.1 / 104.1 / 153.9 s of run, worst 153.9 -> the bound is that worst
> measurement times 1.5 for the machine's own weather = 230000 ms.

**277.6 s is 1.80x the worst measurement the bound was derived from.** The gate exits 1.

## Where it goes

| test | plain | sanitised | share of the bound |
|---|---|---|---|
| `unit/core/AnExactRayAgreesWithTheScanItReplaces` | 9 959 ms | **53 728 ms** | 23.4 % |
| `unit/gltf/ADerivedCameraIsTheFramingRuleAndNotAQuotation` | 11 355 ms | **52 271 ms** | 22.7 % |
| `harness/claims/EveryDeclaredSuiteResolvesItsOwnSymbols` | 36 509 ms | -- | 15.9 % |
| `harness/claims/TheOraclesExrReadsAsItsRaw` | 19 668 ms | -- | 8.6 % |

**Two tests carry 46 % of the whole bound**, and both carry it in their SANITISED arm.

`AnExactRayAgreesWithTheScanItReplaces` compares an exact ray walk against the linear scan it
replaces at `kTriangles = 4096` and `kRays = 4096` (`:154-155`) -- **16.8 million**
ray-triangle tests, every one of them through ASan's shadow memory.

The reviewer measured the gltf case independently at 51 639 ms against my 52 271 ms, so this
is the tests' own cost and not the machine's weather.

## What will be true

1. The regression gate is FAST, which is what CLAUDE.md says it is for: *"the unit mirror is
   the REGRESSION GATE and it is fast; the long driver suites are the sporadic full proof,
   run when named"*. A comparison at 16.8 million cases is the sporadic full proof; the gate
   arm is the smallest population that still holds the claim, and it says which it is.
2. Neither population is chosen to make the gate green. The gate arm's size is derived from
   what it must detect, and the full arm keeps the number it has.
3. If the tests cannot be made to fit, the bound is REDERIVED in the open -- with the new
   measurements written beside the old, and the reason the tree got slower named. A bound
   quietly raised to fit its overrun is not a bound.

## Comments

- 2026-08-24 -- filed as 1784 and RENUMBERED to 1788: the reviewer's round issued 1784 for
  the fitted corridor's radius in the same minute. Two queues drawing from one counter is
  itself a defect, and board:1783 is where it belongs.
- 2026-08-24 -- the reviewer flagged the overrun in his round and correctly declined to file
  it: his measurement ran beside a 63-minute drive holding the main nest, so it was
  CPU-contaminated. This one is not -- the drive was stopped -- and it overran by more.
  `board:1735` is regressed, not merely at risk.

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

## The biggest single item, repaid (2026-08-24)

`AnExactRayAgreesWithTheScanItReplaces` carried **23.4 %** of the gate's bound in its
sanitised arm alone. It is now **2 781 ms** against 50 059.

The split is not "make it smaller". The two arms measure different things:

| arm | what it proves | population |
|---|---|---|
| plain | every ray the tree answers agrees with the scan it replaces; a rare disagreement is found by asking often | **4096 triangles x 4096 rays** = 16.8 M, unchanged |
| sanitised | no read past a node, no unaligned load, no signed overflow in an index -- and those fire on the FIRST wrong access, not the millionth | 4096 triangles x **512 rays** = 2.1 M |

**The tree stays whole in both.** A first attempt cut the triangles to 1024 as well, and the
case objected: the occluded share fell to 50 of 512, under the mixture bar the agreement
proof needs, because a thinner scene is a different scene. The tree is the SUBJECT being
walked; the ray count is the SAMPLE. Only the sample falls.

- **Proving test**: the case itself, which gained `CHECK(built.Depth() > 1, "a flat tree
  exercises no walk")` so the sanitised population cannot be shrunk into meaninglessness.
- **Negative controls**, both run:
  - the sanitised arm given the full 4096 rays -> **22 544 ms**, eight times the cost for a
    memory proof that was already complete.
  - the tree cut to 2 triangles -> both arms red, `a flat tree exercises no walk` and `the
    ray set is genuinely mixed` together.
- Point 2 of this item holds: neither population was chosen to make the gate green. The
  plain arm keeps every case it had; the sanitised arm's 512 is the smallest sample that
  still leaves the occluded share inside the mixture bar the case already enforced.

## The bound holds again -- and what the measurement does not contain

```
235 tests: 231 PASS  0 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  4 UNPREPARED  in 150558 ms
run.sh: gate headroom 141023 ms of 230000 (run 88977 ms, builds 61581 ms beside the bound)
```

| | run |
|---|---|
| before this repair | **245 503 ms** -- over the 230 000 bound |
| after | **88 977 ms** -- 141 023 ms of headroom |

**But 88 977 ms does not include four cases.** `unit/gltf/ADerivedCamera...` and
`unit/gltf/ANodeHierarchy...`, both arms, are UNPREPARED because `board:1789`'s shared corpus
lost their subject. They cost 52 271 + 11 355 ms when they last ran, so the honest figure is

```
88 977 + 63 626 = 152 603 ms of 230 000
```

which still holds, with 77 397 ms of headroom. That arithmetic is the claim, not the 88 977 --
a bound measured with four cases missing is a bound measured on a smaller tree.

`board:1735` is no longer regressed: this run and the arithmetic above both sit inside the
bound the tree derived. `board:1789` and `board:1786` carry the corpus loss.

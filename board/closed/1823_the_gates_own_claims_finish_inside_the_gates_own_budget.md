Type: bug
Area: test
Tags: gate, cold-start, measured

# The gate's own claims finish inside the gate's own budget, from a clean checkout

CLAUDE.md binds the hourly review to its own worktree: *"its gate: run only in its own
`git worktree` -- the main nest is pid-locked"*. A fresh worktree is a cold object tree, and the
first gate run in one is not green.

Measured, worktree detached at 2b2c2f69, nothing warm:

```
TIMEOUT harness/claims/EveryDeclaredSuiteResolvesItsOwnSymbols  120390 ms
28 tests: 26 PASS  1 FAIL  1 TIMEOUT   (the FAIL is this reviewer's own negative control)
run.sh: harness/claims/EveryDeclaredSuiteResolvesItsOwnSymbols MEASURED NOTHING -- it was
        killed at 120 s before it finished
$ ls -la .../harness-claims-EveryDeclaredSuiteResolvesItsOwnSymbols.log
-rw-r--r--  0 bytes
```

Same worktree, second run, warm:

```
28 tests: 28 PASS  0 FAIL  0 TIMEOUT  in 89 244 ms
```

The claim links every declared suite, so on a cold tree it spends the whole per-test budget
compiling before it can measure anything. The zero-byte log is the shape `board:1778` names: a
verdict neither pass nor fail.

## Why this is not "just slow"

The gate's own reasoning is that the fast mirror is the regression gate. A gate whose verdict
depends on whether the machine happens to hold objects from a previous run is not a gate: the
architect's mandated worktree, a fresh clone and any CI runner all see the red, and every one of
them sees it in the claim that audits the build declaration itself -- the claim that would
notice a suite silently losing a source.

`board:1666` already tracked this case's cost (2.1 s at its cure, 6.3 s a round later, warm).
It has now crossed the budget under the one condition the review is required to run in.

## What will be true

- [ ] The claim separates BUILD from RUN: whatever it needs compiled is built by the runner as
      part of the library/suite build (which is not under the per-test timeout), and the case
      itself only reads objects.
- [ ] Or the runner charges build time separately from run time and reports both, so a cold
      case is slow rather than absent.
- [ ] Proving test: a cold worktree, `test/run.sh harness/claims`, 0 TIMEOUT. Negative control:
      the build folded back into the case -> the cold run times out and the trailer names it.

**Closed on the second box, and the first one is what landed.**

`test/run.sh` gained a `.warms` mechanism (board:1833 corrected its failure handling): a file
beside a case names what must stand BUILT before it runs, the runner evaluates it, and the time
lands in the build column where every other compile does.

```
test/harness/claims/EveryDeclaredSuiteResolvesItsOwnSymbols.warms:
  sh test/run.sh --audit-link
```

**Measured in a cold `git worktree`** -- `git worktree add --detach`, its own `$BUILD` under
`$TMPDIR`, nothing warm:

| | RUN time | of the 120 s budget |
|---|---|---|
| with `.warms` | **14.53 s** | 12.1 % |
| without it | 87.99 s | 73.3 % |

**73.46 s move out of the per-test budget into the build column.**

**The control does not reproduce the item's TIMEOUT, and that is the honest result.** The item
measured 120 390 ms at 2b2c2f69; the same case without `.warms` on the same cold worktree today
takes 87.99 s and PASSES. Three things changed between: four dead sources left `src/`, the
machine's file cache differs, and `--audit-link` itself got faster. So the finding this closure
can prove is not "it timed out and now does not" -- it is **the ratio to the budget**: a case
that spends three quarters of its per-test budget compiling is one slower machine away from
measuring nothing, and it now spends an eighth.

Proving test: a cold worktree, `test/run.sh harness/claims` -> `29 tests: 29 PASS 0 FAIL 0
TIMEOUT` with `EveryDeclaredSuiteResolvesItsOwnSymbols` at 14.5 s. Negative control, run: the
`.warms` file removed on a freshly cleared build -> 87.99 s, six times the run time, same
verdict.

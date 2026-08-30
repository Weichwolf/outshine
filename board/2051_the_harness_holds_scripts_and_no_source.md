Type: task
State: open
Area: test
Tags: tooling, cleanup

# The harness holds SCRIPTS, the runner is short, and no source hides in either

**Benchmark** — Unreal: `RunUAT` is a driver; the checks it drives are content or modules, never
code inside the driver. RAGE: the same. **They agree** -- a test runner that contains the tests is
a runner nobody can read.

## What stands today

    test/run.sh                        2340 lines
    test/gate.sh                         62 lines
    test/harness/shared                1167 lines of C++
    test/harness/claims                  29 C++ cases about the TREE, not the engine

Most of `run.sh` is per-suite include paths, source lists, sanitiser flags and link flags -- one
branch per suite, six of which name suites that no longer exist. board:2049 removes the need for
those branches for the render corpora; board:2050 removes them for the script corpora.

## What will be true

- [ ] every script lives under `test/scripts/`
- [ ] `test/harness/` holds no `.cpp` and no `.h`
- [ ] `run.sh` names a suite, builds nothing per-suite that the Makefile cannot build, and fits on
      a screen a reader can hold
- [ ] the 29 claims are re-judged one by one: a claim about the SOURCE belongs in `make lint`,
      which is a script; a claim that needs to run the engine belongs at the door; a claim that
      grades us against ourselves and proves nothing goes, the way 82 cases went in board:2048

## What this does NOT cover

The corpora themselves. This item moves and deletes MACHINERY; a vendor case's verdict is not
touched by it, and if one changes, that is a defect in this item's work rather than a finding.

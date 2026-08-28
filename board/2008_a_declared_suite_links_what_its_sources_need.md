Type: bug
State: active
Area: test
Tags: measured, layering

# a declared suite links what its own sources need, and `outshine/scenario` does not

**Benchmark** — neither engine faces this: `test/run.sh`'s layer declaration is outshine's own
apparatus. **The choice is mine and the standard is CLAUDE.md's**: *the directory IS the
dependency tier*, declared once in `test/run.sh` and enforced by `--audit-layers`. A tier that
names a directory takes every source in it, and every symbol those sources reach has to be
reachable too.

`outshine/scenario` declares `src/base/math src/base/format src/base/spatial ...`. `src/base/spatial`
holds `Cooked.cpp`, which reads a `Geometry` through `PositionsOf`/`TextureOf`. `src/base` -- where
`Geometry.cpp` lives -- is NOT declared, so the link fails:

    "outshine::Geometry::TextureOf(int, int) const", referenced from:
        outshine::Cook(...) in src-base-spatial-Cooked.o
    ld: symbol(s) not found for architecture arm64

Two of the suite's two cases are BUILD, so **the whole suite has been unrunnable**, and
`sh test/run.sh outshine/scenario` at HEAD reads `2 tests: 0 PASS ... 2 BUILD`. It was found while
validating an unrelated change and confirmed against a clean tree with `git stash`, which is the
only reason it is not being blamed on that change.

- [x] `outshine/scenario` builds and its cases run -- `2 tests: 2 PASS`, by declaring
      `src/base/Geometry.cpp` beside the tiers the suite already names.
      proof: outshine/scenario
- [x] the gate runs every `outshine/` suite it does not NAME as uncovered -- it runs
      `scenario`, `geo` and `fuzz` now, 42 cases instead of 23, GREEN in 46s.
      proof: test/gate.sh **This is why the
      first defect stood**: the gate ran `physics content audio`, and `scenario`, `geo` and
      `fuzz` were neither run nor named in the coverage line -- so a suite that could not LINK
      reported nothing to anybody. CLAUDE.md's first named trap is `a gate blind to a path`, and
      the guard it states is *name what the gate does not cover*. Three suites were in neither
      list, which is worse than being named as uncovered: an unnamed suite is one a reader
      believes is covered.

**The measurement that would show I am wrong:** `git stash && sh test/run.sh outshine/scenario`
reads 2 BUILD at HEAD, so the defect is not mine. If the audit already covered this the suite
could not have been red, so the second predicate is what stops the third occurrence.

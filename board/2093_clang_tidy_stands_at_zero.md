Type: bug
State: open
Area: build, all
Tags: measured, gate
Supersedes: 2095, 2119

# `make lint` is green: no finding, no unreached symbol, no unmapped include

**Benchmark** -- Unreal builds with `bWarningsAsErrors`, gates a static-analyser pass, and
enforces include-what-you-use as a named UBT mode; RAGE cannot be read on this and does not get
a column. CLAUDE.md states the rule itself -- *a warning is an error* -- and `make lint` is the
one place it is not yet held.

## Where it stands, measured 2026-09-04, `make lint` at HEAD ae603ff9

```
  clang-tidy findings          107      79 cognitive complexity, 16 magic numbers,
                                         5 swappable, 3 include-cleaner, 4 one-offs
  symbols nothing calls         56      target 0; the baseline file is gone (632d6e70)
  the tree's own headers        IGNORED by misc-include-cleaner (.clang-tidy: IgnoreHeaders src/.*)
```

The complexity tail, largest first: `CompileInto` 197 (`render/plan/Compiled.cpp`), `LayPatchwork`
178, `OsmVector::Parse` 176, `Xml::Parse` 147, `Markup::Read` 129, `Assemble` 118, `LayDown` 115,
`Grounds` 107, `CookDag` 101, `ReadScenario` 100, `ParseValueInside` 98, `Models` 92.

## The solution, and it is three moves rather than a sweep

**Complexity dissolves into a TYPE, not into halves.** Measured over the forty repairs that got
here: the split that pays names what the parameters ARE, and the function shrinks on its own
(`CellGrid` and `LayCutFace` took `GroundYield` from 8 findings to 0 with bit-identical
pictures). `Grounds`, `LayPatchwork`, `Models` and `CookDag` are board:2101, 2115 and 2122's
functions and fall with those items; the parsers (`OsmVector`, `Xml`, `Markup`, `ReadScenario`)
are a table-driven dispatch each -- one `switch` over a declared tag set -- and the render
compiler (`CompileInto`) is a worklist whose cases are one function per resource kind.

**Unreached is a deletion per line.** `python3 test/scripts/unreached.py` names the 56; it is an
UPPER bound (it cannot see the client or a case), so each is checked against both before it goes.
Two are already known: `Live::SkyEye` (dead since the driver app went) and
`Engine::State::TellsWhatCrossed` (board:2092). `Cover` keeps `EdgeM_`, `HasEdge_`, `RunnerUp_`
that `Ground::CoverAt` writes and nothing reads -- three fields and two out-parameters per
ground sample; the decision is that they go, because a class-boundary blend nobody wrote is not
a capability.

**The include check has to be able to SEE.** Ten files in five generator areas use `Yield`,
`Claim`, `Cover`, `Rank` through `Making.h` and include none of them; `.clang-tidy` ignores
`src/.*` so the check is silent about it. The mapping changes first, the count appears, the
includes follow -- a change that looks local to `Making.h` must not break five areas.

## What will be true

- [ ] `make lint` reports 0 findings, 0 unreached, 0 undocumented (the last is board:2131's),
      every guard green
- [ ] Every check still off in `.clang-tidy` carries its count and its reason on the line above
      it; the comment block there contradicts its own list today (six off, "these five")
- [ ] `misc-include-cleaner` maps the tree's own headers and reports 0 over `src/generators/`
- [ ] Proving case: Venice and OldTown render byte-identical across each mechanical sweep; all
      nine once at the end
- [ ] Negative control: one `(float)` cast back and the gate goes RED at 1

## Ruled out, measured

- a fixit is a suggestion about code the tool did not read: `modernize-loop-convert` broke a
  worklist (`Compiled.cpp`) and the gate would have caught it -- THE GATE RUNS BEFORE THE COMMIT
- replacing where a value is WRITTEN without reading where it is READ: `OsmField::Build`'s
  `cx, cy` fetched every tile around (0, 0) and the frame time looked BETTER
- the two checks over the global allocator cannot both be satisfied; `test/lint.sh` counts
  only locations this tree owns and says which five prefixes it drops

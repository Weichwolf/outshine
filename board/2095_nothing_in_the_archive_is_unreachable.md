Type: chore
State: open
Area: all
Tags: measured

# Nothing stands in the archive that nothing reaches

**Benchmark** — Unreal deletes on the day a call site goes and leans on `-Wunused` plus its own
deprecation cycle; RAGE cannot be read on this. CLAUDE.md already states the rule -- *"Delete on the
day you replace"* -- and the tree measures its own compliance and then allows the number to stand.

## Measured 2026-09-01

`test/unreached-baseline` reads **147**, and `python3 test/scripts/unreached.py` names them. Three
were found by hand while doing board:2093 and were not on anybody's list until somebody looked:

| what | how much |
|---|---|
| `src/base/math/Mat4.h` | 74 lines, a whole matrix and vector algebra, NO caller |
| `NormalFromMap.h`'s C++ half | `SuppliedFrame` · `SurfaceBasis` · `SurfaceBasisAt` · `NormalFromMap` · `Facing` · `Normalised` -- only the shader-text loader is ever called |
| `TreeVec3.h` | a second vector algebra, reached only by the tree tier that had no way to see the first |

All three are gone. The baseline is what is left.

## WHAT THE MEASURE CANNOT SEE, before the number is trusted

`unreached.py` reads `build/liboutshine.a` and subtracts what `include/` names. It does NOT subtract
what `src/client/` calls, and it does not subtract what a test case calls. So 147 is an UPPER BOUND
and a symbol on that list is a candidate, never a verdict -- deleting one because the list names it
is how a client loses a function it was using. Every removal is checked against the client and the
cases first, and the baseline falls by what was actually removed.

## Done when

The baseline reads a number every entry of which has been looked at, and each entry that stays names
who reaches it. A baseline may only fall, so it falls in the commit that removes something and never
otherwise.

## Measured 2026-09-02, walking the `Try*` idiom out of the tree

Fourteen methods in ten files were a hand-rolled `std::optional`: `bool TryX(T *out)`. Converting
them made the compiler name every reader, which is exactly what the pointer form does not do -- and
it named four that do not exist:

| unreached | its class |
|---|---|
| `EnuFrame::TryToGeo` | the inverse of the only direction anybody projects |
| `Claim::TryId` | a `std::optional<BodyId>` behind a `bool` and an out-parameter |
| `Cover::TryRunnerUp` | the second-best ground class at a point |
| `Cover::TryEdgeM` | how far that point is from the class boundary |

All four are deleted. `Claim::TryId` took its member with it: nothing read `Id_`, so `Claim` is now
one `Outcome`, `Claim::Of(BodyId)` is `Claim::Placed()`, and `BodyId` lost `friend class
OccupancySink` because the sink no longer mints one -- `BodyRange::Nth` is the only place that can.

**AND ONE THAT IS STILL THERE, WITH ITS REASON.** `Cover` keeps `EdgeM_`, `HasEdge_` and
`RunnerUp_`: `Ground::CoverAt` still WRITES them, so `-Wunused-private-field` is silent, and
`ClassStructure::Evaluate` still computes both to fill them. Three fields and two out-parameters
per ground sample, on a path that runs per generator cell, feeding nothing.

Deleting the readers was bounded and safe. Deleting the writers reaches into the ground classifier
and is a decision about what `Cover` is FOR -- a class row, or a class row with a boundary distance
something was once going to blend across. That decision needs the generator author, not a sweep, so
it is written here rather than taken in passing.

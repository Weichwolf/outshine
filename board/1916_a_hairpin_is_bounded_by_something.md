Type: bug
State: open
Parent: 1499
Area: actor, sim
Tags: measured, geometry, alignment, corner

# A refusal from the fit names the allowance that decided it

**The fit itself is repaired and `Alignment` is GREEN again.** What is left is what the fit SAYS
when it refuses, and one degenerate corner that answers zero instead of refusing.

REPAID at HEAD. `Align`'s ternary search now minimises `FurthestShareOfArc` -- the same quantity
the acceptance at `Alignment.cpp:241` reads -- so a run no longer splits because the objective and
the test measured different things (`src/actor/path/Alignment.cpp:159-160`). The corner bound is
finite everywhere: `JunctionKerbM(halfA, halfB, deflection, shorterLeg)` (`Alignment.cpp:197`)
caps `w/cos(D/2)` at the shorter leg, so the 215 m licence a 178-degree deflection used to buy is
gone, and the formula stands ONCE rather than inline in `CorridorLay`. The inverted derivation in
`ScoreWhereACornerFits` is corrected and the case prints the limits it computes.

## What is still wrong

**The refusal cites a number it did not use.** `src/actor/path/Alignment.cpp:172` prints
`withinM`, the GLOBAL allowance, while the search that produced the radius read the per-vertex
`withinAtM`. A refusal may name only what it measured; today it says 1.5 m where 2.61 m was
applied, so the reader cannot reproduce the decision.

**A full reversal answers zero rather than refusing.** `JunctionKerbM` returns `0.0` when the
deflection is within 1e-9 of pi (`Alignment.cpp:199`). Zero is a legal kerb length, so a caller
cannot tell "no corner here" from "a corner of no extent", and CLAUDE.md puts a refusal that
carries its reason ahead of a sentinel.

## What will be true

- [ ] The refusal names the allowance that decided, with the vertex it belongs to.
- [ ] `JunctionKerbM` answers `std::expected` and refuses the antiparallel case by name.
- [ ] Proving case: one corner walked from 5 to 179 degrees with unequal half widths, the bound
      monotone and bounded, and the refusal at each end quoting the number the code used.


Type: bug
Parent: 1762
Area: test
Tags: claims, threshold-fitted, green-trailer

# Every non-green node carries a citation the claim walks

`EveryColourCitesALineThatSaysIt` is titled EVERY and judges one node in nineteen. Its
threshold is the count that happens to exist.

## Evidence

The walk only sees citations that spell a line number:

```cpp
const std::regex cited(R"(`([^`]+)`\s*\((?:([A-Za-z0-9_]+\.(?:h|cpp)))?:(\d+)(?:-(\d+))?\))");
```
— test/harness/claims/EveryColourCitesALineThatSaysIt.cpp:55

`CLAUDE.md` at HEAD carries exactly four matches, all four on one file:

```
`struct Eye` (World.h:49)
`Refine(const Eye &eye, double nowMs)` (:55)
`EyeInMercatorBand()` (:118)
`const double eye[3]` (:189-195)
```

and the test's bar is:

```cpp
CHECK(citations >= 4, "the map cites the code it judges, and this walk found the citations");
```
— test/harness/claims/EveryColourCitesALineThatSaysIt.cpp:93

**Four required, four present.** Delete `World`'s row from the map and the test goes red for
the right reason; delete any OTHER red or amber node's justification and it stays green.

The CURRENT class-structure diagram carries 4 red and 15 amber classes (CLAUDE.md:226-228).
One of the 19 is cited. The other three reds are justified by COUNTS, which drift the same
way a line number does and which the regex cannot see:

| node | its stated reason (CLAUDE.md:239-241) | verified at HEAD by this review |
|---|---|---|
| `SubjectDraw` | "six responsibilities in 919 lines" | `wc -l src/render/stages/SubjectDraw.cpp` = 919 — holds |
| `Sim` | "62 public verbs over 59 members and 25 quoted includes" | 62 public verbs, 25 `#include "` in src/clients/Sim.h — holds |
| `Live` | "25 public verbs over 17 members" | ~23 by a crude count — within counting method, holds |

All three hold TODAY. Nothing keeps them holding, and the claim that says it guards them
does not.

This is board:1765's defect on the claims side: a green trailer that names what it did not
judge. The item that commissioned the walk (board:1762) exists because a justification had
gone stale; the walk closes the door on one node and leaves it open on eighteen more.

## What will be true

1. The walk ENUMERATES the non-green nodes from the diagram's own `class …,… wrong` /
   `class …,… unsure` lines and requires a justification row for each — a node without one
   is a FOUND, so the map cannot colour something red without saying why.
2. A count-shaped justification is judged like a line-shaped one: `N lines` against
   `wc -l`, `N public verbs` / `N members` / `N quoted includes` against the file. If a
   number cannot be re-derived by the walk, the map may not use that shape.
3. `citations >= 4` dies. The bar is "every non-green node", derived from the diagram, not a
   constant that equals today's population.
4. Negative control: bump `SubjectDraw`'s 919 to 918 in CLAUDE.md and the claim goes red.
   Today it stays green.

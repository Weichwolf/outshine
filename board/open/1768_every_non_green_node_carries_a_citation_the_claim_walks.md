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

---

## REOPENED HARDER (review 2026-08-24): the repair SHIPS RED

The walk rewritten at b4e9ce04 does not pass. Fast gate, reviewer worktree at HEAD b4e9ce04,
`test/run.sh` with no suite named:

```
233 tests: 232 PASS  1 FAIL  0 TIMEOUT  0 SIGNAL  0 BUILD  0 SKIP  0 UNPREPARED  in 567112 ms
FAIL    harness/claims/EveryColourCitesALineThatSaysIt    323 ms
```

```
NOTE citations judged = 15 citations
NOTE nodes painted red = 2 nodes
FOUND SUBJ is painted red and the paragraph does not name it
FOUND GLASS is painted red and the paragraph does not name it
FAIL test/harness/claims/EveryColourCitesALineThatSaysIt.cpp:126  the diagram paints reds for this walk to judge
       CHECK(nodes >= 3)
FAIL test/harness/claims/EveryColourCitesALineThatSaysIt.cpp:131  **EVERY NON-GREEN NODE CARRIES A CITATION THE CLAIM WALKS**
       CHECK(unjustified.empty())
```

The gate exits 1. The commit that carries this repair was made against a red tree.

### The cause

```cpp
const std::regex painted(R"(\n\s*class ([A-Za-z0-9_,]+) wrong\b)");
std::smatch reds;
...
if (std::regex_search(document, reds, painted)) {
```
— test/harness/claims/EveryColourCitesALineThatSaysIt.cpp:91-95

`std::regex_search` returns the FIRST match. CLAUDE.md carries `class ... wrong` twice: the
render-plan CURRENT diagram (`class SUBJ,GLASS wrong`) comes first, the class-structure
CURRENT diagram (`class TilePool,World,SubjectDraw,Sim,Live wrong`) second. The walk therefore
judges the render plan, whose reds are justified in PROSE ("red = provably wrong (subjects:
six responsibilities, instancing a literal, nothing culls; glass: a full clone of the subject
stage)") and not in the `| \`Node\` |` table the walk demands. `nodes = 2` fails `>= 3`, and
both nodes are reported unjustified for a row that was never supposed to exist.

### What the repair actually has to decide

The item asked for the bar to follow the MAP rather than a fitted count. The rewrite fitted
the map's first diagram instead of the count -- the same mistake at one remove. The walk must
either

- iterate ALL `class ... wrong` lines (`std::sregex_iterator`), and accept a justification
  that is prose naming the node as well as a table row -- one shape of evidence per diagram
  is the map's own form, and forcing every diagram into a table is the claim rewriting the
  document; or
- name WHICH diagram it judges and say so in the CHECK text, and then judge that one
  completely.

Either way the item is not closed by a walk that requires a green tree to be red.

### Still unaddressed from the filing

The title says EVERY NON-GREEN node. The rewrite judges only `wrong`; `unsure` (amber) is
still unwalked, and CLAUDE.md paints 15 amber nodes in the class-structure diagram alone with
no citation obligation on any of them.

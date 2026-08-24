Type: bug
Parent: 1777
Area: test, docs
Tags: claims, drift, map

# A number in the map is recomputed by the walk

`board:1768` said it plainly: *"a reason made of bare numbers is a reason nothing can check"*.
Its repair made every red and amber cite a `file:line`, and
`EveryColourCitesALineThatSaysIt` checks that the cited LINE still carries the cited SYMBOL.

It does not check the NUMBERS standing beside them. The justification table at HEAD asserts,
in prose the walk steps over:

| claim in CLAUDE.md | what nothing verifies |
|---|---|
| `Sim` -- "62 public verbs over 59 members and 25 quoted includes" | three counts |
| `Live` -- "25 public verbs over 17 members" | two counts |
| `Subject` -- "carries 42 `[[nodiscard]]` queries" | one count |
| `Renderer` -- "publishes 52 `[[nodiscard]]` queries and 16 bare `const {` getters" | two counts |
| `SubjectDraw` -- "six responsibilities" | a count that IS cited, six times over -- the good case |
| `AssembleDrive`, `LayCorridor` -- "nine arguments each" | two counts |
| `DriveTick` -- "returns 20 fields by value per tick" | one count |

Thirteen numbers. Every one was measured on 2026-08-24 and every one drifts the moment
somebody adds a getter. A stale count reads exactly like a fresh one, which is the defect
`board:1762` filed against the colours themselves.

`SubjectDraw`'s row shows the shape that works: instead of "six responsibilities" it names
the six entry points at seven `file:line`s, and the walk checks all seven. The others say a
number where they could name the things.

## What will be true

1. Every count in the justification table is either (a) replaced by the citations it counts,
   the way `SubjectDraw`'s row is, or (b) RECOMPUTED by the claim from the file it describes
   and required to match.
2. `EveryColourCitesALineThatSaysIt` parses `N <thing>` out of a justification row and
   verifies it against the cited file -- `[[nodiscard]]`, `const {`, `#include "`, arguments,
   members -- so a number in the map cannot go stale silently.
3. A number the walk cannot recompute may not stand in the table at all; it goes in the board
   item, where a date and a commit stand beside it.

## Comments

- 2026-08-24 -- filed against my own repair. board:1777's table was written by MEASURING the
  ambers rather than remembering them, which was the right move -- and it put thirteen
  unverified numbers into the one document every agent reads first.

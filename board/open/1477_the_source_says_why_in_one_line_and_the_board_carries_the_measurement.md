Type: bug
Area: core
Tags: scope

**The source says why in one line, and the board carries the measurement**

[MEASURED] comment lines as a share of the file:

| | |
|---|---|
| `src/render/stages/SubjectDraw.cpp` | 665 of 2150 -- **30 %** |
| `src/render/Renderer.cpp` | 208 of 885 -- 23 % |
| `src/gltf/Subject.cpp` | 229 of 1288 -- 17 % |
| `src/core/Script.cpp` | 81 of 979 -- 8 % |
| `../softgl/libsoftgl/src/dlist.c` | 4 of 625 |
| `../softgl/libsoftgl/src/lighting.c` | **1 of 313** |

**`CLAUDE.md` already carries the rule and the tree does not follow it**: *default kein Kommentar --
Code und Namen erklären sich selbst; nur ein wirklich nicht-offensichtliches Warum rechtfertigt einen,
dann 1 Zeile, nicht 3-4.*

## Why it drifted, because the reason is a good one and the answer is not "write less"

**The essays carry MEASUREMENTS**, and this file demands those be kept: a number with its population,
a hypothesis that was refuted, the reason a shape was chosen over the one a reader would guess. Deleting
them would lose exactly what the next round needs.

**They are in the wrong place.** `board/` is *the only documentation tree*, and *every statement has
exactly one place*. A measurement in a source comment is a second copy of what the board item says, and
the two drift the moment one side is re-measured -- which is the defect duplication always is here.

## What must be true

- [ ] **A source comment is a non-obvious WHY in one line, or it is not there.** What the code does is
  the code's to say
- [ ] **A measurement lives in the board item that produced it**, and the source names that item with
  the `board:NNNN` marker it already carries -- one id, three views, none a copy of another
- [ ] **The marker is the link.** A reader who wants the round that chose a shape greps one id
- [ ] **No information is lost in the move**, and that is the constraint that makes this work rather
  than a deletion: every number, every refuted guess and every population moves to an item first

## What this may NOT do

**It may not strip a file and call it done.** A comment removed without its content reaching the board
is a measurement destroyed, and this repository has spent whole rounds re-deriving numbers it once had.
*The order is: the item first, the deletion second.*

**And it may not touch a refusal's message.** What a refusal SAYS to its caller is behaviour, not
commentary.

## Done, and one comment turned out to be a declaration

[MEASURED] the sweep: **72 986 C++ lines to 59 400**, 13 586 removed; `test/run.sh` 1096 to 785; 20
Python files; 338 comment lines inside the shader text that lives in raw strings, which the C++ pass
correctly left alone because they are string CONTENT and had to be taken separately.

**`test/unit/compile/*` broke, and it was right to.** Those subjects carry `// ACCEPTED` or
`// REFUSED: <the compiler's own words>`, and `test/harness/shared/Layering.h` READS that line -- it is
the fixture's declaration and not a note to a reader. Stripping it took the expectation out of every
compile subject at once. **A comment a program reads is data wearing a comment's syntax**, and the
lesson is not "keep prose" but that this one has the wrong spelling: the state could sit in the path,
the way `board/open|active|closed` already spells a state, and then nothing would be readable-by-accident.

**27 `Covers("board:NNNN ...")` lines lost their id** and kept their sentence, which is the same rule
facing the report: a test says what capability it covers, not which slip of paper asked for it.

## Comments

The nine `Covers` strings that name `I.26`-style ids are a DIFFERENT taxonomy and were left alone --
they name a requirement in the engine's own numbering, not a work item.


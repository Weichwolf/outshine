Type: issue
Area: test
Tags: process, map, claims

# Every file:line CLAUDE.md cites says what its row claims

CLAUDE.md's CURRENT diagrams justify every red and amber node with a `file:line` citation, and
the review's own charter makes a stale one a defect:

> *MUST be corrected when the code moves -- a node added, removed, renamed, recoloured, or a
> `file:line` citation that no longer says what its row claims*

**Nothing checks it.** `harness/claims/EveryNodeTheMapDrawsIsNamedByAProof` checks that node
NAMES have proofs; `harness/claims/EveryPathCitedInADocumentResolves` checks that PATHS exist.
Neither opens the file at the line.

## The drift rate, measured rather than asserted

At HEAD (2b2c2f69) every one of the 24 `` `<code>` (File.h:NN) `` citations resolves -- this
review walked them all and found none stale. That is not evidence the mechanism is unnecessary:

| | |
|---|---|
| citations in the CURRENT tables | 24 |
| that moved during ONE session's work and had to be hand-corrected in `89f04a28` | 2 (`DriveTick.h`, `CorridorLay.h`) |
| that are stale again in the uncommitted working tree, one hour later | 1 (`DriveTick.h:98` -> the signature now stands at `:108`) |

A citation that must be hand-chased twice in one session is a citation a walk should chase. The
map is green today because a human happened to look; the next reader who does not look reads a
row pointing at the wrong line and forms the wrong picture of the tree, which is the one failure
mode a CURRENT diagram exists to prevent.

## What will be true

- [ ] A claim parses every `` `<quoted code>` (File.h:NN) `` pair out of CLAUDE.md, opens the
      file, and asserts line NN contains the quoted text. A row whose citation has drifted is
      red, named with what the line holds instead.
- [ ] The claim reports how many citations it checked, so a map that stops carrying them is as
      visible as one that carries wrong ones.
- [ ] Negative control: a citation shifted by one line -> red, naming both the row and the line.

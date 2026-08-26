Type: task
State: open
Area: test
Tags: precision, claims, render, measured

# A cited LIST of lines is checked at every line, not at its first

The headline of this item is repaid and I verified it at a73c6ca5 in my own worktree:
`CLAUDE.md:27` now cites `src/render/stages/SubjectDraw.cpp:731,733,736`, the file holds 816
lines, and all three carry the casts the sentence describes. `CitedBy` reads the bare
`` `path:lines` `` form the map writes, and `TheMapCitesLinesThatSayWhatItClaims` passes over a
non-empty citation set.

**What is left is the LIST.** The bare-form parser stops at the first non-digit:

    test/harness/claims/TheMapCitesLinesThatSayWhatItClaims.cpp   digits collected until a
                                                                 non-digit; a following ','
                                                                 only makes the citation legal

so `731,733,736` enters the walk as ONE citation for line 731. Two of the three lines the
precision rule stands on are still unchecked: the file may shrink below 733 or 736 and the guard
stays green. The map's only citation is the one that decides whether a body a thousand kilometres
from the origin has a wheelbase, and two thirds of it are unwalked.

## What will be true

- [ ] A citation naming N lines produces N citations, and each is checked.
- [ ] Proving case: `SubjectDraw.cpp:731,733,99999` -- FAIL, naming 99999. Negative control: the
      parser stopping at the comma, and the same map passes.

Type: feature
Area: core
Tags: instrument

**Every directory under `src/` names the function of what is in it, three levels at most**

**The owner's ruling:** *reorganise every folder under `src/` to at most three levels deep. At every
folder I want to see the FUNCTION of the classes it contains. Subfolders are specialisations.*

Together with `board:1525` -- every module a library on its own terms -- that is a complete rule for
the tree: **a directory is a function, its subdirectories are specialisations of that function, and
its declared include set is the smallest one it compiles against.**

## What must be true

- [ ] **No path under `src/` is deeper than three levels**
- [ ] **Every directory's name is a FUNCTION**, not a category. `src/corridor` says what a corridor
      does; `src/core` says nothing about what is in it and is the first thing to answer for
- [ ] **A subdirectory is a specialisation of its parent's function**, so `src/render/stages` is
      stages OF rendering and reads as one
- [ ] **The dependency graph between directories is acyclic and published**, so *copy this one and
      these* is a finite answer
- [ ] **The move is one round and the suite is green on both sides of it.** A rename that lands
      half-done is worse than the tree it replaced

## Comments

**Not started, and deliberately not started mid-goal.** Moving every file under `src/` touches every
include set in `test/run.sh` and the `Makefile`; doing it while the Munich to Hamburg case is red
would mean two red things at once and no way to tell which moved. It is the first thing at the next
green boundary.

`src/core` is the one that will resist: it is included nearly everywhere, which either means it is
genuinely the shared floor or that it has become a bag. `board:1525` says which of the two is a
measurement nobody has taken.

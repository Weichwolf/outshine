Type: feature
Area: core
Tags: instrument

**Every module is a library on its own terms**

**The owner's ruling:** *every module in outshine must be built so that it would work as a standalone
library -- practically, you can copy any subdirectory into another project and it works on its own.*

**Operationally that is one rule and it is checkable: a module's declared include set is the SMALLEST
set it compiles against, and carrying one it does not use is a defect.** The build already compiles
each directory with its own set, so a name a module must not reach has no spelling in it -- what was
missing is that a set could be too WIDE and nothing said so.

## Where it stands, measured

| module | declared set | on its own? |
|---|---|---|
| `src/corridor` | `-Isrc/corridor` | **yes** -- its own headers and the standard library |
| `src/physics` | `-Isrc/physics` | **yes** |
| `src/pilot` | `-Isrc/corridor -Isrc/pilot` | corridor only, for the reference line it follows |
| `src/world/Wayfinding.cpp` | `-Isrc/world` | **yes** |

All four carried `-Isrc/core` and none of them used it. Removing it broke nothing, which is the proof
that it was never a dependency and only ever a habit.

## What must be true

- [ ] **Every module's declared set is minimal**, and a claim proves it by removing each declared
      dependency in turn and requiring the compile to FAIL -- a set nobody can shrink is a set that
      is true
- [ ] **The dependency graph between modules is acyclic and published**, so *copy this directory and
      these* is a finite answer rather than a hope
- [ ] **A module that only its own tests use is named as such**, because a library nobody outside can
      link is a library only by shape
- [ ] **`src/core` earns each of its users.** It is included nearly everywhere, which either means it
      is genuinely the shared floor or that it has become a bag -- and which of the two is a
      measurement nobody has taken

## Comments

**The strict reading -- zero sibling dependencies anywhere -- is not what this can mean**, because the
engine's whole decomposition is layered: generators read the field, compositors read parts, the
renderer reads a draw list. A module with no siblings could not participate. What the rule DOES mean
is that the dependency is explicit, minimal, acyclic and declared in one place a reader can see, so
that copying a module out is a mechanical act rather than an archaeology.

**The instrument is the removal experiment and nothing weaker.** A module compiling against its set
proves the set is sufficient; only removing a dependency and watching the compile fail proves it is
necessary.

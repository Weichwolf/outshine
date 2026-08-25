Type: issue
State: open
Area: all
Tags: measured, magic-number

# Every number carries its origin, and 191 do not

`--audit-numbers` walks `src/` for named constants whose value is a bare literal that is not 0,
1, a power of two or a half. Those are DECISIONS, and CLAUDE.md asks each to carry its origin --
derived, measured or `[SET]` -- with a unit and a population. A walk cannot read a reason, so it
counts them instead: `DECIDED=191` is declared in `test/run.sh` and the gate refuses the day the
count moves in either direction.

Where they stand, ten heaviest:

    18  src/world/generators/draw/TreeGrower.cpp
    16  src/world/generators/draw/BuildingShape.cpp
    14  src/world/generators/draw/BuildingMesh.cpp
     7  src/render/stages/ParticipatingMedium.h
     7  src/base/math/Units.h
     6  src/content/gltf/Framing.h
     5  src/world/ground/World.cpp
     5  src/world/generators/draw/TreePrototype.cpp
     5  src/render/stages/SubjectDraw.h
     5  src/render/stages/IridescenceLobe.h

The generators carry a third of them, which is where a shape decision lives and where the
argument for each is a paragraph rather than a citation. `Units.h` carrying seven is a different
kind: those are conversions and their origin is a standard.

## What will be true

- [ ] The count falls, one file at a time, and each number moves to where it is DECLARED -- the
      scenario or the client -- or gains a derivation beside it in its board item
- [ ] `DECIDED` falls with it, so the declaration is a measurement and never a target

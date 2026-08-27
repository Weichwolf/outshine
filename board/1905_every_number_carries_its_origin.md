Type: issue
State: open
Area: all
Tags: measured, magic-number

# Every number carries its origin, and 125 named ones do not

**Benchmark** — Unreal: numbers live in data assets with editor metadata and ranges. RAGE: tunables in data files. **Both agree** — a number's origin travels with it, and a bare literal in code has its reason nowhere.

`--audit-numbers` walks `src/` for named constants whose value is a bare literal that is not 0,
1, a power of two, a half or a value naming another constant. Those are DECISIONS, and CLAUDE.md
asks each to carry its origin -- derived, measured or `[SET]` -- with a unit and a population. A
walk cannot read a reason, so it counts them instead: `DECIDED=125` is declared in `test/run.sh`
and the gate refuses the day the count moves in either direction. It fell 195 -> 191 by moving
five engine constants to the door, and 191 -> 125 when the walk stopped counting derived values
(commit 0842e22f, which states the reason).

Where they stand, ten heaviest (STATE.md prints the same list, generated):

    16  src/world/generators/draw/BuildingShape.cpp     5  src/render/stages/ParticipatingMedium.h
    14  src/world/generators/draw/BuildingMesh.cpp      5  src/render/stages/IridescenceLobe.h
     6  src/content/gltf/Framing.h                      4  src/render/stages/SubjectDraw.h
     5  src/world/ground/World.cpp                      4  src/actor/path/ReferenceLine.h

The generators carry a third: that is where a shape decision lives and where the argument is a
paragraph rather than a citation.

## The two blind spots the count does not see

**1. An ANONYMOUS literal is invisible to it.** The walk matches `k[A-Z]`-named constants only,
so the numbers with no name at all -- the ones CLAUDE.md actually calls magic -- are not counted.
Measured at a32c4919: **40 literals with three or more decimals stand inline in `src/*.cpp`**,
among them

    src/sim/CorridorLay.cpp:496              0.477
    src/render/stages/SubjectResidency.cpp:13  0.04045     the sRGB transfer knee, unnamed
    src/world/generators/Forest.cpp:21       2.4494897     sqrt(6), unnamed

A named constant with a bare literal is the SAFER half; this is the other one.

**2. A struct member default in `include/` is invisible to it.** The walk reads `src/` only. The
five decisions that left the engine this session landed as public data defaults in the door and
left the count without leaving the tree:

    include/Scenario.h   PatienceS = 30.0 · RisesBy = 0.35 · WheelStepPx = 48.0
                         MostStepsInArrears = 8 · StepS = 1.0 / 60.0

Their new home is right -- they belong to a scenario -- and their origin is still nowhere.

## What will be true

- [ ] **The shipped body's numbers are in this count** (from board:1509): every spring and
      damper rate DERIVED from a declared ride frequency and the load rather than guessed, the
      eye height confirmed rather than estimated, and each traced to a measurement or a
      published dimension. JSBSim is the bar -- an aircraft's mass, inertia tensor and ground
      reactions are declared numbers with origins, which is how one simulator flies a Cessna
      and a 737.
- [ ] The count falls, one file at a time, and each number moves to where it is DECLARED -- the
      scenario or the client -- or gains a derivation in its board item
- [ ] The walk sees an anonymous literal, so relocating a number cannot lower the count without
      giving it an origin
- [ ] `DECIDED` falls with it, so the declaration is a measurement and never a target

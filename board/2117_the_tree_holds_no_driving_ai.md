# The tree holds no driving AI

State: open

Roughly 2700 lines of autonomous-driving code stand in the tree and are wanted gone. It is reached
from `engine/Advancing.cpp` and `engine/EngineHeld.h` and from nowhere else, and its door type
`Scenario::Journey` is declared by NO scenario in the tree.

## THE WORD `DRIVE` MEANS TWO THINGS HERE, AND ONE OF THEM STAYS

This is the whole difficulty, and a `git rm` over the word would take both.

| what | where | verdict |
|---|---|---|
| `Scenario::Drive`, `Drives::Effort \| Motion` | `include/scenario/Scenario.h` | **STAYS.** This is the DRIVE of a degree of freedom -- CLAUDE.md's own engine vocabulary, beside body, joint, constraint, force, contact. Nothing to do with cars |
| `Sim::DriveProduct`, `DriveAssembly`, `DriveTick`, `Column<Journey> Drives` | `src/sim/`, `EngineHeld.h` | goes |
| `AssetAnimation::Driven` | `include/scenario/Scenario.h` | door change -- decide separately |

The same care applies to `src/base/curve/`: `ReferenceLine`, `Ribbon`, `Fit` and `Alignment` are
ROAD GEOMETRY and `generators/road/RoadMesh.cpp` builds on them, so they stay. `SpeedProfile` and
`Carriageway` describe how fast a vehicle may go through a curve, and go with the driver.

## What is not yet decided

`Sim::Rigging::Stand(const Scenario::Body &declared, ...)` builds a physical body FROM A
DECLARATION, which is exactly what a declarative engine owes a scenario -- but it returns
`Pilot::Axles` and an `Envelope` and includes `SpeedProfile.h`. So the file straddles the line and
has to be read, not swept. Same question for `Sim::Support` / `GroundSupport`, which is "what is
underneath a body" and reads generic.

## What will be true

- no file in the tree plans, steers or paces a vehicle
- `Scenario::Drive` still means the drive of a degree of freedom, and a reader cannot confuse the two
- what remains of `src/sim/` is physics a scenario can declare, or it is gone too
- the door declares nothing no scenario can use

## What will show I was wrong

`make` links and `make shots` reproduces all eight digests bit-identical. If a digest moves, the
deleted code was not dead and the cut was in the wrong place. If the link fails, the boundary is
not where this item says it is.

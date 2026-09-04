Type: bug
State: open
Area: generators, world
Tags: measured, owner
Depends: 2101, 2133

# A building reads the street it FRONTS, and its storeys and door say so

**Benchmark** -- Unreal: PCG samples the road spline and orients and sizes what it places by
distance to it. RAGE: a procedural building faces the road its lot is attached to, and the lot
carries the setback. **Both agree**: the street is an INPUT to the building.

## Where it stands, measured 2026-09-04

`StructureBake::NearestStreet`, `Frontage`, `StandBackM` and `DefaultStoreys`'s `onStreet`
branch exist and are complete -- and `RawTile::Ways` is never filled (`StructureBakes.cpp`,
`RawOf`), exactly as `GroundStack.cpp:128` passed `std::span<const WayLine>()` before it.
`Fronted` reads 0 at every place. A complete capability nothing reaches (CLAUDE.md, question 2).

## The solution

`RawOf` copies the tile's highway lines (the `Ways` layer, half-width from the road class)
beside its polygons; `BakeStructures` already turns them into `WayLine`s. The half-width comes
from the same table board:2133's network reads, so the two cannot disagree.

## What will be true

- [ ] `Fronted` > 0 at OldTown, and the picture moved only where a facade turned to its street
      -- named with `pixels.py` before acceptance
- [ ] Negative control: pass no ways and `Fronted` reads 0 again

Type: debt
State: active
Architecture: ready
Parent: 2139
Depends:
Priority: P1
Area: engine, world, generators
Tags: naming, ownership, runtime

# Engine world orchestration names its actual owners

## Problem and evidence

`src/engine/Asking.cpp` groups four unrelated `Engine::State` operations:
`WhereTheEyeStands` resolved geographic camera focus, `GenerateInitialInstances`
and `GenerateInstancesForRegion` place generated world instances,
`PrepareRuntimeWorld` opens providers and runtime ground, and
`RequestTerrainCoverage` schedules bounded elevation work. The filename
describes none of them. Their private constants/helpers are unrelated.
Changing source selection forces recompilation and review of placement and
camera code. Renaming the file alone would misrepresent the remaining duties.

## Ownership decision

- `WorldFocus.cpp` owns `CurrentGeographicFocus` and its geodesy dependencies.
- `WorldPlacement.cpp` owns the two generator-placement operations, the
  instance-budget diagnostic and snapshot-row constant.
- `RuntimeWorldPreparation.cpp` owns `PrepareRuntimeWorld`, declared-OSM
  layer mapping and relief-source setup. It coordinates providers but does
  not read files, parse OSM, construct navigation or build generator geometry.
- `TerrainCoverage.cpp` owns `RequestTerrainCoverage`, tile-request budgets
  and its zoom constant. It requests work; TilePool owns execution.
- Keep existing `Engine::State` calls and resource owners for this move. Do
  not add forwarding facade, aliases, duplicate state or a new public type.
  Minimize includes per file so naming reflects actual dependencies.

## Falsifiable acceptance

- Remove `Asking.cpp`; each moved operation has exactly one definition and
  callers retain the same input, result, error and publication behavior.
- Groundless declared OSM still queues and publishes. Invalid provider still
  preserves the previous assembly; generator placement and terrain focus
  tests retain their results. A deliberate duplicate or missing definition
  fails the link as a negative control.
- The library and client link; run `make format`, focused engine/world tests
  and `LINT_JOBS=2 make lint` on the completed commit.

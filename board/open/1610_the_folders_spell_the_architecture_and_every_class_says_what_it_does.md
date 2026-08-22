Type: issue
Area: core
Tags: scope, layering

**The folders spell the architecture, and every class says what it does**

The owner's ruling (2026-08-22): src/'s directory names and nesting mirror the architecture
diagram; class names state their function. "Journey" names nothing -- and rightly so: it MIXES
streaming setup, road-network building and drive orchestration, which is the separation-of-
concerns defect its decomposition (1581) is removing. No transitional rename: the name dies
with the monolith.

## The mapping (folders := the TARGET diagram)

| architecture noun | folder today | folder target |
|---|---|---|
| PROVIDERS | src/data | src/data (stands) |
| GROUND (fields) | src/world | **src/ground** -- the diagram's own noun |
| GENERATORS | src/generators | stands |
| COMPOSITORS | (absent, red) | src/compositors when 1538/1595 build it |
| RENDERER | src/render | stands |
| SCENARIOS | src/scenario | stands |
| the scene store | src/scene | stands |
| ACTOR CHAIN | src/physics + src/pilot + src/corridor + src/sim | **src/actor/** -- body/ (physics), mind/ (pilot), path/ (corridor + pathfinding), with the sim systems dissolving into them |
| the doors | src/clients | stands (Engine; Live folds per 1582) |

## Rules

- a folder rename is a git mv + the one include truth in run.sh; quoted includes resolve by
  filename, so sources stay untouched except where they name the moved layer
- the unit mirror renames in the same commit (the mirror claim enforces it)
- class names: no metaphors -- Wayfinding plans, CorridorLay lays, DriveTick ticks, Rigging
  stands a declaration up; anything a reader cannot place from its name alone is a finding

## Slices

- [ ] src/world -> src/ground (the diagram's noun, the cleanest cut)
- [ ] src/actor/ assembles: physics -> actor/body, pilot -> actor/mind, corridor (+ Wayfinding,
      RoadHarvest from ground) -> actor/path -- decided WITH move 2(d)/(e) so the sim systems
      land in their homes once, not twice
- [ ] Journey's name dies when (d)/(e) dissolve its last mixed concern

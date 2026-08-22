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
- [~] src/actor/ assembles: physics -> actor/body DONE, pilot -> actor/mind DONE (git mv +
      the one include truth, unit mirror moved in the same commit, namespaces Physics and
      Pilot stay -- they state function honestly). OPEN: corridor (+ Wayfinding, RoadHarvest
      from ground) -> actor/path, which carries a namespace decision (Ground::Network et al
      must not keep the old layer's name inside actor/path -- the 2x-flagged debt pattern);
      its own sitting
- [ ] Journey's name dies when (d)/(e) dissolve its last mixed concern

---

**Sharpened (review 2026-08-22).** The src/world -> src/ground mv landed, but only the
DIRECTORY speaks the diagram's noun -- every spelling inside the layer still says the old one:
`namespace outshine::World` across all of src/ground (e.g. BuildingField.h:18,
ClassBuilder.h:16), header guards `OUTSHINE_WORLD_*` (RoadHarvest.h:1, Wayfinding.h:1), and
`src/ground/World.{h,cpp}` -- a class named World inside a folder named ground, which is also
the class the architecture adjudication marked red for spelling camera/LOD in the ground
layer. The commit's claim "the ground layer speaks the diagram's noun" is a quarter-truth
until the namespace, the guards and the World class's name follow the folder (the latter
together with its 1595 decomposition, so it is renamed once, not twice).

---

Sharpened (review 2026-08-22, night round): the criticised namespace/folder split now GROWS --
src/ground/GroundStack.{h,cpp}, born this hour (4a320f2e), adopts `namespace outshine::World`
and the guard `OUTSHINE_WORLD_GROUNDSTACK_H` inside the folder named ground. Every new file in
src/ground written before the namespace follows the folder deepens the rename debt; the fix
must sweep one file more than it would have yesterday.


---

The namespace follows the folder (this commit): `outshine::World` became `outshine::Ground`
across the 60 files that spelled it, the OUTSHINE_WORLD_* guards became OUTSHINE_GROUND_*,
and the class-World member qualifications survived intact (Ground::World is the red class,
renamed once with its 1595 decomposition, not twice). Remaining slices: the actor/ folder
assembly (body/mind/path) and the World class's own death.
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
      Pilot stay -- they state function honestly). DONE: corridor -> actor/path with the value
      types staying in plain outshine (the tree's value vocabulary) and Wayfinding carrying
      namespace outshine::Path (Network, Route, ApartM, Plan -- the pathfinding SERVICE).
      RoadHarvest went and CAME BACK: the linker exposed that it reads OsmField -- it is the
      GROUND EDGE (field -> network adapter), so it stays Ground::Reap in src/ground filling a
      Path::Network. The remaining seam, noted for its own slice: harvest should emit neutral
      way data and the network should weave FROM data, so ground stops naming a Path type
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
---

Sharpened (review 2026-08-22, path-move round): d3a51ebb established the rule for ground —
namespace AND guards follow the folder — and the two actor moves of the same hour skipped the
guard half. src/actor/body/*.h still open with `OUTSHINE_PHYSICS_*` (Body.h:1, Contact.h:1,
Rig.h:1, Shear.h:1), src/actor/mind/*.h with `OUTSHINE_PILOT_*` (all six), and actor/path is
three spellings deep: `OUTSHINE_CORRIDOR_FIT_H` (Fit.h:1), `OUTSHINE_CARRIAGEWAY_H`
(Carriageway.h:1), `OUTSHINE_PATH_WAYFINDING_H` (Wayfinding.h:1). The namespaces Physics and
Pilot staying is the recorded decision and stands; the guards spell dead FOLDERS, not
functions, and the ground sweep is the precedent. One pass: OUTSHINE_ACTOR_BODY_*,
OUTSHINE_ACTOR_MIND_*, OUTSHINE_ACTOR_PATH_*.

---

Guard slice verified complete (board queue): the actor sweep landed since the sharpening --
every src/actor header spells OUTSHINE_ACTOR_{BODY,MIND,PATH}_*, and the one straggler
(Angle.h, born mid-night as ANGLE_H) joined the rule. grep for the dead spellings
(OUTSHINE_PHYSICS_/PILOT_/CORRIDOR_/CARRIAGEWAY_/PATH_) over src/ is empty. Remaining before
close: the World class's own death, renamed once with its 1595 decomposition.

---

## Sharpened by the hourly review, 2026-08-25 -- two moves landed and the mirror did not follow

This item's own rule: *"the unit mirror renames in the same commit (the mirror claim enforces
it)"*. Two files moved out of `src/clients/` this session and the mirror moved with neither:

| moved | its twin, at HEAD |
|---|---|
| `src/clients/Species.{h,cpp}` -> `src/generators/Species.{h,cpp}` (`5d1511be`) | `test/unit/clients/AWorldReadsTheSpeciesThatGrowInIt.cpp` -- and it includes `Forest.h` and `Species.h` and nothing else, so every subject it names now lives in `src/generators/` while the case sits under `test/unit/clients/` |
| `src/clients/LogSinks.{h,cpp}` -> `src/core/io/LogSinks.{h,cpp}` (`da2aa9c0`) | none. `grep -rln LogSinks test/` finds one file, `test/render/outshine/world/AWorldStandsUpWhereItIsDeclared.cpp`, which uses it as scaffolding and proves nothing about it. `test/unit/core/io/` holds one case and it is the PNG reader |

The mirror claim does not enforce the rule this item states, and that is the second half of the
finding: `EverySourceLayerHasItsUnitMirror` asks whether a LAYER has a mirror, so a case that
stays in the wrong folder while its subject moves is invisible to it. `src/generators` has a
mirror; `test/unit/clients/AWorldReadsTheSpeciesThatGrowInIt.cpp` is still in the wrong one.

- [ ] `AWorldReadsTheSpeciesThatGrowInIt.cpp` moves to `test/unit/generators/`, where its
      subjects are.
- [ ] `src/core/io/LogSinks.{h,cpp}` gets a twin under `test/unit/core/io/`, or it is folded
      into `Log.{h,cpp}` and covered by that layer's case.
- [ ] The mirror claim is sharpened from per-layer to per-FILE, so a source whose folder moves
      and whose case does not is red -- which is what this item's rule already demands and
      nothing measures.

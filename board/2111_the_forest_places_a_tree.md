Type: feature
State: open
Architecture: ready
Priority: P1
Area: generators, world, engine, render
Parent: 2169
Depends:

# Forests populate suitable ground with shared native quality levels

## Goal and evidence

Dense plausible forests and city trees use source data and deterministic rules, with
no Place-specific paths. Existing TreeGrower/TreeMesher/TreeFoliage/TreePrototype,
ForestDraw, Shipping catalogue, native materials, instanced leaf geometry, crown atlas
and VegetationStreaming are reusable. Do not replace the tree generator.
Current world consumer primarily draws crown cards; placement covers only the eye region.
Near cards, unsuitable scale/species, missing streamed re-entry and incomplete readiness
remain visually rejected. Prior Places waited for 5–30 crown prototypes after terrain/OSM
were ready. Completed preparation is not proof that the next render shares its demand.

## Executable first slice

1. Add one deterministic isolated flat forest scenario under test/outshine/integration,
   with explicit area/density/seed/view distance and near -> far -> near camera path.
   Use one existing species profile and shared TreePrototype, not per-tree meshes.
   Initial fixture: 128x128 m and 64 trees [SET], then raise density against measurements.
   This is a calibration scene, not the final dense-forest acceptance.
2. Connect ForestDraw/WorldPlacement native instances to shared near/mid prototype
   geometry in VegetationStreaming. TreePrototype::InstancedGeometryAt already separates
   bark/leaf geometry and placements. Reuse native geometry and stable species/material IDs.
   Do not expand every leaf into every world tree. Generate outside the frame path.
3. Select valid resident near/mid/far levels by conservative silhouette/coverage criteria
   and hysteresis. A failed far atlas retains native geometry; a missing fine level reports
   quality shortfall. No billboard at the eye and no biome/age change when switching rank.
4. Compare the current far crowns with the new path in motion, backlight and steep views.
   Demonstrate coverage/alpha mip stability. Measure triangles, instances, overdraw,
   shadows, uploads, CPU/GPU cost and prototype/history bytes; open the PNGs.

## Ownership and integration

- Generators own growth, geometry, species and stable source-derived seeds. world owns
  semantic placement/exclusion and versioned geographic cells. engine/streaming owns
  prototype demand, bounded work/residency and handoff; render selects resident products.
- Owners/files: flora/{TreePrototype,TreeGeometry,TreeMesher,ForestDraw,ModelLadder},
  generators/Shipped, engine/WorldPlacement and streaming/VegetationStreaming; native
  renderer instance/piece submission. Preserve generator-library independence and reaches.
- Isolated native proof requires neither completed global streaming nor all material/
  species WIs. 2225's atomic crown publication is required when streamed groups update
  alongside a world candidate; 2184 supplies bounded atlas capture, not native near geometry.
- After isolated proof, add bounded geographic re-entry/eviction and complete versioned
  prototype demand shared by preload/render. Reject stale source and partial publication.
  2282 supplies land-cover evidence; height/slope/climate guide plausible placement.
  Roads/buildings/water remain free through shared semantic data, not render-depth guesses.
- 2314 gives city and forest the SAME scene-render envelope, shared with sky/clouds.
  No fixed vegetation quota. 2176 expands species AFTER the native forest proof; 2171/2167
  own material/lighting improvements without blocking this first native integration.

## Deferred hypothesis

Clustered crown volumes are an optional far alternative. Compare against current crowns
before committing a new volume renderer: edges, clearings, winter, conifers, top views,
transmittance, motion and cost. No voxel/volume framework without a measured advantage.

## Acceptance

- Near/mid/far/re-entry opened PNGs: plausible scale/silhouette, no near cards or popping,
  stable coverage under changing light. Forced near impostor and disabled culling FAIL.
- Source/seed/species IDs survive cell order and motion. Wrong instance matrix, lost
  handoff, stale demand and second-upload refusal FAIL independent controls.
- Dense isolated and world forests report cold/warm/moving p50/p95/p99, overruns, GPU
  passes/overdraw, CPU/GPU bytes and bounded queues. Cheap missing trees cannot pass.
- make format; focused flora/ForestDraw/VegetationStreaming/publication suites; full lint.
  Run/render via outshine-client, then open Koerbersee/Wien/Rosenheim and mixed city/forest
  views. No A18/720p60 claim without target-device evidence.

Konkretes Code-Gate: `make format`; `make suite SUITE='outshine/src/generators/flora/ForestDraw outshine/src/generators/flora/TreePrototype outshine/src/engine/streaming/VegetationStreaming'`;
`LINT_JOBS=2 make lint`. Neue Slice-Orakel liegen bei den genannten Ownern.

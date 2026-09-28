Type: feature
State: open
Architecture: ready
Parent: 2092
Depends:
Priority: P1
Area: render, engine, generators
Tags: budget, vegetation, infrastructure, sky, clouds, lod, measured

# Infrastructure, vegetation, sky and clouds share one render budget

## Goal and evidence

The user requires a city and a forest to use the same render-time budget.
OSM infrastructure, vegetation, sky, clouds and atmosphere compete by visible
contribution and measured cost. No permanent class quotas, separate forest allowance or requirement
that a sparse scene waste time to match a dense one. Same target hardware, resolution,
view conditions and total budget apply to city, forest and mixed scenes. Sky often
covers one third to two thirds of the image [user design target]; measure each view,
never reserve that fraction blindly. Cloud volume, illumination and motion are core
image quality; ground shadows and ambient-light effects matter even when sky is occluded.
Existing StructureCellDetail uses a conservative geometric guard; VegetationStreaming
has shared prototypes and crown products. Neither proves joint cost-aware selection.
FrameMeasurements observes host stage/fence times; these are not per-product GPU timings.

## Ownership and data flow

- render owns resident visual selection and a bounded SceneDetailBudget planner.
  Input snapshots describe visible native products, source generation, resident levels,
  conservative error/coverage benefit, lighting contribution and calibrated costs.
  Class labels do not assign priority or reserved budget. Never call generators in render.
- engine/streaming owns bounded build/upload requests, CPU scratch and residency limits.
  Consume detail commands; infrastructure and vegetation share admission capacity.
  Preserve one drawable valid fallback and retire only after last GPU use.
- generators own deterministic alternatives and their geometric certificates.
  world owns terrain/source evidence and semantic exclusion for roads/buildings/water.
  Simulation/contact/navigation are independent of visual quality reduction.
- Optimize visible quality within one render-time envelope. Account separately for
  geometry, instances, alpha overdraw, materials, shadows, sky and volumetric work.
  Calibrate marginal costs from measurements; missing cost evidence stays conservative.
  Stable ordering and hysteresis prevent oscillation during mixed-scene crossings.
- Same render budget does not imply equal triangle/tree/building counts. A forest's
  alpha/shadow cost and a city's geometric/material cost require different selections.
  Report predicted and measured cost, denied upgrades and actual quality error.
  A budget-constrained fallback cannot falsely report Refined.
- CPU frame, GPU execution, upload bandwidth, GPU/CPU bytes and streaming work remain
  separate constraints. Host render() or fence time alone cannot prove GPU cost parity.
  The total 60 Hz frame target is 1000/60 = 16.6667 ms; measure fixed costs before
  assigning the remaining scene-render share. Sky sampling/resolution and cloud
  march/temporal quality compete with geometry; preserve coherent atmospheric lighting. Local host results are not A18 evidence.

## Executable first slice

Audit StructureCellPlanner, VegetationStreaming and render submission for actual
selection/cost inputs, including SkyStage/SkyPass. Add the native planner with a caller
carrying infrastructure, vegetation and sky/volumetric alternatives.
Define one explicit quality objective and a bounded solver; compare small allocations
with exhaustive independent optima, and measure regret instead of claiming exact global
optimality. No architecture-only counters or fixed equal split as an implementation.
Files: render/SceneDetailBudget.{h,cpp}, engine/streaming/{StructureCellPlanner,
VegetationStreaming}, render/stages/{SkyStage,SkyPass}, engine/FrameMeasurements; tests
mirror their owners.

## Falsifiable acceptance

- Matched city, forest, sky-dominant and mixed-camera traces share the configured render envelope;
  cold/warm/moving p50/p95/p99, overruns, GPU passes, uploads, residency and quality logged.
- Scarce capacity moves to the larger visible gain per calibrated cost regardless of
  class. Swap class labels without changing allocation; unused capacity crosses families.
  Independent views with 1/3 and 2/3 sky must value cloud quality and ground-light effects.
- Under overload lower visual cost while retaining contacts, source correctness,
  fallback coverage and bounded queues. Crossings/backtracking do not thrash residency.
- Negative controls: fixed class quotas, ignored foliage overdraw, optimistic unknown
  cost, ignored cloud/lighting cost, stale source and unbounded requests fail fixtures.
- make format; focused planner/streaming suites; full lint including clang-tidy.
  Render city/forest/sky/mixed scenes through outshine-client, open PNGs, quantify quality
  alongside costs. 2111 still owns plausible near/mid/far vegetation and placement.

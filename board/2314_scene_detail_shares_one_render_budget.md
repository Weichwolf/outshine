Type: feature
State: open
Architecture: planned
Parent: 2092
Depends:
Priority: P1
Area: render, engine, generators
Tags: budget, vegetation, infrastructure, sky, clouds, lod, measured

# Infrastructure, vegetation, sky and clouds share one render budget

## Binding goal

City, forest and mixed scenes share the SAME render-time envelope on the same hardware,
resolution and view conditions. OSM infrastructure, vegetation, sky, clouds and atmosphere
compete by visible contribution and measured marginal cost. No class quotas or extra forest
allowance. Sparse scenes need not waste time to match dense ones.
Sky often occupies 1/3 to 2/3 of the image [user design target]. Measure each view;
cloud volume, illumination and motion are core quality. Cloud shadows/ambient light also
matter when sky is occluded. Pixel area alone cannot rank all visible lighting effects.

## Current boundary and decisions

- Source audit: StructureCellDetail has geometric guards; VegetationStreaming primarily
  publishes crown impostors. SkyStage consumes atmosphere LUTs; there is no cloud renderer.
  FrameMeasurements records host/fence phases, not per-product GPU execution.
- 2092 owns matched traces and cost calibration. 2298/2111/2140 own usable local quality
  ladders. Their COMPLETE WIs do not block measurement/design here; actual unavailable
  alternatives block their integration. This umbrella is not an executable planner task.
- Do not build SceneDetailBudget, a new registry or a generic solver without these inputs.
  Reuse resident product selection and render pass graph. Start with bounded discrete
  choices and stable ordering, then measure whether a common controller is worthwhile.

## Implementation recommendations

1. Capture matched city/forest/sky/mixed traces at 720p, identical time/weather and camera
   pacing. Record source/build/quality settings, visible coverage, complete drawn content,
   CPU frame/host costs, GPU captures, uploads and CPU/GPU bytes. Missing capabilities are
   missing quality, not a cheap-scene victory. Keep static/cold/warm/moving evidence separate.
2. Measure quality/cost ladders: source-keyed building levels; shared native near/mid trees
   and far crowns; cloud resolution, bounded march steps and temporal accumulation. Calibrate
   overdraw, shadows and fixed passes as well as geometry. Local host is not A18 evidence.
3. Define a documented quality score from coverage, contrast/silhouette, image loss and
   temporal stability. Geometric certificates remain correctness bounds; heuristic ranking
   cannot replace them. Cross-family weights require opened reference/variant PNGs.
4. render selects resident alternatives; engine/streaming consumes bounded detail commands
   and budgets preparation/upload/residency. Generators own alternatives/proofs, world owns
   sources and semantic exclusion. Contact/navigation never follows visual degradation.
5. A bounded upgrade plan spends remaining capacity on marginal visible gain per cost,
   with deterministic ties and hysteresis. No fixed equal split. Compare small cases with
   independent exhaustive allocations; report regret rather than claim global optimality.
   Unknown costs stay conservative. Preserve one valid resident fallback per visible product.
   Feasible choices also obey memory, upload and preparation caps (2228); no time-only
   optimum that exceeds residency. Shared prototypes/atlases are charged once at their
   owner, so marginal cost depends on current shared residency, not just instance count.
6. Same budget means different geometry/instance/march counts. A budget-constrained fallback
   reports its quality shortfall and cannot falsely report Refined. Atmospheric radiance,
   transmittance, cloud shadow and history use one coherent state at every quality level.

## Acceptance and open architecture

- Total frame target: 1000/60 = 16.6667 ms. Measure fixed/host work before assigning the
  render share; do not sum asynchronous CPU/GPU p99 or call fence time a GPU pass duration.
- Matched city/forest/mixed and 1/3-/2/3-sky views meet the declared common envelope with
  quality evidence. Cold/warm/moving p50/p95/p99, overruns, passes, uploads and bytes logged.
- Swapping class labels preserves allocation; unused capacity crosses families. Ignored
  foliage/cloud overdraw, lighting cost, stale source and unbounded work must FAIL controls.
- Open decision: target-device GPU measurement/cost interface and cross-family quality
  calibration. Resolve with 2092's captures and 2111/2140's ladders before marking ready.
- Focused owner tests, format/full lint, outshine-client renders and opened PNGs are required.

## Gegenbeispiel zur Ratio-Heuristik

Budget 10 Kosteneinheiten: A kostet 6 bei Gewinn 9, B/C je 5 bei Gewinn 7.
Greedy wählt A wegen 9/6 = 1.5 > 7/5 = 1.4 und erreicht 9; B+C kostet 10 und
liefert 14. Ratio allein ist kein Optimum. Diese unabhängige kleine Fixture muss den
Regret 14−9 = 5 sichtbar machen. Beschränkte Austausch-/Bundle-Schritte erst nach
Messung erwägen; gemeinsame Atlas-/Prototypkosten in beiden Orakeln berücksichtigen.

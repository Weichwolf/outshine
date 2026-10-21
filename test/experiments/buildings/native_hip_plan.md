# Native hip-plan construction and cache-byte comparison

`HipRoofPlanProbe.cpp` prepares the 48 original `reference/hip-plans.json` inputs
and 64 analytic rectangles with both windings. It emits the actual `HipRoofCodec`
bytes, including explicit rejected-plan states. Comparing complete `plans` arrays
between two binaries checks native cache compatibility without rewriting pins.
This does not compare either result with a geometric oracle by itself.

Compile the probe using the full `HipRoofPlan.cpp` compilation-database flags,
replacing only input/output paths and dependency-output paths. Link against the
prepared archive and its GEOS/SFCGAL dependencies. For an isolated replacement,
put the changed `HipRoofPlan.cpp` object before the unchanged archive. Preserve
the old archive before rebuilding. This is a native integration tool, not a
Python researcher implementation or a new library interface.

```sh
"$HIP_PROBE" test/experiments/buildings/reference/hip-plans.json \
  "${TMPDIR:-/tmp}/outshine-hip-plan/result.json" 10000
```

The output directory must exist. Repetitions are bounded to 100,000. Cold-first
and repeated rectangle preparation are separate; serialization and the original
48-case comparison follow the measured rectangle loop. No cache hits, rendering
or GPU times are measured. A checksum and refusal count accompany timing.

The library-disabled unit `RectanglesAvoidSkeletonLibrary` independently proves
that analytic rectangles need no SFCGAL calls. Its complex-input negative control
still enters the unavailable backend. The original implementation fails this
rectangle control before partitioning. Ordinary roof tests continue using the
real library for complex cases, courtyards, clearances, capacities and cache use.

## First native boundary change, #955

On A18 Pro, three interleaved old/new fresh-process runs prepare 10,000 rectangles
each. Median totals: 28.630/6.047 ms; per plan this is total milliseconds divided
by 10,000 and multiplied by 1,000, or 2.863/0.605 microseconds. Ratio 4.73.
Background development load was not isolated. This is a small construction cost,
not a measured scene-load or frame speedup; no whole-app memory saving is proved.

All 112 encoded results are unchanged: 64 valid rectangles, 46 valid original
plans and two existing `InvalidFacet` refusals (`Vienna:112890:2` and
`Vienna:1137690:3`). These refusals are preserved observations, not accepted target
behaviour. The complete SFCGAL replacement must handle the original corpus with
the verified contour method, then pass native consumer, cache, image and cost checks.
No geometry recipe or cache-format version changes for this byte-identical step.

## Complete native contour port, #955

The runtime now uses the typed contour implementation under
`src/generators/building/roof/`. `HipRoofPlan::Prepare` keeps the analytic rectangle
fast path; other footprints use bounded moving-front events, directed contact
pairing, per-original-eave swept facets, shared-node refinement and final
plane/manifold/disjoint-coverage checks. GEOS handles polygon operations; no
SFCGAL call remains in plan construction. The existing precision, clearance,
capacity and deadline limits remain. JSON is diagnostic glue outside the kernel.

The previously library-disabled control is now `PlansAvoidSkeletonLibrary`:
rectangles and a triangular roof succeed with zero skeleton calls. The fixed
57-eave courtyard must succeed with every original eave and the same area;
it can no longer pass by refusing the plan. The original 48 pins and their
comparison tolerances remain unchanged. The actual rebuilt product archive
passes all 48 pins; all 64 rectangle Codec results remain byte-identical.

Prepared building assets use recipe `prepared-building-base-11`, and mesh producer `outshine-building-mesh-20`, because complex
roof geometry and old refusals change. The binary plan format does not change;
existing valid plans remain readable. A recipe miss rebuilds the affected product;
a hit continues loading the stored immutable plan without constructing topology.
This kernel result alone admits neither the complete client, Places, resource
budgets nor removal of the remaining package/build/test dependency. Their actual
terminal results are recorded in #955.

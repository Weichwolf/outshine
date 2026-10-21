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

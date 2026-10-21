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

## Native contour-motion port, #955

`RoofContourMotionProbe.cpp` ports the affine velocities, moving vertex/edge
contacts, edge collapses and simultaneous events of the delivered contour
reference. Two scalar pair sweeps replace three dense collision arrays; the
second sweep collects the events at the minimum time. Report distinct pairs and
the actual two pair visits separately. Topology and facet assembly remain Python.

Compile with the complete existing `HipRoofPlan.cpp` CDB flags, substituting only
the source/object paths. Link the resulting object as a shared library with the
same CDB language standard: `-dynamiclib` on macOS, `-shared` and a PIC object on
Linux. No archive, GEOS or SFCGAL link is required for this isolated motion probe.
The exported `OutshineRoofCollision` JSON bridge is private diagnostic glue; its
returned pointer remains valid until the next call on that thread. It is not a
runtime API. The same source also has a CLI for recorded intermediate inputs.

```sh
PYTHONPATH=lib/src:test/experiments/buildings OPENBLAS_NUM_THREADS=1 \
  python3 test/experiments/buildings/verify_native_roof_motion.py \
  --library "$ROOF_MOTION_LIBRARY" \
  --output "${TMPDIR:-/tmp}/outshine-roof-motion/result.json"
```

The checker uses the actual native events inside the unchanged complete Python
contour/topology/facet pipeline and existing48-original pin checker. Every
construction deadline, source-plane/coverage/source-edge and height-mode check
remains. It also requires identical affine velocities at each invocation. A
normal A18 Pro run passes48/48 originals and1739 calls, including8 courtyards and
the two old native refusals. This is an intermediate native algorithm port,
not a complete native plan, consumer/cache/image or renderer admission.

A separate frozen-state comparison initially finds two different event lists:
one redundant contact at an edge collapse, and a near-simultaneous collapse
included by scalar arithmetic. NumPy matrix products and scalar sums differ by
up to2.84e-14 on these inputs. The whole advanced front differs by at most1.20pm,
below the existing numerical guard; no guard is increased. One additional
stronger all-vertex plane diagnostic fails equally in both producers at an
almost opposing front before topology rebuilding. Its22nm residual is retained,
not used as a false port rejection or hidden by changing a product test.
The complete hybrid pin comparison is the relevant follow-up and passes; full
native topology/facet integration still has to establish that result itself.

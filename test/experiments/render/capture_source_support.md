# Whole-source capture support before fine work

Owner: #1720; native joint capture: #1721; World admission: #1570.
This Python reference tests geometric containment, not visibility or field quality.

SIGGRAPH first: Durand et al., *Conservative Visibility Preprocessing using
Extended Projections* (2000), local PDF
`doc/references/geometry/siggraph/2000-conservative-visibility-extended-projections.pdf`,
sections2.2–2.4/figures2–4, were inspected including the diagrams/equations.
Their conservative projected support/depth reasoning motivates preserving entire
source extents. This experiment does not implement their view-cell/PVS algorithm
or infer occlusion. Fixed orthographic AABB projection here follows directly from
the extrema of a linear function over a box.

Test an existing complete cohort envelope first: one box uses18 directed products
and48B of numeric endpoints. A positive enclosure avoids per-source work. A
negative box result is inconclusive; refine source bounds only when useful, or
retain the parent. In these controls the foreground accepts early; the interleave
has an inconclusive cohort box but all1891 finer source boxes fit. Cohort bounds
belong to existing native metadata; do not scan/rebuild them each frame.
Support belongs to the bound cohort/placement/capture descriptor. Recompute when
those inputs change, and reuse it for presentation-only camera changes that keep
that descriptor. It is independent of lighting. Use existing product metadata/
leases; this reference adds no scheduler or cache.

`capture_source_support.py` borrows a bounded nonempty pair of Nx3 source bounds.
The native-control placement sequence is Double translation followed by Float32
vertex storage. Both operations are monotone, so applying that sequence to
enclosing endpoints encloses every stored translated original vertex. This is
not a general ECEF, instance-matrix or arbitrary GPU-transform model.

Subtract camera eye with outward rounding; for each of three capture axes choose
each coordinate's lower/upper endpoint according to the coefficient sign. Round
each multiplication and accumulation outward separately. This encloses exact
real projection under the supplied finite camera coefficients. Compare all six
capture planes, including near/far. Nonfinite projection and uncertain boundary
touches retain originals. Invalid requests/placement overflow are rejected.

The candidate needs18N directed coordinate products, versus144N for expanding all
eight corners and applying the same interval arithmetic. The independent
`corner_support` work-order opponent returns identical intervals/flags/limits in
all three native controls. Exact-rational tests independently check projections,
including tiny products, rotated axes, large eyes and stored Float32 placement.

## Replacement and native boundary

The camera projection model is Double geometric arithmetic. GPU matrix assembly,
camera rebasing, Float32/FMA/reassociation and raster clipping are separate native
obligations. This reference does not certify their error. A source that passes
geometric containment still needs correct capture samples, source/material/light
leases, pose/error calibration and external-background composition.

Per-source flags plan a new complete-source cohort. If any member is clipped,
the existing whole-group capture cannot replace the entire requested group.
Keeping that member's original while reusing its partial pieces in the old field
would create two owners. Either retain the parent or generate a new source-bound
cohort that excludes every retained original. Changing capture extents/clipping
or membership changes the bound product; an old Raw field is not relabelled.

The two unsupported ordinals3825/3827 remain unresolved per controlled instance.
Cell33 retains its archived opacity assumption. Passing supported source bounds
does not resolve those sources or qualify the complete requested world cohort.

## Existing original controls

The fixed #1716 packet supplies1891 supported source ordinals and complete bounds.
Held metadata occupies98,332B: source IDs plus two Float64 Nx3 endpoint arrays.
The metadata path reads only these lazy NPZ entries, never fine images/geometry.
The diagnostic fixture checksum is verified separately; this packaging is not a
new product-cache format or a cold-IO/device benchmark.

The foreground and interleaved neighbour fit all geometric capture planes.
For the separated neighbour,78 source boxes cross the side boundary and676
cross the far plane; their union710 is retained. All-original primitive checks
confirm702 actually clipped sources (78 lateral,666 far), with no incorrectly
admitted source. Eight extra retentions are conservative box slack. Bounds can
extend102.5338m past the lateral edge and150.2949m past the far plane. These are
controlled placements of complete existing originals, not geographic neighbours.
The original exploratory78 count only tested side planes; tests were explicitly
corrected to the complete six-plane requirement using the primitive opponent.

`capture_support_study.py` validates all original geometry/lineage hashes, compares
the equal-result corner opponent and independently projects every actual stored
original vertex. It records source ordinals and plots support/clipping in capture
coordinates. Geometry is loaded only after metadata decisions, for this opponent.
Plots remain in system temp and must be opened before delivery.

## Reproduction

Use repository `.venv`, existing NumPy/Matplotlib and fixed original-control
dependencies. Set `PYTHONPATH` to `lib/src`, `test/experiments/render`,
`test/experiments/buildings`, `test/experiments/transport`;
`OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`. No new dependency is required.

```sh
.venv/bin/python -B -m unittest test_capture_source_support
nice -n 10 .venv/bin/python -B test/experiments/render/capture_support_study.py \
  --output /tmp/outshine-capture-support/review
.venv/bin/python -B test/experiments/render/capture_support_cost.py \
  --output /tmp/outshine-capture-support/cost
```

Freeze measured source code first. Costs use nine rotating normal-priority
CPU/wall rounds and separate Memray/native-allocation traces. Comparison is
outside timing. Held inputs/opponent precede incremental peaks; total copies,
physical memory traffic, process/UMA peaks and native device/frame times remain
unmeasured. Known Float32 roundtrip copies and corner expansion are explicit.

On67058cbda, Linux/Python3.13.5/NumPy2.5.4/Memray1.20, host load
3.26/4.48/3.78, full1891-source endpoints/corners cost1.577/9.350ms CPU for the
foreground,1.311/9.643ms for the separated neighbour and1.378/9.822ms for the
interleave. Endpoint requested allocation is1.998–2.000MB and incremental peak
0.337–0.338MB; corners request12.170–12.171MB with2.394–2.395MB peak. Results
are identical. These are warm Python function scopes, not native frame times.
The single cohort box costs0.396–0.410ms CPU using endpoints, with71,331B requested
and7,296B scoped peak; this Python overhead is not extrapolated to C++/devices.

Numeric cohort endpoints need48B; the diagnostic still holds the full98,332B
source metadata and derives the cohort envelope outside timing. Endpoint output
is94,598B for1891 boxes versus98B for one box. Known Float32 roundtrip copying is
136,152B for1891 boxes; the corner opponent additionally gathers363,072B of
coordinates. Inputs/held opponents precede peaks; total copies and physical
memory traffic remain unmeasured. Twelve separate raw allocation traces are kept.

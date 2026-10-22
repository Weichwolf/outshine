# Shared physical river-height reference (#760)

`compare_physical_river.py` now fits one continuous physical surface from the
original WATER2 Ill input. This replaces independent flat fragments **in Python**;
native water/terrain remain unchanged. The 13 selected original surfaces, island,
121 raw course segments, original addresses and positive face/source CSR remain.
Read/epoch/selection contracts belong to [the original frame comparison](native_water_frame.md).
The [derived-bound QP comparison](linear_physical_river.md) removes normal auxiliaries
using two existing directional intervals per face; the SOCP remains its control.

## Height and contact contract

One Float64 vertex buffer owns the physical ENU triangles. Sharing a vertex means
sharing the same physical contact. All consumers must query those triangles; an
independent interpolated height function is not an equivalent contact surface.
The fixed **zero-datum reference XY** footprint has 1492 vertices/faces, one body,
one island and all 1607 original memberships. This is not exact geographic boundary
parity after lifting the footprint: geodetic vertical and anchor ENU-Up differ.

Historical `Surface.LevelM` values are generated DEM-percentile priors, not measured
water-level pins: [original WaterField.cpp](https://github.com/Weichwolf/outshine/blob/6d462324c/src/generators/osm/fields/WaterField.cpp#L127)
samples `GroundQuery` and `SurfaceLevel` selects a percentile. Original flat priors
remain in the input and diagnostic baseline, not as contradictory hard planes.
Original course DEM levels instead supply a soft blend-affine prior (2997 enumerated
references). Its declared numerical model heights are converted to ENU-Up at each
fixed XY by Newton iteration using `dh/dz = Up.z`; no geoid correction is invented.
The QP minimizes the exact triangle mass integral of squared **ENU-Up** target error
over the reference XY domain, not physical-area or observed geodetic-height error.

Hard constraints are shared geometry, explicit finite height bounds, declared
directional intervals and separately supplied ENU-Up vertex contacts. The real case
has **no observed hard height pins**. Separate tests enforce a chosen hard pin and
reject incompatible pins without relaxing them. Lake/sea datum policy, regional
seams, terrain/bridge contacts and source-class admission remain cooker work.

## Whole-facet curvature envelope

Each face is a graph over fixed ENU XY, with unnormalised upward normal
`n=(-gx,-gy,1)`. Heights are bounded by the explicit prism
`z_min <= z <= z_max`. The anchor's h=0 tangent plane supports the ellipsoid;
signed closest ellipsoidal height is at least z_min, including interior points.
The [signed extension](signed_physical_water.md) requires z_min above `-a(1-e²)`
and additionally bounds centre roundtrip error for nonpositive prisms.
The geodetic Up derivative has eigenvalues `1/(N+h),1/(M+h),0`, obtained by
differentiating the WGS84 coordinate map. Both curvature radii are at least
`a(1-e²)`. If a radius R encloses all six prism vertices about its centre c,
integration along the straight segment c→p gives
`|Up(p)-Up(c)| <= delta = R/(a(1-e²)+z_min)` throughout the original positive prism.
The signed case replaces R by R+epsilon and the height floor by z_min-epsilon
for the reconstructed centre and its forward/inverse discrepancy.
The implementation adds 1e-12 unit-vector roundoff allowance; this is Float64
numerics, not an interval-arithmetic implementation. Invalid/nonlocal prisms are
rejected; the regular-normal/local-cap domain is an explicit reference restriction,
not a worldwide altitude or physical-source admission. The original positive
Ill-case bounds and arithmetic are preserved.

For each row choose a unit tangent d at u0=Up(c). At any physical point let
`t=(I-u uᵀ)d`; its normalised projection defines the **declared direction field**.
Horizontal metres are on the parallel surface at that point's current height:
`g = -n·t / ((n·u)|t|)`. This is explicitly distinct from both ENU-Up/XY grade and
the h=0 horizontal metric in [the facet Jacobian](geodetic_facet.md).
Since `|t-d|<=delta`, `|t|>=k=sqrt(1-delta²)`, and `s>=|n|`,
the following convex sufficient conditions enforce `lower<=g<=upper` everywhere
for intervals containing zero (D=-lower>=0, U=upper>=0):

```
n·d + s delta <= D k (n·u0 - s delta)
-n·d + s delta <= U k (n·u0 - s delta)
n·u0 - s delta >= 0.5 s
|(gx,gy,1)| <= s
```

The normals depend linearly on shared vertex heights. Thus sparse linear rows and
one four-dimensional second-order cone per face suffice; no iterative geographic
sampling is used to impose these conditions. An independent cross-product normal
reader reconstructs signed grade envelopes after solving. Primal/dual cone,
stationarity, complementarity and gap checks concern **this finite conservative
convex problem**. They do not establish optimality of the physical field among all
possible fields or of the Earth interpretation. Grade/height checks use 1e-9;
finite QP certificate residuals use 1e-7, separately from old planar test limits.

## Original-input comparison

The same mesh, soft target, 4469 direction rows and objective are used for ENU,
geodetic-centre-only and whole-prism controls. The controls are explicitly marked
as lacking physical whole-face admission. Face directions and 1485 crossing-source
conditions derive from 98 height-delta hypotheses; 23 nearly flat raw segments
remain unknown. Original order is not an observed flow direction. Crossing tests
use straight **zero-datum XY chords**, not a certified geographic source trajectory;
90.7443 m outside the reference footprint and original segment addresses are reported.
Down/cross limits 0.1/0.005 m/m and 20 m boundary spacing are declared laboratory
parameters. They are not source tags, measured channel dimensions or world defaults.

| Declared normal model | Physical sampled row violations | Largest violation | Target ENU-Up RMS |
|---|---:|---:|---:|
| Constant anchor ENU | 100 (96 upper, 4 lower) | 5.4350e-4 m/m | 0.038818 m |
| Actual geodetic Up at centre only | 206 (194 upper, 12 lower) | 5.0652e-6 m/m | 0.038539 m |
| Whole-prism curvature enclosure | 0 | 0 | 0.038574 m |

Each solution is independently checked at 45 barycentric points per face: 67140
physical points and 201105 directional grades. These samples find counterexamples
in the controls; only the separate enclosure supplies the whole-facet statement.
The physical fit's largest soft target change is 0.775725 m. All declared facets
and source bindings remain; its vertices remove the original fragment overlap
step (largest old prior difference 10.227783 m). The local 3D before/after and grade
diagnostic was actually opened. This is a research render, not a native Place image.

## Cost, bounded failure and reproduction

The actual reference solve has 2984 unknowns, 19382 rows, 55148 constraint nonzeros
(673716 B CSC), 7460 full Hessian nonzeros (101460 B CSC), and 178928 B requested
solver vectors. On Linux/x86-64, Intel i5-1135G7, one instrumented run takes about
1.06 s including Python setup,
with 0.228 s inside Clarabel/23 iterations and ~3.30 MB Python-traced peak. Input
preparation adds ~1.56 s/~1.67 MB traced peak. Native solver/GEOS/PROJ allocation,
factorisation work and total copied bytes are **not** measured. Returned diagnostic
arrays occupy ~7.29 MB and include extensive control samples, not a proposed asset.
These are host Python costs; no native/device/frame-budget claim follows.

Limits are 8192 vertices, 16384 faces, 65536 rows, explicit time/iteration budgets
and delta<0.01. Time is a cooperative preparation/solver budget, not process
preemption. Exhaustion/infeasibility returns no geometry and a reason for this
component. World orchestration, local partial recovery and cached native assets
are not implemented here. No provider/source/asset cache is written.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error -m unittest test_physical_river_fit test_native_river_inputs \
  test_geodetic_facet test_river_membership test_river_surface test_profile_mass
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error test/experiments/water/compare_physical_river.py \
  --input test/experiments/water/fixtures/feldkirch-ill-native-inputs.json \
  --output /tmp/outshine-research-water-physical-fit-<commit>/capture
```

Dependencies: existing NumPy, SciPy, Shapely/GEOS, pyproj/PROJ, Clarabel and Matplotlib.
Seven new tests cover geographic/height targets, prism corners/interiors at four
anchors including dateline/high latitude, independent geodetic finite differences,
centre-only uphill counterexample, hard pins/infeasibility, zero-budget no-solver
behaviour and the actual original case. Existing reference fitters remain unchanged.

SIGGRAPH-first: [Kovalsky et al., SIGGRAPH Asia 2015](https://shaharkov.github.io/LargeScaleBD.html),
p.2/Fig.2/equations visually inspected, formulates sparse differential constraints
and explicitly distinguishes approximate feasible optimisation from a global
guarantee. Its iterative bounded-distortion method is not implemented here.
The envelope above is a direct derivation from the WGS84 map; primary coordinate
Jacobian reference: [NASA/JPL DRDGEO](https://naif.jpl.nasa.gov/pub/naif/toolkit_docs/FORTRAN/spicelib/drdgeo.html).
Solver cone conventions follow [Clarabel's Python documentation](https://clarabel.org/stable/python/getting_started_py/).

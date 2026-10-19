# Physical water-facet height and grade (#760)

`outshine_ref.water.geodetic` evaluates a physical plane along WGS84 geodetic vertical and
derives its height Jacobian. It consumes the existing shared ENU convention from
`outshine_ref.places.camera`, matching native `TangentFrame` with origin at
geodetic height zero. Longitudes precede latitudes in this experiment's API;
the original WATER2 input instead stores latitude,longitude and needs an explicit
adapter. No source datum, solver, native consumer or tolerance is changed.

For a query's zero-height point p, geodetic unit up u, and plane normal n through
v₀, its ellipsoidal model height is

`h = dot(n, v₀ − p) / dot(n, u)`.

Let N and M be WGS84 prime-vertical and meridional curvature radii at the query.
For east/north distances measured on the parallel surface at explicitly supplied
metric height m, differentiating the same plane equation gives

`dh/dEast = −dot(nWorld, East)/dot(nWorld, Up) × (N+h)/(N+m)`

`dh/dNorth = −dot(nWorld, North)/dot(nWorld, Up) × (M+h)/(M+m)`.

For m=h the metric factors are one. For m=0 they are required when comparing with
WGS84 geodesic distances on the reference ellipsoid. Choosing either metric must
be explicit; the old affine ENU-Up gradient cannot simply be relabelled geodetic.
A plane's geodetic height/grade generally vary across its interior. Sampled checks
are not a whole-facet directional guarantee; the old affine-gradient guarantee
remains specific to its declared planar model.

The helper records the extended plane height, grade, normal/up denominator and
numerical barycentrics. An outside-plane-extension result is not a triangle height.
The barycentric diagnostic does not replace the native watertight edge/BVH query.
Nearly parallel queries are marked invalid; degenerate triangles are rejected.
No result establishes an ASL/orthometric-to-ellipsoidal observation or geoid offset.

## Independent controls

Five tests cover an equatorial closed form, independent PROJ EPSG:9837 frame
conversion, geodesic finite differences, horizontal-metric interpretation and
outside/parallel cases. The fixed comparison has four anchors (including a dateline
and high-latitude case), radii 10/1000/5000 m, and declared vertex height 500 m.
Two physical constructions are compared: constant ENU-Up and constant geodetic
vertex height. No approximate Ill coordinates are inverted into original inputs.

At 10°E/47°N, a 10 m corner radius already gives about 1.178e−6 m/m difference
between ENU and geodetic grade. The constant-geodetic-vertex plane lies about
7.84 µm below its declared level at the centre. At 5 km radius that centre error
is about −1.9595 m; holding ENU-Up at 500 m instead puts its extended plane about
1.958…1.963 m above that geodetic height at the original corner coordinates.
Those corner queries may lie outside the ENU-control triangle, which is reported.
The height-induced XY shift at 5 km is about 0.3925 m. These are synthetic controls,
not errors measured in the native world or proposed world acceptance limits.

All 312 main point queries agree with independent PROJ ECEF coordinates plus a
3×3 ray/barycentric solve within 8.13e−10 m; the analytic grades agree with 1248
geodesic finite-difference queries within 5.60e−10 m/m. These are measured maxima
in this finite corpus. The checks allow explicit Float64 ECEF-rounding-scale errors,
not a relaxed geographic/model accuracy requirement. The local plot was opened.

The numeric arrays total 27708 bytes; one instrumented corpus evaluation takes
about 0.73 s with ~1.37 MB Python-traced peak. PROJ/LAPACK/native allocations are
excluded. Plotting adds 202 point queries and its own time; raw RSS counters include
imports/plotting. None of these values is a native runtime or device admission.
The original Ill export is now supplied separately in `feldkirch-ill-native-inputs.json`;
the same reference can be applied to its actual geographic rings and physical facets.

## Reproduction and sources

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error -m unittest test_geodetic_facet
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/water .venv/bin/python \
  -W error test/experiments/water/compare_geodetic_facet.py \
  --output /tmp/outshine-research-water-geodetic-<commit>/capture
```

Dependencies: existing `outshine_ref`, NumPy, pyproj/PROJ and Matplotlib. The report
pins versions, numeric/image hashes, corpus values and cost scope. No generated
test oracle, native asset or README/doc image is updated.

SIGGRAPH-first review: local *Simplification Envelopes* (1996), p.1/Fig.1 visually
inspected, keeps geometric approximation error explicit; its simplification method
is not implemented here. The actual geodesy follows the defining WGS84/ENU equations,
checked against the primary [PROJ topocentric conversion documentation](https://proj.org/en/stable/operations/conversions/topocentric.html).
PROJ geodesic controls use the algorithms described by [Karney](https://geographiclib.sourceforge.io/geod.html).
The plane/Jacobian differentiation above is a direct derivation, not a new global
water solver or a guarantee borrowed from either graphics paper or library.

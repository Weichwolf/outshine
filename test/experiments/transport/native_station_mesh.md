# Native planar station cuts — #1189 / #1200

`CutPlanarStations` splits the shared planar arrangement before source-specific
height identities. Every child retains its original planar face parent. The
caller propagates the parent's source mask, then `BuildSurfaceHeightTopology`
creates independent source heights and qualified port unions. Geometric face
identity does not depend on those height variables. This order avoids the lost
overlap constraints demonstrated in #1200; the separate corrected Python lineage
remains an independent reference.

The operator ports the existing `mvt_t_stair_loft._split` half-plane clipping.
A per-cut undirected edge map shares every edge intersection. Fixed original
points never move. Each axis has one station array; new vertices interpolate all
arrays and retain the exact active cut station. Triangle fans preserve positive
winding. Zero-area output is omitted exactly, without a geometric epsilon.
Input must be finite, indexed and positively oriented. Vertex, face, axis, cut
and cumulative face-visit budgets reject without publishing partial geometry.
Cancellation preserves the input. Defaults are local work limits, not world rules.

Cost is proportional to initial axis projections and visited faces/intersections
per cut, with expected hash-table lookup cost. It is not a global linear-time or
optimality claim. Returned vector capacities exclude temporary faces, hash nodes,
allocator overhead and physical process peak. Source acquisition, LP preparation,
solver, body creation and frame/GPU work remain separate.

```sh
make all db
make suite SUITE=outshine/src/base/spatial/PlanarStationMesh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  python3 -B -W error test/experiments/transport/native_station_mesh.py \
  --arrangement /path/to/native-planar-arrangement-native-body \
  --output "${TMPDIR:-/tmp}/outshine-native-stations"
```

Produce the arrangement with `native_planar_arrangement.py --native-body` first.
Use the existing experiment environment and a fresh output directory. Both native
probes retain complete compilation-database flags, source hashes and terminal
receipts. Four rigid controls compare every position/station word, triangle and
parent against the unchanged Python split on the same independently projected
input axes. Source coverage and exact original profile vertices are checked.
The original case additionally consumes native source height identities in the
existing Python height/body/navigation/terrain-product audits and writes its
actual Float32 body diagnostic and raw arrays.

The translated/rotated control has three proximity-diagnostic pairs: two valid
distinct vertices lie about 4.65e-10 m apart. The unchanged Python split produces
identical words/indices. Exact GEOS canonical/source coverages are valid; a real
T-junction control is invalid. The shared scale-based `hanging_pairs` diagnostic
is reported, not redefined or used to weld these points. No tolerance is enlarged.

This station experiment still uses Python for flight selection, LP assembly and
final body products. The separate native `BuildSurfaceHeightProgram` operator
assembles the height model from prepared topology/recipes/junctions/flights and
whole profiles (#1189). `native_flight_plan.py` separately exercises native port/flight
selection and the complete native cut/rebinding chain. Native 2D construction planning
and body/runtime integration remain open.
No runtime/cache/Places fix or frame-budget admission follows from either operator.

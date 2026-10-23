# Native shared planar source domains — #1189

`ArrangePlanarDomains` assembles existing prepared source polygons and partition
lines before height variables or world meshes. It does not infer source topology,
join layers or replace source preparation and the qualified outer profiles.

GEOS unary union nodes the domain boundaries and supplied lines. Polygonization
builds closed cells; constrained Delaunay triangulates occupied cells. Ordered
Float64 coordinates feed `ConformPlanarMesh`. Each output triangle retains a mask
of domain owners; shared XY does not imply a shared height variable. The generator
must retain `(source, XY vertex)` until an admitted port joins those identities.

Only closed cell partitions become CDT boundaries. A dangling interior line is
not a supported general PSLG constraint. The reference checks axes and sections
inside their own source domain; their construction extensions through unrelated
sources impose no height constraint there. Closed landing boundaries use the
occupied union. Direct collinear edge intervals verify these cuts without thin
polygon buffers, which were unstable in a translated control. Coverage validity,
absence of hanging vertices and per-owner symmetric area differences are checked
independently with Shapely. Triangulation order is not a cross-version GEOS API.

The caller supplies weld distance and allowed footprint area difference. No new
global geometric, gradient or raster rule is introduced. Each prepared polygon
is validated; holes and independent overlapping owners are retained. There are
at most 64 domains per local invocation because ownership uses a UInt64 mask;
this is not a world/source limit. Input/noded segments, cells, mesh work and
domain tests have separate limits. Refusal returns no partial arrangement.

GEOS >=3.14 additionally observes the stop token at context-local library
interruption points. Earlier versions observe it between calls and in native
work. No global GEOS interrupt state is changed. Input and returned-work budgets
do not bound GEOS temporary allocations or prove linear time. This is an
asynchronous asset-preparation operation, not work to repeat each frame.

```sh
make all db
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  python3 -B -W error test/experiments/transport/native_planar_arrangement.py \
  --output "${TMPDIR:-/tmp}/outshine-planar-arrangement-replay"
PYTHONPATH=lib/src:test/experiments/transport \
  python3 -B -W error -m unittest test_native_planar_arrangement -v
make suite SUITE=outshine/src/base/spatial/PlanarArrangement
```

Use a fresh output directory and the existing experiment environment. The probe
build preserves complete current compilation-database flags and records source
hashes, commands, original inputs, output geometry and terminal results. It also
replays three rigid transformations. Native and Python GEOS versions are recorded.
No height-program assembly, stair loft, port union, body/navigation/terrain,
asset/runtime integration, GPU saving or world/p99 admission follows from this
operator comparison. Those remain the next steps in #1189/#1003.

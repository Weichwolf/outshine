# Native footpath connection plan

`FootpathConnectionPlan` ports the bounded stair/path proposal in
`mvt_t_connections.py`. Explicit ground, layer, access and endpoint facts govern
admission. Source indices refer to immutable caller-owned axes; original knots
remain in order. A derived intersection is shared by the target cut and branch
extension. Coincident coordinates do not claim original provider-node identity.
Unknown ends, tile fragments, terminals and ambiguous targets stay unconnected.
Native axes also retain original-knot and connection IDs for height/node binding;
fixed existing connections remain protected. Known positional bounds further
restrict the existing width/reach recipe.

The existing `PlanHierarchy` indexes eligible target segments. Its closed box
query uses bounded stack traversal and supports consumer interruption. If a
candidate query exceeds the remaining work budget, it retains at most budget+1
indices and tests none of that incomplete set. This intentionally differs from
Shapely's full returned-index count; the old Python telemetry test stays intact.
No eligible branch endpoint builds no index. `EditsOf` identifies changed sources;
runtime integration need not allocate copied axes for unaffected ways.

Compile `NativeFootpathConnectionsProbe.cpp`, `FootpathConnectionPlan.cpp`,
`FootpathSources.cpp`, `FootpathCandidates.cpp` and `PlanHierarchy.cpp` using their
complete current compilation-database configurations. The probe uses the
`FootpathConnectionPlan.cpp` configuration with only source/object/dependency
output paths replaced. These objects link without GEOS, HiGHS or the renderer.

```sh
make suite SUITE=outshine/src/generators/road/FootpathConnectionPlan
make suite SUITE=outshine/src/base/spatial/PlanHierarchy
PYTHONPATH=lib/src:test/experiments/transport \
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 "$TRANSPORT_PY" \
  test/experiments/transport/native_footpath_connections.py \
  --binary "$NATIVE_CONNECTION_PROBE" \
  --output "${TMPDIR:-/tmp}/outshine-native-connections"
```

The comparison covers the original three sources, a separate original source
context and four shared-identity-preserving rigid transformations. Discrete
choices match exactly; derived Float64 values use the existing 1e-10 comparison
from the original independent geometry checks. This is a test comparison, not
a relaxed physical/profile constraint. Original knots remain exact.

Native timings distinguish plan/index construction and deliberately constructing
every output axis for the comparison. Parsing, process launch and rendering are
outside those timers. Capacity-based index/scratch byte counts are identified
containers, not complete allocator or process peaks. Actual device receipts and
the combined native 2D/native LP/Python body checks belong to
[#1003](https://github.com/Weichwolf/outshine/issues/1003).

`Corridors` consumes the bounded plan before its height/profile construction and
road-page meshing. The OSM reader preserves optional placement/access and tile-end
ownership; construction defaults belong to the native generator. New joins share
the target height and node, while original knots and existing bindings remain.
The live-page test covers a nearby gap, an out-of-reach gap and an existing join.

This plan is a 2D construction proposal. The shared native surface arrangement,
profile program, indexed body and outer mesh/navigation/terrain consumers still
need integration; this connection stage does not establish complete stair bodies,
navigation/terrain coupling or frame-budget admission. Generation
belongs to bounded asynchronous asset-miss work; frames consume finished products
under [#10](https://github.com/Weichwolf/outshine/issues/10).

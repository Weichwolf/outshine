# Native planar region remeshing · #1189

Remove redundant coplanar envelope edges **before** producing height-band cuts.
Retriangulate each owned level/loft-plane band afterwards. This is the existing
`stair_envelope_remesh`/`stair_band_remesh` procedure, with native GEOS execution.

`RemeshPlanarRegions` takes indexed XY faces and disjoint lists of selected face
indices. Every output region retains its input position. Unselected faces protect
their XY vertices. Selected groups retain source-point IDs, holes and footprint;
they add no points. Input points remain caller-owned. The caller verifies that
each region has one valid elevation plane and preserves its source/level metadata.

The boundary queue visits each removed vertex's two neighbors. Final chain error
is checked against the unchanged 1e-10 m collinearity allowance. Simplified polygons
must be valid, retain their holes/protected indices and change coverage by at most
the existing 1e-7 m² numerical allowance per region. Cancellation or exhausted
input/output/ring budgets refuse the whole result. GEOS union/CDT cost is separate
from the linear ring pass; no global linear-time or frame-performance claim.

```sh
make all db JOBS=2
PYTHONPATH=lib/src:test/experiments/transport python3 -B -W error \
  test/experiments/transport/native_planar_regions.py --output "$TMPDIR/planar-regions"
make suite SUITE=outshine/src/base/spatial/PlanarRegionRemesh JOBS=2
```

The experiment builds both native probes with complete current CDB flags, then
consumes native envelope/band faces and the native height-solid closure in the
stacked/crossed reference pipelines. Existing full-profile, manifold Float32 body,
independent-level, navigation and terrain checks remain in force.

GEOS versions may choose different diagonals in cocircular polygons. Region
comparison therefore requires identical oriented indexed boundaries, used vertex
IDs and facet counts, plus nonoverlapping equivalent coverage. Source/level groups
are checked separately. No winding reversal, seam loss or new approximation is
accepted. The closure itself must match every Python geometry word, triangle and
wall family for its actual input. Reports distinguish these two contracts.

Height-band clipping, recipe/group selection and consumer assembly still run in
Python. This is a native operator-consumption proof, not the complete C++ transport
pipeline, cache integration, world rendering or a device-frame admission.

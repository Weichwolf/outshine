# Native surface body · #1189

`BuildSurfaceBody` consumes the existing `SurfaceHeightRequest` and its expanded
height solution. Sources, ports, flight ownership and cut stations stay shared with
height preparation. It creates no second port plan or height solution.

One native call performs:

1. Indexed cap ownership and rise-bounded tread recipes.
2. Coplanar envelope remeshing; retained landing vertices must yield the same recipes.
3. Indexed height-band cuts with shared original-edge intersections.
4. Source/level/flight-plane remeshing, used-point compaction and closed solid assembly.

Ordinary sources retain continuous heights. Coincident XY sheets retain independent
height identities. The reference field stays continuous beneath flat treads. Inputs,
visits, regions, output and cancellation are bounded; failure publishes no partial body.
Operator errors retain their original category. Budgets remain caller-controlled.

```sh
make all db JOBS=2
make suite SUITE=outshine/integration/transport/SurfaceBody JOBS=2
PYTHONPATH=lib/src:test/experiments/transport python3 -B -W error \
  test/experiments/transport/native_surface_body.py \
  --output "$TMPDIR/outshine-native-surface-body" --render
```

The probe builds with complete current compilation-database flags. Both existing
stacked/crossed OSM-derived examples consume its body through the unchanged physical
reference consumers: whole exterior profiles, reference-field constraints, owned
rendered contacts, manifold positive-volume Float32 solids, independent levels,
navigation and terrain reservation data. These consumers remain Python.

GEOS versions may choose different internal diagonals. Final comparison requires
identical tread levels/owners and source/level coverage with nonoverlapping caps;
existing 1e-7 m² area and 1e-8 m boundary allowances remain unchanged. Reports also
show exact array equality separately. Operator-level indexed-boundary and closure
word comparisons remain in `native_planar_regions.py` and `native_height_solid.py`.

Elapsed time covers the native body call, excluding probe IO and fixture solving.
Counters cover planning visits, nearest/plane tests and the constituent operators;
they are not an exhaustive allocation or process-memory account.
No native fixture height solve, runtime/cache integration, actual deformed DEM,
whole-world or frame-budget admission follows from this body-consumption experiment.

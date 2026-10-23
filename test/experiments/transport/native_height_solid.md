# Native indexed height solids · #1189

Input: conforming CCW XY triangles, per-corner upper elevations and per-point
lower elevations. Output: one indexed body, caps in input order, reversed lower
caps, owned boundary/riser walls and analytic positive volume. Elevation is Z.
Independent source sheets retain distinct point IDs even where XY coincides.
Domain clipping, height solving, tread partitioning and collision remain callers.

Walls query a CSR index of sorted elevations per source point. They include
existing intermediate elevations, preserving edge conformity without scanning
every point/elevation pair for every wall. Directed edge twins and cyclic vertex
fans reject open/nonmanifold bodies. Crossing upper profiles require prior edge
refinement. Work/output budgets and cancellation refuse the whole result.

```sh
make all db JOBS=2
PYTHONPATH=lib/src:test/experiments/transport python3 -B -W error \
  test/experiments/transport/native_height_solid.py --output "$TMPDIR/height-solid"
make suite SUITE=outshine/src/base/spatial/HeightSolid JOBS=2
```

The experiment builds with complete current CDB flags. Default inputs are the
existing stacked/crossed independent-flight Python bodies. Optional repeated
`--input path.npz` consumes prepared closures with `points` (XYZ), `faces`,
`step` and `levels`, matching `stair_junction_body.close`; the comparison driver
uses that reference's 0.3 m underside thickness. The generic C++ kernel receives
explicit lower elevations and has no thickness or transport recipe assumption.

Every geometry word, triangle and wall family must match unchanged Python.
Float32 geometry must remain closed with positive volume. The probe records
native elapsed time, indexed heights, range comparisons and index capacity;
process/build/IO and the remaining Python preparation are outside that timing.
Source point IDs and wall parent face indices remain independent of render LOD.
These local checks do not admit a complete native body pipeline or a world frame.

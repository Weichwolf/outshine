# Canonical local station cuts — #1206

`canonical_station_cuts.refine` cuts one indexed Float64 XY mesh before source
height variables exist. Inputs are XY, positive triangles, UInt64 source masks
and axes with `origin_xy_m`, `tangent`, `cuts_m`, `initial_faces`. Selections name
original input faces. `None` selects all faces; an empty list selects none.
Each child retains its original parent and that parent's source mask.

Exact equal frame/cut pairs merge their initial selections. One indexed edge
intersection is created once per cut; identical frames receive identical active
station words. Other faces receive only their actual split boundary edges.
There is no height union, XY proximity lookup, new grid or geometric epsilon.
The existing `stair_station_cuts._split` remains the shared clipping operation.

The native `PlanarStationAxis.InitialFaces` and `PlanarStationBudget` contract
under #1189 is the boundary: defaults4096 vertices,8192 faces,16 axes,32 declared
cuts and262144 face visits. Limits/cancellation reject before publishing a result;
inputs are not modified. Work reports visited faces and created intersections,
not a global optimality claim. The native implementation is owned by master.

`station_cut_stage="canonical"` is opt-in for the Python profile planner.
It freezes original source domains and landing patches on canonical geometry,
selects flights, cuts, and only then creates independent source-height identities.
Qualified ports share heights; spatial overlap alone does not. Stations bind by
the retained height-to-planar vertex index, without an XY search. One solved
height plan still drives the body, per-source stair levels, Nav and terrain.
The existing `source_heights` stage stays the default/reference.

```sh
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  .venv/bin/python -B -W error \
  test/experiments/transport/compare_mvt_independent_flights.py \
  --cut-stage canonical --arrangement stacked --repeats 3 --render \
  --output /tmp/outshine-canonical-stacked
```

Besides the actual body/products/report, this writes input/output NPZ arrays,
axis JSON and an ordered source-ID list defining mask-bit identity. `native-input/`
contains the existing probe's mesh/axes/selection text, with round-trip Float64
words. These are experiment inputs, not a new product format or native execution.
Global controls use all input face IDs in the selection text; the declared JSON
still distinguishes `None`. NPZ output includes parent IDs and every axis station.

Input arrays are retained by reference for parent/coverage reproduction and export.
Their payload, output arrays and height-binding map are counted separately.
Selected XY copies/materialized arrays are not total allocations or UMA residency;
traced Python peak and whole-process RSS have their own scope in the report.

Controls reuse the original case, both #1205 complete independent networks, their
rigid transforms and #1189's fixed `data/station-profile-boundaries.json` input.
The global profile-loss control remains negative. Whole profiles, independent
heights, closed positive Float32 bodies, qualified Nav, shared terrain and local
failure are checked. No native, World, swept walking-route or device admission.

Architecture sources remain the SIGGRAPH/Galin references already reviewed for
the parent in [the transport review](../../../doc/references/transport/README.md)
and the existing finite operators in [profile contacts](mvt_t_profile_contacts.md).
This transfers the existing local operator; it is not a complete paper pipeline.
Commit-specific calls, terminal results, images and measured costs belong in the PR.

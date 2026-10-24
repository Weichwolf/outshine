# Native straight-flight preparation

Owner: #1189. Reference: `mvt_t_stair_loft._flight` / `_clear_cut`.

`BuildSurfaceHeightTopology` retains sorted, unique height-variable membership for
its explicitly qualified ports. Holes and unrelated source levels remain excluded;
subsequent unions remap earlier ports to their final representatives.

`BuildSurfaceFlightPlan` consumes those ports, prepared source axes and explicit
source/start/end/direction declarations. It projects indexed points once per flight,
filters landing members by source faces and chooses the midpoint of the largest
unoccupied station interval outside each landing. Default straightness/search
parameters preserve the existing Python recipe; they are local construction policy,
not a global road grid or world limit. Curved axes require separately planned flights.

`PrepareSurfaceFlights` runs initial height identities → flight planning → shared
station cuts → refined height identities. Initial source-face selections constrain
each flight's cuts; only shared edge neighbors are conformed beyond that selection.
It rebinds the cutter's exact station
values through canonical planar indices. Source masks, child-face identities,
original points and independent source heights remain intact. Every stage preserves
its error type; cancellation/capacity failure publishes no partial result.

Work is bounded by points/flights/visited entries and the existing cutter/topology
budgets. Gap selection sorts only stations inside each local search interval.
Counters cover projections, inspected entries, gap candidates, station bindings,
cut visits/intersections and initial/final port queries. This is asset construction,
not frame work, IO or a GPU performance claim.

The height field uses `h(s)=(1-f)h_start+f*h_end`, with
`f=clamp((s-cut_start)/(cut_end-cut_start),0,1)`. If all three face stations are
at/before the start cut or at/after the end cut, their heights are exactly equal.
Their monotonic gradient is therefore identically zero. Such faces generate no
monotonic-gradient row: computing an absolute-height gradient on thin triangles
and then projecting it can introduce a false constraint through cancellation.
The ascending/descending, translated/rotated thin-landing regression checks the
analytic flat/run/flat field with unchanged original-matrix verification. Grade,
contact, port and non-flat monotonic constraints remain active.

```sh
make all db
make suite SUITE=outshine/src/generators/road/SurfaceFlightPlan
make suite SUITE=outshine/integration/transport/SurfaceFlightPreparation
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 \
  python3 -B -W error test/experiments/transport/native_flight_plan.py \
  --arrangement /path/to/native-planar-arrangement-native-body \
  --output "${TMPDIR:-/tmp}/outshine-native-flight-preparation"
```

The experiment checks the unchanged original case and three rigid transformations:
qualified members, axis/cut/station words, cut positions, triangles and parents.
It also consumes the original complete source profiles through the existing Python
height/body/navigation/terrain audits, including Float32 geometry and both rejected
height-policy controls. Successful images/arrays and exact build commands stay in
the explicit temp output. Three previously documented proximity diagnostics in the
rotated/translated case remain; no welding or tolerance changes.

The cutter supports explicit initial-face selections on one canonical geometry;
see `native_local_station_cuts.md`. The independent multi-flight body qualification
(#1205/#1206) remains separate; this experiment does not admit those native cases.

The experiment's declarations/domains/sections still come from its prepared Python
recipe. The newer native producer is documented in `native_surface_layout.md`;
whole-profile extraction/ribbons and native body/earthwork operators have their
own experiments. Their presence does not establish runtime/cache/Places admission
or actual navigation/DEM-contact integration.

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
station cuts → refined height identities. It rebinds the cutter's exact station
values through canonical planar indices. Source masks, child-face identities,
original points and independent source heights remain intact. Every stage preserves
its error type; cancellation/capacity failure publishes no partial result.

Work is bounded by points/flights/visited entries and the existing cutter/topology
budgets. Gap selection sorts only stations inside each local search interval.
Counters cover projections, inspected entries, gap candidates, station bindings,
cut visits/intersections and initial/final port queries. This is asset construction,
not frame work, IO or a GPU performance claim.

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

The native cutter currently retains the global station scope. The new Python
`flight_footprint` scope and independent multi-flight body qualification (#1205)
have not been ported; this experiment does not admit those native body cases.

The 2D construction declarations/domains/sections still come from the prepared
Python recipe. Native height assembly is a separate product operator; native final
body/navigation/terrain generation and runtime/cache/Places integration remain open.

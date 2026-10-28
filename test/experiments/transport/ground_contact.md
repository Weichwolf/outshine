# Local terrain contacts at an immutable qualified road

[#1574](https://github.com/Weichwolf/outshine/issues/1574), under#1402, addresses
a concrete gap between the existing qualified three-arm road and bound DEM.
The road has123 vertices/154 triangles and three qualified terminal profiles.
Its original-observation error stays within0.5m, but later refined vertices can
lie1.858306m below a fresh DEM query. A valid road alone does not establish a
terrain contact. This is an offline Python reference; native#826 remains separate.

## Existing products, explicit finite construction

`ground_contact_inputs.py` calls the unchanged source/terminal producer and
retains its binding/quality/end profiles. Inputs are the existing pinned facts
and DEM, no source acquisition or new package. The caller's one ENU frame is
shared by road and terrain. DEM queries use the existing ellipsoid-ray geographic
query and convert the supplied geodetic height to scalar ENU up. This explicit
local height-function convention is not a new original-grid/vertical-ray contract.

`ground_contact.py` keeps every road XYZ and face unchanged. It tries the explicit
local margin family5/10/15/20m with soil gradient2/3. Margins are construction
variants, not a global minimum footprint. A first corner/road height witness can
reject before meshing: a continuous function with gradient norm bounded by G on
the convex full patch obeys `abs(za-zb)<=G*distance(a,b)`. Use this witness only
when the fixed road obeys the same physical norm bound; otherwise skip it.

The5m actual candidate violates this necessary bound by2.485479m. The10m candidate
does not violate that witness, but its selected finite terrain cells reach0.699706
instead of2/3. This is a cell-family failure, not general infeasibility. A trial
with94 free soil midpoints/conic fitting did not repair it; no road or acceptance
limit was changed. The15m candidate is directly valid, maximum soil grade0.637716,
without a second road or terrain solve. Its four outer DEM chord profiles are fixed.

Re-use `ground_layout`, its exact road union/constrained-Delaunay outside partition,
and `ground_refinement.refine_ground` with midpoint updates. The protected road
prefix and external edges are never refined; all old vertices stay fixed. Four
passes add124 bounded free terrain vertices on this case. Existing whole-face
soil allowance `G*1.0000001` is unchanged; reported maximum is about0.666666667.
No point membership epsilon releases a contact. No cache/scheduler/public/native
API or GPU contract is introduced. Arrays are local caller-owned offline products.

## Complete finite qualification and limits

`ground_contact_checks.py` independently checks actual positive triangle normals,
strict nonoverlapping conforming coverage of the whole chosen rectangle, unique
XY nodes, manifold incidence/Euler1, all123 original XYZ/154 faces, and all90 road
boundary segments with exactly one road and one soil owner. Four outer vertices
and their entire linear edge profiles stay unchanged. Removing a soil face despite
preserved contact point values, a soil-only T-junction or altered outer Z fails.

The chosen result has251 vertices/496 triangles and no height gap on those90 entire
shared edges. Its domain is3341.626256m². Refinement has explicit vertex/face/pass/
query caps and may retain the valid coarse surface when exhausted. Missing initial
DEM/capacity or no valid variant returns local Pending; the original road survives.
The original three terminal products are untouched, not newly interpreted.

This is C0 finite terrain with coarse visible facets, not C1/smooth photorealistic
terrain, a dense-DEM error bound, original terrain-grid/neighbour seam admission,
other building/road/water contacts, legal route/vehicle clearance or native World.
The four chosen outer chords refer to this local before/after DEM height mesh;
they do not certify the existing native DEM-grid border. No Source/World/LOD/device
acceptance follows from the local geometric result. Those gaps remain with#826/#1402.

## Alternatives and reproduction

The coverage-union path reuses known nonoverlapping road triangles. The general
union control uses the same complete inputs, candidate order, limits, refinement
and independent checks. Require exact final/coarse meshes, mapping and prior
buffers for the actual comparison. No new solver family or artificial always-fit
baseline. Report source/road preparation separately from warm complete construction,
all DEM query calls/points, variants/layouts, refinement queries/vertices/faces,
allocation requests/scoped high watermark and retained bytes/process peak.
Total copied bytes and native/cache/frame/device costs remain unmeasured.

```sh
PYTHONPATH=lib/src:lib/tests:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 nice -n 10 .venv/bin/python -B -W error -m unittest test_ground_contact ground_surface_checks ground_refinement_checks ground_network_checks ground_terminal_checks -q
PYTHONPATH=lib/src:test/experiments/transport OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B test/experiments/transport/replay_ground_contact.py --output /tmp/outshine-research-ground-contact-comparison --rounds 3 --trace --plot
```

Use fresh explicit system-temp outputs. JSON/NPZ retain bindings and actual source/
before/after meshes. Local figures use actual triangles and metric aspect, no
height exaggeration. No README/doc images. SIGGRAPH2008 graph/geometry separation
and Galin et al.2010,pp8–9/Figs17–20, road profiles followed by local terrain
refinement/cut/fill are primary context; full local pages/figures inspected.
Finite margin selection, the conditional witness and exact fixed-product contact
checks here are our own bounded derivation, not the full paper algorithm.

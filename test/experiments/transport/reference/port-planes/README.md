# Captured shared road ports and joint ground

Owner: [#169](https://github.com/Weichwolf/outshine/issues/169), parent #20.
These files are input fixtures, not expected new C1 results. Normal tests and
experiments verify hashes and never recapture or change pins.

`Vienna.npz` retains the original indexed road/module/port order and the conforming
joint road/soil mesh: 30256 road vertices, 31780 road faces, 3317 modules,
3888 whole shared ports, 30260 joint vertices and 60514 joint faces.
The road vertex map binds the two index spaces; road priors/grade frames match
exactly. `metadata.json` records input hashes, dimensions, metric frame and origin.
Mapterhorn Z values remain as supplied; no geoid/MSL conversion is claimed.

`sources.json.gz` keeps all 3478 original normalized source axes with attributes,
their complete usage ledger, source URLs/hashes and original DEM capture receipts.
Per-face direction sources select grade recipes and do not claim exclusive face
ownership. Original unsupported/deferred sources and their reasons remain present.

Vector sources: © OpenMapTiles · © OpenStreetMap contributors. Elevation:
Mapterhorn. Provider URLs and exact captured hashes remain in the source receipts.
No SDK binary, foreign implementation or runtime dependency is contained here.

## Height construction

Original source triangles and whole cuts remain the domain. Shared junction
groups own affine planes `P0`/`P1`; ordinary corridors join them. Let `u` and `v`
be scaled signed distances to the two actual cut lines, `D=u+v>0`, `t=u/D` and
`w=3*t²-2*t³`. The patch is `F=P0+w*(P1-P0)`. On either whole cut, `w` equals the
required endpoint value and its gradient is zero. Therefore both the height and
the complete two-dimensional gradient equal the same parent plane used by the
neighbouring junction. Different end crossfalls are included in this identity.

At multiport corridors or ill-conditioned denominators, an explicitly logged
local common-plane recipe merges the affected parents. On these captured inputs
there are 1536 plane nodes: 909 shared junction groups, 509 terminal parents and
118 isolated modules. The 104 local recipes retain all XY/source geometry.

For a triangle, `D⁴*grad(F)` is polynomial of degree four. Its triangular
Bernstein coefficients divided by the positive Bernstein coefficients of `D⁴`
are a convex hull for the rational gradient. Requiring every normalized vector
coefficient to lie inside the grade ellipse is sufficient for the entire face.
This is conservative and is not equivalent to an exact extremum solver.

For a linear height interpolant `L`, `D³*(F-L)` also has degree four. Positive
degree-elevated coefficients of `D³` bound the entire height error by the maximum
absolute coefficient ratio, with an explicit floating-point guard. Independent
controls check analytic affine/curved cases, finite differences, dense barycentric
probes and rigid frame changes. Red/green children remain in their original
triangles; port-edge ancestry and the captured prior's linear interpolation survive.

The sparse global fit shares parent variables, enforces point contacts and also
checks actual finite road/soil gradients. C1 applies to the analytic road plan;
finite faces and soil remain C0. The experiment is not a complete traffic, layered
network, LOD, native-runtime or driving-dynamics proof. SIGGRAPH2008 page2/Fig.2
motivates the graph/geometry separation, not this particular constructive solver.

## Reproduction

```sh
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/port_plane_checks.py
PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/port_plane_world_checks.py
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src .venv/bin/python -W error test/experiments/transport/port_plane_adaptive.py --output build/experiments/transport/port-planes/adaptive
```

NumPy, SciPy, Shapely and the existing optional Clarabel transport dependency are
required. Results, costs and generation hashes belong under `build/experiments/`.
The public showcase retains exactly four whole tiles and only receives verified
replacement images. Detailed validation views remain separate.

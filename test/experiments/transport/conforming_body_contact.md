# Conforming terrain contact with an unchanged body

Issue [#1245](https://github.com/Weichwolf/outshine/issues/1245), parent
[#1189](https://github.com/Weichwolf/outshine/issues/1189).
This finite Python generator reference follows the point-support experiment
[#1235](https://github.com/Weichwolf/outshine/issues/1235). No native recipe,
source role, provider or world cache format changes.

## Deficit and construction

Correct point heights do not make a grid's triangles contact a body. The driver
uses the same captured World ports and constructed 198-vertex body as #1222,
with its 58 actual underside caps, source masks, parts and UInt64 port IDs.
The caller explicitly declares one ground group; this is not inferred from
provider tags. The same nearest-footpoint field and controlled exterior blend
from #1235 remain: flat prior 25 m, relief 3 m, existing adjustment limit/fade.
This is not a measured Earth/DEM reconstruction or a current native bug report.

The geometric reference nodes the input grid triangle edges and cap edges in
XY with GEOS, polygonizes that arrangement, and performs constrained Delaunay
triangulation inside each cell. Original cap breaklines become terrain edges.
The supplied height callback evaluates the unchanged support field only after
geometry selection; inside the body it returns the actual affine cap height.
The callback is a caller contract, not arbitrary finite heights certified by
`conform`. The driver independently checks actual mesh contact before success.

`local_conform` limits that arrangement to input triangle boxes meeting the
complete cap box. This conservative box is a work selector, not ground evidence.
It rejoins untouched triangles using exact-coordinate identity, with no snap
or epsilon weld. Full joined coverage and the complete input domain are checked.
For the fixed World case, local and global XYZ words and unordered indexed
triangles are exactly equal; all original grid vertices survive. The local
construction is the driver's selected method; `conform` remains the measured
whole-domain reference. The separate `arrange` helper constructs XY only.

## Contact and topology verification

A separate common-refinement check intersects every relevant terrain triangle
with the source caps. Both heights are affine on each intersection polygon,
so their signed extrema occur at its vertices. The check examines all positive
area intersections, not merely the earlier cap centres and used vertices.
The result is a binary64/GEOS calculation, not an exact-rational proof.

For this unchanged World control the ordinary 0.5 m grid reaches 0.0581597 m
above the underside and 0.0654143 m below it. These larger values strengthen,
rather than contradict, #1235's explicitly finite point probes. Both conformed
meshes reach at most 7.11e-15 m in either direction, within the existing 1e-7 m
height tolerance. Compared area is 14.30251599685808 m² versus source area
14.30251599685800 m²; do not interpret that rounding difference as exact area.
The grid grows from 3,021 vertices / 5,824 triangles to 3,469 / 6,720.
Output numeric arrays grow from 212,280 to 244,536 bytes, before any native
format, padding, attributes or acceleration. The original 13 body arrays remain
word-equal, 24,936 bytes; source rows and ports are exported unchanged separately.

Checks include positive/conforming coverage, exactly two incident faces on
indexed interior edges, no internal boundary edges, full domain preservation,
the actual source words/masks, both diagonals of an off-grid tilted plane,
a selected body with a hole, and cancellation/budget/unresolved-height cases.
The body's hole remains covered by terrain; it is not incorrectly cut away.
No source body is lowered or lifted to hide a terrain error.

## Costs and limits

The CLI rotates ordinary/global/local calls for one first and seven following
invocations, recording wall and process CPU together at normal priority.
Ordinary and conformed meshes have different contact quality: their timings
are costs, not an equal-quality speed contest. Global/local are equal geometry
in the fixed control and supply that comparison. Tracemalloc runs separately;
its live blocks are not cumulative allocations, and it excludes GEOS heap
allocations. Whole-process peak RSS includes all alternatives/source, before
plotting. Logs retain raw timings and load average; actual results belong in PR.

The default XY arrangement admits at most 32,768 input segments and 16,384
output cells/triangles. Local input also admits at most 16,384 triangles.
The reused support callback has its own bounded candidate admission. Cancel is
checked between stages, not within individual GEOS calls. These limits do not
establish a hard scratch-memory cap: arrangement complexity and GEOS internal
allocation/cancellation latency remain unmeasured. Exhaustion/errors return no
mesh; inputs remain unchanged. This is asynchronous generation work, never a
frame-path operation. A cached native product must avoid rebuilding on a hit.

Only one explicitly qualified continuous height surface is selected. Multiple
levels, disjoint-height medial-axis continuity, neighboring stamps, actual DEM
shape, page boundaries, Float32 quantization and physical/native ownership remain
consumer work. Conforming contact does not fix global continuity or slope of
the unchanged exterior recipe. Generalisation requires those contracts; no
native exterior default, device performance or worldwide admission is claimed.

## References and reproduction

SIGGRAPH 1993 *Mesh Optimization*, p. 1, was checked first and visually opened:
its approximate fit objective does not enforce this contact constraint.
SIGGRAPH 2016 *Mesh Arrangements for Solid Geometry*, p. 3, was also visually
checked for intersection cells and robustness limits. Its exact-rational solid
construction is not implemented or claimed here. This is a simpler bounded
2D GEOS arrangement. Both PDFs already exist under `doc/references/`.
The installed experiment environment has Shapely 2.2.0 / GEOS 3.14.1; constrained
triangulation and coverage validation are required. No new dependency is added.

```sh
PYTHONPATH=lib/src:test/experiments/transport .venv/bin/python -B -W error -m unittest test_conforming_body_contact test_body_ground_support
PYTHONPATH=lib/src:test/experiments/transport:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/transport/conforming_body_contact_experiment.py --repeats 7 --render --output /tmp/outshine-conforming-body-contact
```

The explicit driver saves report, full before/after geometry, all source body
arrays, source/port bindings and a local diagnostic. Ordinary tests write no
oracles. The diagnostic uses the existing full CPU perspective depth raster at
1280×720 for both unchanged scenes and a separate centre cut; no painter-order
3D visibility is assumed. Material-ID colours and facet shading are scientific
geometry diagnostics, not PBR or device screenshots. Open the resulting PNG.

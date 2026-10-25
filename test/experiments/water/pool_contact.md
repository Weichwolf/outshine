# Source-bound pool body and local terrain contact — #1248

The actual #826 counterexample is `swimming_pool`, not a natural bank. The
existing basin stamp drops that source distinction and lowers one node by 2 m.
This finite Python reference preserves the water and constructs the missing
pool body and two distinct terrain-contact roles. No native format/shader or
new water-height fit is introduced.

## Original and declared additions

`fixtures/native-bank-contact/input.json` is the exact 6,446-byte slice from
[#826/6385878425](https://github.com/Weichwolf/outshine/issues/826#issuecomment-6385878425):
21 prepared facets and four terrain nodes, native commit3e22f2c5a, asset/package
hashes, both geodetic and ENU probe values. Its original XZ digest is
8d395043756eaec944b1eb52023c157e0a16916bc194da63c12b0bce4612eae2.
`source.json` binds those unchanged bytes to the later full local-component
and source-semantic evidence from
[#826/6386000596](https://github.com/Weichwolf/outshine/issues/826#issuecomment-6386000596),
XZ digestc56ad9b25410dbb91fb62f59a76f8cb6fb1de3ab4ba6d33e1b32777037249556.
All facet words were checked against that second payload before metadata
extraction; duplicate coordinate copies are not retained in the fixture.

The source retains kind, layer, native input/surface/face IDs, feature13808816262,
provider revision/digest and tile. Its connected native component is complete
locally, not certified geographically complete beyond source coverage. Ground
contact is an explicit controlled recipe declaration, never inferred from kind
alone. Unknown/roof/bridge ground roles and clipped/continued footprints refuse
this recipe. Holes/islands and multiple components refuse rather than fill.

The original water facets/height words stay unchanged. The declared recipe is
2 m water-to-floor depth, 0.2 m floor thickness, 0.3 m wall/rim width and 0.2 m
freeboard. These are plausible generator choices, not measured construction or
bathymetry. New exterior points reuse the same native height-model value through
the existing WGS84 coordinate evaluator; original water vertices override those
values exactly. No new datum or second height optimization is fitted.

The native attribution replays exactly one wet node, source face5317. Its affine
bed height matches the native stamp to rounding. The existing ENU→geodetic→F32
reference also reproduces the saved single low height word; this is not full
native vertex/shader equivalence. The separately supplied native queried water
Up differs from the facet interpolation by about1.8 nm and is not overwritten.

## Body and contact construction

The floor retains the source triangulation. A mitred outer offset and constrained
triangulation form only the wall/rim band; internal water facets create no walls.
Five indexed parts close the solid: underside, floor top, rim top, inner walls,
outer walls. In the original case this is115vertices/226triangles/339edges;
every edge has two oppositely directed incident faces, Euler characteristic2
and positive volume. Vertical intervals on the common cap triangulation prevent
crossing floor/rim layers; lost positive materialized thickness is rejected.

Terrain cannot simply follow the nearest underside outside: that creates a deep
trench around the pool. The supplied contact surface uses the floor underside
inside, rim height at the outside boundary, and a continuous ramp wholly within
the solid wall band. It shares the body's band triangulation. Affine nodal
lower/upper inequalities therefore hold throughout each band triangle. Outside,
the same declared quintic3m transition joins the supplied terrain prior. This is
a pool-specific contact recipe, not a changed transport exterior rule.

The four saved geodetic/F32 terrain heights are placed in the existing ENU model,
with an explicitly chosen diagonal; this is not native render-vertex word proof.
A strict unchanged #1245 GEOS overlay declines on this oblique cell: no missing
area, but5.61e-15 m² rounded extra boundary area. Its exact-domain check is not
weakened. The declining GEOS candidate remains an explicit comparison, not a
silently accepted fallback.

`pool_cell_partition` clips the existing triangles with exact rational planar
predicates/constructions, joins all exact shared edge nodes, and fans each convex
piece around an interior point. Entirely interior/exterior triangles are
qualified by exact clipped cap area. Partial contact uses at most64outer-edge
half-planes: contact pieces must be contained in their intersection and cover
its exact area. Sequential clips partition the remaining exterior without
overlap. A concave partial contact that cannot satisfy this proof is refused.
It verifies exact local interior and full area before Float64 output. Fractions
describe the input doubles exactly, not the Earth exactly.
The saved one-cell case crosses one true outer wall segment and remains unchanged.
The original case has6candidate pairs,8pieces,21vertices/30triangles and390edge-node
tests. Unique output vertices are rounded once; distinct exact nodes may not
collapse. Rounding is measured explicitly; original-edge parameters accompany
all output boundary vertices. Adjacent products must consume the same bindings
and resulting position words. No global exact-geometry or new cache system.

The complete binary64 common-refinement contact check is about1.1e-13 m above/
below the defined contact surface, within the inherited1e-7 m height tolerance.
It covers the whole0.218114m² body/patch overlap, not just point samples. This
represents only the supplied cell. Boundary heights change by up to1.0652m;
output admission is `candidate_pending_neighbor_updates`, with explicit old/new
heights and edge parameters. No closed neighboring/world contact is claimed.
New boundary vertices also require neighbor updates when heights do not change;
`new_outer_boundary_vertices` counts these subdivisions. A flat-floor control
with two independently generated triangles retains the same shared midpoint and
position words in both exact and GEOS paths. It exposes the former height-only
update decision without claiming a native raster defect.

The same known cell can be split into its two original triangles and generated
independently with different local vertex slots and cyclic face order. The
follow-up check maps slots back to the four original input-corner IDs and
canonicalizes edge direction/parameters. Both cuts contain the same five points
on their common diagonal. Exact XY words and reference Float32 XYZ words agree;
rejoining the independent pieces gives the original21vertices/30triangles and
the same unordered index connectivity without epsilon welding. Float64 height
queries retain the existing1e-7m contract; the observed Linux case is word-equal
there too. This is a two-consumer check inside one known cell/frame. It does not
qualify adjacent native pages, differing frames, shader transforms or the missing
outer terrain. No new border cache, resolver or native product format is added.

This one-cell agreement does not make arbitrary independent partitions composable.
Splitting the existing padded-outline planar control into its two original
triangles gives13and11shared-edge knots, whose union has15. Piecewise linear
interpolation then leaves a3.319199m vertical seam outside the body despite
matching heights at shared points. These remain the existing declared planar
control heights, not newly supplied native terrain or a reconstructed DEM.

`pool_contact_neighbors.reconcile_pair` keeps one supplied source frame and
original vertex IDs for two nonoverlapping conforming patches of at most64faces
each. It first collects their exact original-edge parameters, exchanges the
union on shared edges, and rebuilds only patches missing knots. An already
matching original-cell pair reuses both first-pass results without rebuilding.
`boundary_requests` contains
an original boundary edge and a rational parameter string; reversed edge
direction reverses the parameter. The exact partition inserts these points
before triangulation, then evaluates the unchanged contact recipe. It neither
fits another height surface nor moves an original point. Requests on internal
edges, out-of-range/invalid parameters, excessive inputs and requests to the GEOS
comparison path are refused. At most1024requests with512characters each enter
the existing1024node/262144candidate bounds; exponents are not accepted.

The second pass creates no new polygon intersections or boundary demands:
it only inserts the first pass's known knots. Both resulting profiles must have
the same parameters, identical XY words and reference Float32 XYZ words, and
height disagreement at most1e-7m. Otherwise the pair refuses qualification.
The control then has15knots on each side, zero measured seam gap and a joined
indexed disk whose shared segments each have two incident faces. Its outer
boundaries still report `candidate_pending_neighbor_updates`. This bounded
two-patch reference does not implement page streaming, multi-page closure,
different frames or native packing, and supplies none of the missing9pages.

Additional planar controls use all21original water facets as an interior domain,
the actual23-edge outline inside a padded rectangle, and an analytic convex
corner with exactly one sixth of the input area inside. They check complete
coverage, positive faces and indexed topology. These are geometric controls;
they do not supply or reconstruct the missing native neighborhood heights.

## Original neighboring pages

The original nine-page window is now pinned as
`test/outshine/data/terrain/pool-contact-neighborhood.tar.xz`, with provenance
beside it. `pool_native_neighborhood` verifies the archive, every height/fraction
file, complete tile coverage and shared core words before constructing geometry.
It reuses the supplied source/package identities and all44,100Float32 height
bytes;392repeated core-edge values agree exactly. The old four height words are
present unchanged. Their WGS84 reconstruction differs from the earlier printed
geographic coordinates by at most2.05nm. These are the original deformed end
heights, not a pre-deformation DEM or a newly cooked replacement.

The finite reference places those words in the existing WGS84 Double frame and
retains explicit NW-SE grid diagonals. It does not reconstruct native shader or
Float32 page transforms. The source has9409unique nodes/18432triangles. Four-cell
patches keep the existing exact kernel bounds; a conservative patch box rejects
work beyond the declared3m apron. Exact original-edge parameters are exchanged
once, and only patches missing a knot are rebuilt. Reusing an unchanged patch
must preserve every output word relative to unconditional rebuilding. No further
intersection is introduced by inserting the already known boundary knots.

The complete window qualifies all2681.769m² of the body's two-role contact
surface. All common-refinement extrema remain within the existing1e-7m height
contract. The384outer source edges retain their original positions/words, and
the joined mesh is a positive indexed disk. `reference_window_closed` names only
this finite Double reference; partial windows report `partial_body_window`.
Disjoint apron patches now report zero compared area and `positive_area_overlap`
false, instead of failing an empty extrema reduction. This does not manufacture
body coverage; the complete-window check separately requires the entire body.

The direct whole-window ENU→Float32 diagnostic has25coincident vertices and140
nonpositive triangles. It is an explicit negative packing control, not a native
representation or accepted GPU mesh. Native adoption still needs the agreed
contact-mesh/regular-grid choice, shared output frames and original-edge word
ownership; a0.3m wall cannot be assumed represented by the roughly0.9m grid.
Other world-class contacts and physical/runtime integration remain native work.
The existing body/water source words and recipe dimensions are unchanged.

The explicit neighborhood driver stores the whole candidate, original mesh,
source-node mapping, work counters, a same-quality rebuild comparison and a
fixed common1280×720depth before/after view. Allocation tracing is scoped to one
real32-face witness patch; process RSS and cumulative returned numeric array
bytes are separate, with external GEOS allocations uncounted. No Python time,
Float32 diagnostic or closed finite window establishes a world/device budget.

## Checks, cost and limits

Exact edge noding first checks rational coordinate intervals. An interior point
on a nondegenerate edge must lie strictly between its endpoints on the selected
nonconstant axis and inside the closed interval on the other axis. Only surviving
candidates need the rational collinearity determinant. Endpoints remain explicit
polygon vertices. Vertical, horizontal, reversed and sheared shared edges retain
their exact third-point subdivision and indexed incidence. There is no floating
bounding approximation or changed intersection/rounding rule.
`edge_node_tests` still counts every candidate and owns the unchanged work bound;
`edge_node_determinants` counts the surviving noding determinants, excluding the
separate exact clipping calculations. Raw same-input cost comparisons live in
the PR; the additional counter does not imply new resident geometry.

Tests preserve original water words and source permutation, closed body winding,
contact roles, internal-subdivision wall invariance, negative role/hole/dimension
cases, exact partition/positive areas, indexed topology, shared rounding and
explicit boundary obligations. Ordinary tests write no images/oracles.

The explicit drivers retain original/constructed arrays, metadata, edge demands,
reports and local pictures. Full-body perspective1280×720 and local orthographic
1280×720 depth views are separate scientific diagnostics. Both use the corrected
shared CPU raster, actual triangles and declared display transparency; no PBR,
material, near-camera native raster or target-device admission is implied. The
single-cell view intentionally lacks surrounding terrain; its finite outer
boundary is not disguised as a finished environment. Both PNGs must be opened.

The contact driver measures one first and seven following CPU invocations at
normal priority, then a separate traced allocation call. Live blocks are not
cumulative allocations; GEOS heap is excluded. Whole-process RSS includes source,
body and alternatives before plotting. Raw results/load belong in the PR.
Body arrays total16,082numeric bytes; local mesh1,224B plus320B numeric boundary
demands exclude Python/Fraction/string/GEOS/native packing costs. These are extra
products, not a memory-saving or equal-quality speed claim.

Limits:2048source water facets,16384plan vertices; exact cell≤64input triangles,
≤2048contact facets,32768potential pairs,64outer edges,128pieces,1024nodes and262144edge-node
checks. Cancel is checked between exact clipping stages; individual GEOS calls
are not preemptible. These are admission/work bounds, not hard scratch/RSS caps.
Everything is asynchronous generation; a cache hit must not redo it. Native
height-grid/contact-mesh integration, adjacent pages, world/physics/Float32
qualification and device budgets remain native work under #826.

## References and reproduction

SIGGRAPH2016 *Mesh Arrangements for Solid Geometry*, p.3, was visually reviewed
for exact intersection construction and the distinction from floating output.
SIGGRAPH1993 *Mesh Optimization*, p.1, does not enforce these contacts. Existing
#1040 zero-depth-at-shore and #941 shared-cut references were checked first;
they do not justify natural shores for a swimming pool. This implementation is
a bounded planar reference, not the papers' general solid/optimization method.
Existing NumPy, Shapely/GEOS, pyproj and Python Fraction are used; no new package.

```sh
PYTHONPATH=lib/src:test/experiments/water:test/experiments/transport .venv/bin/python -B -W error -m unittest test_pool_terrain_contact test_pool_body test_native_bank_slice test_conforming_body_contact test_body_ground_support
PYTHONPATH=lib/src:test/experiments/water:test/experiments/render .venv/bin/python -B -W error test/experiments/water/pool_body_experiment.py --render --output /tmp/outshine-pool-body
PYTHONPATH=lib/src:test/experiments/water:test/experiments/transport:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/water/pool_contact_experiment.py --repeats 7 --render --output /tmp/outshine-pool-contact
PYTHONPATH=lib/src:test/experiments/water:test/experiments/transport:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/water/pool_region_experiment.py --repeats 7 --render --output /tmp/outshine-pool-planar-controls
PYTHONPATH=lib/src:test/experiments/water:test/experiments/transport:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/water/pool_neighbor_experiment.py --repeats 7 --render --output /tmp/outshine-pool-neighbor-control
PYTHONPATH=lib/src:test/experiments/water:test/experiments/transport:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error -m unittest test_pool_contact_neighbors
PYTHONPATH=lib/src:test/experiments/water:test/experiments/transport:test/experiments/render OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python -B -W error test/experiments/water/pool_neighborhood_experiment.py --compare-rebuilds --render --output /tmp/outshine-pool-native-neighborhood
```

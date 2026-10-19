# Camera-cell surface support (#899)

The isotropic field-tree comparison loses silhouettes and material ownership.
Before simplifying its surfaces again, this component determines a smaller set
that contains the original visible surfaces for an explicit camera-position box.
It keeps original boundaries and vertex-stage fields. This is support for a future
shared projected field, not that field representation or a native replacement.

## Geometry and scope

Input is the unchanged complete native cell and actual120camera poses from
`test/outshine/data/render/building-full-view/`. The observer box is the bounding
box of those camera positions. It has no invented universal radius or guaranteed
async build reserve. Camera orientation is unrestricted within the declared
projection/near-clip guard; the given positions are a finite preparation domain.

The signed distance from the box to each original triangle plane determines
whether the whole box is behind that one-sided surface. Such faces cannot be
front-facing primary hits and are removed. Degenerate/uncertain faces stay.
This is separate from encoded shading normals, which remain original fields.

For occlusion, rank original opaque triangles by area/distance-squared, using at
most128blockers. This is a work cap, not a depth-layer or coverage truncation.
Each blocker must face every box corner and lie outside a conservative near-clip
sphere for every eye. Retain the selected blockers themselves.

Each camera corner and blocker edge define an inward cone halfspace. Intersect
these24halfspaces and the halfspace strictly behind the blocker plane. A candidate
triangle is removed only if all three vertices lie strictly inside this umbra.
Partial/uncertain shadows keep the entire candidate; no projected-grid extension,
silhouette clipping, coplanar replacement, field averaging or shadow fusion occurs.

Why corners suffice in exact geometry: for a fixed target point, eye positions
whose segment intersects a convex opaque triangle form a convex cone on the
observer side. If all eye-box vertices lie in it, their convex hull does too.
For a fixed eye the blocker shadow is convex, so testing all candidate vertices
covers the triangle interior. The behind-plane condition enforces depth ordering.
The implementation uses Float64 original Float32 positions with inward margins;
finite independent controls supplement this argument, not exact-arithmetic proof.

The guard matters: an occluder clipped by the renderer's0.1camera-depth near plane
cannot hide a distant surface. Blocker plane distance exceeds0.1times the maximum
declared ray-vector length. Queries reject longer rays, nonfinite positions and
eyes outside the box. This also protects against using arbitrary ray scaling.

Limits:131072input triangles,512maximum configured blockers,1024maximum batch;
the supplied profile uses128and256. The point/halfspace arrays are batched. Copies,
gathered triangle bytes, tested elements and largest test buffers are reported;
these counters are not a complete allocator trace. Process peaks include libraries.

## Product and independent controls

`header.json` owns domain, source provenance, projection/near guard, array costs
and omitted-source reasons. Domain/identity are checked before `surfaces.npz`
is opened. Surface vertices/indices are compacted without resampling; the original
UV bits, normal words, metre fields, RGBA and six-column source lineage remain.
Default-white RGBA and wholly zero metre fields use explicit empty storage.
Indices use exact u16 when possible. Material IDs are evaluated with the existing
original vertex-stage rules before interpolation; none are inferred from hits.
Original face IDs retain tie order and allow independent boundary comparisons.

The consumer accepts a ready product and request without original geometry or
recipes. It holds one prepared CPU triangle index across queries. Empty support
is a valid explicit camera-only result. Unmet domain demand raises a named error;
async original fallback/publication in the real engine is not implemented here.

Tests include known hidden and partially visible triangles, an interior-view
occlusion that disoccludes at a box corner, nearer/coplanar surfaces, a box crossing
the blocker plane, near-clipped blockers, nonwhite alpha/affine fields, empty
products, source mismatch and rejection before payload IO.

`check_viewcell_support.py` independently solves segment/triangle intersections
for every removed-by-umbra original triangle:25eyes (corners, centre,16seeded
interior points) times8target points. Direct original plane checks cover backfaces.
The full comparison keeps all120original poses,8additional interior viewpoints,
native resolution/2x2subsamples, independent exhaustive native rays, raw fields,
two changed Lambert lights and actually inspected material/normal PNGs. This is
an opaque diagnostic, not native PBR/filtering/opacity/TAA validation.

## Costs and decision boundary

In the initial complete-cell control,8185of14169triangles are wholly back-facing.
The additional128-blocker pass removes475more, leaving5509triangles/11726vertices.
Surface arrays take408286Bplus154252Blineage, versus943302Bexact original native
u16geometry/attributes without diagnostic lineage. The product currently also
stores zero metre fields on retained roof vertices; costs include that padding.
Bit-identical full-attribute welding is checked as an alternative to the original.
Actual containers, header, derived CPU fields/index allocations and peaks add cost.

Backfaces alone leave5984triangles and438880Bcore arrays. They account for most
of the reduction at much less cooker work. The extra umbra removes only30594Bcore
in this cell for53016525halfspace/vertex tests. Measure total bake/amortization
before retaining that step in a real field builder; fewer records alone do not win.

573of576source identities remain. This does **not** reduce world-level object
jobs, establish a global projected hierarchy or solve #844. It must not become a
per-camera product cache with unbounded duplicate geometry. Shared field nodes,
directional demand, useful-domain reserve and global amortization remain next.
Do not run parameter sweeps to turn this support component into a whole-world claim.

Camera-hidden faces can still cast shadows, reflect or become visible from another
observer. Original logical and caster assets stay authoritative. If they remain
resident, a separate camera product adds bytes rather than proving net world RAM
savings. Native execution and both device budgets remain unmeasured. Use the
support initially to avoid unnecessary work in a shared field bake, not to delete
original world content or introduce a native rendering/binding path.

SIGGRAPH-first basis: Durand/Drettakis/Thollot/Puech2000, DOI10.1145/344779.344891,
local `doc/references/geometry/siggraph/2000-conservative-visibility-extended-projections.pdf`.
Pages3/4(figures2–5and convex-view arguments) optically inspected. This component
implements a small single-blocker halfspace specialization; it does not reproduce
the paper's extended-depth raster, reprojection sweep or occluder fusion.

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render .venv/bin/python -m unittest \
  test_viewcell_visibility test_viewcell_product test_layered_tree test_attributed_prisms test_full_view_reference
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/render/compare_viewcell_support.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --product /tmp/outshine-research-viewcell-product-<commit> \
  --output /tmp/outshine-research-viewcell-report-<commit>
OPENBLAS_NUM_THREADS=1 .venv/bin/python test/experiments/render/bench_viewcell_product.py \
  --product /tmp/outshine-research-viewcell-product-<commit> \
  --request /tmp/outshine-research-viewcell-report-<commit>/cache-request.json \
  --output /tmp/outshine-research-viewcell-cache-hit-<commit>
```

Paths are explicit fresh system-temp locations. Existing NumPy/Trimesh/Rtree/Pillow
and shared references with Shapely/SciPy are required. #899and the overall goal
remain active; this completes a continuous-domain support component only.

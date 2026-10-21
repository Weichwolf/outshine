# Analytic projective field cost control (#899)

The proposed source-only analytic field loses the first cost comparison. It keeps
one trimmed surface record for every original triangle. That avoids the earlier
sample-cell silhouette extension, but does not bound world work by image error.
No native port or full-pose/device admission follows this cost probe.

## Data and query

The projector is fixed by source bounds, a deterministic skew of their shortest
axis, the provided up direction and a stand-off of16times the largest extent.
The stand-off shapes the index; it is no camera reuse radius or LOD error bound.
All original triangles remain, including backfaces and hidden surfaces. The asset
does not bake lighting or require a new capture when the observer moves. Tests
query the same two-sided source from opposite sides and keep hidden depth layers.

Each record holds a projected triangle boundary, three reciprocal-depth corner
values, original post-vertex UV corners, a shading-normal word, an explicit flat
material category and full source/native-face provenance. Nonzero metre fields
and nonwhite RGBA deltas have sparse corner tables. Surface fields interpolate
with perspective-correct barycentric weights. Categories are never averaged.
Inverse-depth corners are shared in value at common source vertices; independently
rounded plane coefficients would introduce cracks. The CPU decoder derives plane
coefficients in Float64. A Float32 native implementation remains unverified.

Projected one-pixel bins contain variable face lists over conservative boundary
rectangles; no depth-layer cap is imposed. Bin pitch changes lookup work rather
than replacing surfaces with cell-sized patches. A ray first clips against the
field's convex projected frustum, then gathers bins over the projected segment's
rectangle. Plane intersections and trim tests on represented boundaries follow;
attributes are evaluated only at the selected front-facing hit. Both ray directions
and parallel-depth rays are supported. The rectangle is an explicit overestimate,
not an efficient DDA or a bound on the number of surface tests per pixel.

Limits:131072triangles,262144bins,4194304memberships,1048576raw memberships per
query batch,128rays per batch. Unsupported frames, degenerate/turned quantised
boundaries or budget overruns request a finer/original path; no surfaces are
silently dropped. The coordinate box±1e7m is the local numeric domain, unrelated
to the test-camera box. Identity/numeric-domain checks precede payload IO.

The ready field query uses its own corners/planes/tables, without the original
XYZ vertex/index buffers or source recipes. Its information content still follows
original surface count. It is an analytic comparison baseline for future sampled
fields, not evidence that an LDI has become independent of source complexity.

## First cost comparison and stopping decision

The unchanged complete native cell has576sources/14169triangles. The existing
probe-pose selector supplies static20,moving20,moving21 at1280x720,2x2subsamples.
Float64 and actual Float32 products use identical content and projection. Both
have no lost/added hits or changed source/material identities in those three
poses. Continuous-field/depth errors and two changed Lambert lights remain in
the raw reports; this is not native PBR/filtering/opacity/TAA/shadow validation.

| Stored arrays | Bytes | Scope |
|---|---:|---|
| Original native |1028316|Actual24Bvertices,u32indices,metre stream|
| Original exact u16 alternative |943302|Same original attributes|
| Analytic Float64 field |2144820|All records, index and lineage|
| Proposed Float32 field |1265376|All records, index and lineage|
| Exact factorized storage control |665002|All original geometry/post-vertex fields and same lineage|

The factorized control stores8321unique bit-identical XYZ rows and7315unique
UV/metre rows, with separate corner index streams. Flat normal/category fields
and the same source/face provenance remain. Nonwhite RGBA is retained when present.
Roundtrip checks compare every represented corner's bits; the actual arrays are
saved. This is a reachable storage alternative, not a new native format, API or
GPU timing claim. It does not propose a universal individual-3D rendering path.

Even free deletion of the Float32 field's UV table and metre tables/index leaves
867678B, exceeding this exact control. That only rejects attribute sharing alone
as a rescue of the current layout; it is no global compression lower bound.
The field also needs680304Bderived CPU plane/normal/extent arrays; library/query
allocations add further memory. Containers/JSON and process peaks are reported
separately from these array payloads.

The initial Float32 probe visits27844rays and tests8521854candidate planes over
the three poses. Rectangle traversal repeats references before per-ray deduplication.
These are actual query operations, not native triangle-raster or GPU timings.
Such costs do not justify another full120-pose or device gate for this proposal.
The complete #899acceptance remains unchanged and open. No resolution/layer,
coordinate-packing or traversal sweep is used to keep this exact proposal alive.

Next compare genuinely image/error-dependent common fields over spatial groups,
including disocclusion and filtered material response. Existing original archives
contain64original BSC3 form blocks for the supplied parent, plus the selected
native view. These are different input types, not64native reference renders.
The forms should feed aggregate comparisons before asking for more exports.
A projection index over every original surface
does not solve global amortization or the residency/geometry-build problem.
Logical world, casters, dynamic contents and both target-device budgets stay open.

## Reproduction and references

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/render .venv/bin/python -m unittest test_projective_field
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src .venv/bin/python test/experiments/render/compare_projective_field.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --output /tmp/outshine-research-projective-field-<commit>
```

Use a fresh explicit system-temp path. Both precision variants save complete
products, original/candidate arrays, reports and local material/normal PNGs;
open these actual images. No doc/README images are published. Product loading
is independently testable with the emitted request; the original export is only
used by the bake and reference comparison. Existing NumPy/Trimesh/Rtree/Pillow,
Shapely/SciPy reference dependencies and the local reference package are required.

SIGGRAPH-first references: existing Shade et al.1998LDI and Chang et al.1999LDI
Tree; Oliveira/Bishop/McAllister2000*Relief Texture Mapping*, DOI10.1145/344779.344947,
local `doc/references/geometry/siggraph/2000-relief-texture-mapping.pdf`, pages2/3
with equations and diagram inspected. Our analytic trim/rational-field control
does not implement the paper's image pre-warp, reconstruction or their guarantees.

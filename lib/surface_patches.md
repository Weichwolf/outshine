# Exact resident height-surface patches (#542)

`geometry.surface_patches.SurfacePatch(patch_id, frame, kind, mesh, face_data={})`
owns primitive local-face attributes and prepares a geometric index over an
existing immutable `Mesh`. `frame` is a `LocalFrame`; `kind` is explicitly
chosen by the caller. There is no highest-hit ground/deck/roof selection.
Source provenance, transport access, reservations, loading and LOD remain outside.

Each patch has 1–65536 upward projected nondegenerate faces and at most 65536
vertices, finite metric coordinates bounded by 1e9, no positive-area overlap or
unmatched edges and exactly one supplied height at every repeated XY coordinate.
Holes and disconnected components are allowed. At most eight numeric/bool
scalar attributes of at most eight bytes each accompany every local face.
Attributes are independently owned immutable byte-backed arrays. Meshes follow
the existing immutable `Mesh` contract and remain shared by identity.
`bounds_xy_m` supplies conservative resident geometry bounds.

The strict constructor first tries a sufficient planar boundary witness (#678).
After the same finite-coordinate, index, upward-face and single-height checks,
oppositely directed full internal edges must cancel, leaving one simple CCW
cycle. Robust GEOS orientation checks every triangle in blocks of at most1024.
Winding-number additivity then excludes positive-area overlap and missing
interior coverage. Edge-work temporaries expire before ring-orientation batches.
Holes, disconnected/touching components and any unavailable witness still use
the general GEOS coverage predicate. No persisted validity flag, public signature,
geometry, attribute ownership or replacement tolerance changes.

Starting points: Lipman, [SIGGRAPH2012, section5/theorem5.1](https://www.wisdom.weizmann.ac.il/~ylipman/BoundedDistortion/bounded_distortion_may_1.pdf)
and [SIIMS2014, theorem1/figure1](https://www.wisdom.weizmann.ac.il/~ylipman/2014_SIIMS_bijective_93975.pdf).
The relevant PDF pages5and4 were inspected visually. Positive triangle
orientation alone is insufficient; our exact oriented-chain argument requires
the entire simple boundary as well. The general manifold theorem is not invoked
as a substitute for checking these input conditions. Tests retain independent
GEOS decisions and exact rational orientation adversaries.

`SurfacePatches(root, max_patches=1024)` starts one finite resident surface.
`view.replace(patch, removals, max_removed=2048, max_new_faces=8192)` returns a
new immutable view and work report. `removals` maps old patch IDs to distinct,
currently active local face indices. Frame, kind, attribute names and dtypes
must agree; patch IDs cannot be reused. New whole XY coverage matches the
removed region (existing 1e-7 m² numerical check), and every complete unordered
XYZ boundary edge is exactly numerically equal. Internal selected patch seams
cancel. Heights/triangulation may change inside the removed region. No edge
splitting, tolerance weld or approximate boundary repair occurs in this kernel.

Replacement uses `coverage_union_all` for the two selected XY regions (#678).
The root is a validated coverage; subsets remain non-overlapping and edge-matched.
An accepted replacement preserves the complete exact XYZ boundary and XY domain,
so the active surface retains that invariant inductively. New patches independently
pass the strict constructor. Consequently these inputs satisfy the optimized
union's preconditions; arbitrary unchecked polygon collections do not. The same
1e-7m² symmetric-difference test remains. Holes, point contacts and disconnected
components are covered by public replacement controls. The independent transport
reader can force the previous general-union path. [Methods and costs](../test/experiments/transport/stair_composition_comparison.md).

Unchanged patches and their mesh buffers remain shared; persistent snapshot
updates copy changed active face masks. Local validation gathers temporary
triangle/index data. `retained_mesh_copy_bytes` counts only whole retained Mesh
buffers duplicated in the new view, not those validation copies. Old views stay
valid. Activity counts avoid scanning
whole masks just to decide whether a patch can answer a query. Failure raises
without modifying the old view. Resident patch capacity is 1–4096, independent
of local solver/source budgets. This does not extend the initial finite domain,
evict history, compose arbitrary overlapping levels or replace a world index.

`view.query((xmin,ymin,xmax,ymax), max_faces=2048,
max_candidates=2000000, method="index")` returns `SurfaceQuery`. Bounds have
finite positive area. `index` uses prepared per-patch STRtrees; `scan` evaluates
all faces of intersecting resident patches. Both filter inactive faces, order
results by patch insertion/local face ID and return the same complete geometry.
The candidate budget is conservatively checked against the whole intersecting
patch face count before GEOS emission. Exhaustion yields no partial mesh and an
explicit reason. Returned faces/face keys are bounded; exact XYZ vertices are
canonicalised in the local result, allowing `geometry.SurfaceSampler` to recognise
shared edges across patches. No proximity merge is performed.

`view.nearby(shape, distance_m, ...)` uses the same capacities, output order
and failure reasons for exact XY distance queries. Supply a finite nonempty
valid 2D Shapely geometry of at most 65536 coordinates and a finite nonnegative
distance of at most 1e9 m. The index uses GEOS `dwithin`; the scan evaluates
the same predicate. A bounding-box expansion only rejects distant patches;
it does not replace the distance predicate or approximate a circular buffer.
Distance zero includes boundary contact. Complete triangles are returned.

`view.faces(keys, max_faces=2048)` dereferences an explicit bounded list/tuple
of stable face keys in canonical patch/local-face order. Duplicate, unknown,
out-of-range or stale keys are errors, preserving the old snapshot. Empty
input yields `outside_surface`; exhausted capacity yields no partial mesh.
This supports exact dependency closure without gathering the whole field.

The query includes complete intersecting triangles, not clipped polygons.
`mesh=None` with `outside_surface`, `candidate_budget_exhausted` or
`local_face_budget_exhausted` is explicit absence. It never substitutes height
zero. Work reports count patch bounds, conservative candidates, actual index
visits or scan tests, emitted faces and explicit extraction/canonicalisation
bytes. These are scoped operations, not equal-cost instructions or every native
copy. `face_keys` contains stable `(patch_id, local_face_id)` pairs, not global
source bits. The caller binds those identities to original sources and surface
semantics. Generic Float32/topology/physics acceptance is still the caller's job;
exact shared boundaries remain shared under identical coordinate conversion.

This is a resident finite geometry operation. It is not AssetCache, a native
runtime codec, persistent streaming, complete network growth or an LOD algorithm.
Retired patch/index history occupies memory until its owning view is released;
storage/residency policies belong to #10, LOD to #12. Existing Plan/Reservation/
Provider APIs are unchanged.

## Optional generated-product codec (#678)

`geometry.surface_patches.prepared` supplies `pack_surface(view, compressed=False,
max_bytes=64*1024**2)` and `restore_surface(payload, expected_sha256=...,
frame=LocalFrame(...), max_bytes=64*1024**2)`. Import this module explicitly;
ordinary patch construction does not import this codec. The format
is `outshine-finite-surface/1`: numeric NPZ members, bounded JSON bytes and no
pickle, validity flag, file lookup, cache manager or hidden source fallback.

The caller's existing product record must bind the exact known-valid generated
bytes, format and frame. Normal strict construction establishes geometry before
publication. A digest calculated from an unknown candidate, or supplied beside
it by an unknown source, does not establish validity. Unknown/mismatched products
must use strict construction or rebuild. An ordinary hit on a trusted generated
product needs no manual per-asset admission and no second geometric producer.
Independent cold GEOS reconstruction remains a research/test check, not a new
runtime cache protocol. Native AssetRecord integration belongs to #10/#48.

Restore verifies the expected digest before parsing; ZIP member names, duplicate
members, compression, expanded sizes, NPY version, exact byte lengths, primitive
dtypes and per-member shape/capacity limits before numeric array loading. It then
checks finite Mesh coordinates/indices, exact frame, patch IDs, attribute schema
and active masks. The default byte budget bounds both payload and expanded array
bytes; callers may explicitly choose 1 byte–512 MiB. This is not a total Python,
GEOS or process-RAM bound. Query polygons/STRtrees are rebuilt and cost work.

All restored mesh, attribute and mask buffers are owned immutable byte-backed
arrays. Geometric coverage/composition validity is inherited from the exact
known-valid product; restore skips SurfacePatch geometry validation and old
replacement history. Subsequent new patches and replacements use the unchanged
strict constructors. Whole states retain patch insertion order, local face keys,
XYZ, indices, masks, attributes, capacities and query behavior. File IO, product
storage, source provenance, residency and invalidation remain caller-owned.
The [transport comparison](../test/experiments/transport/stair_prepared_surface_comparison.md)
records the independent experiment and its library-delivery follow-up.

SIGGRAPH starting point: Lindstrom et al. (1996), page 4 figure 3, inspected
visually: local height-field geometry and explicit edge deltas. The present
exact finite facet composition is independently defined; it implements none of
that paper's simplification, projected error, morphing or hierarchy algorithm.
The [real transport experiment](../test/experiments/transport/stair_patch_comparison.md)
records input/product hashes, local/whole controls, costs and actual images.
The [local generator adapter](../test/experiments/transport/stair_local_comparison.md)
uses exact distances, whole-pier dependencies and five-metre auxiliary ground.

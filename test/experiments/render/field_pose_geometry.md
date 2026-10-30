# Whole-field moving-pose geometry

Owner: #1821, Parent #1797, native consumer #1570. Python only. This extends
the #1663 projection identity, using its directed intervals, rather than fitting
another distance/angle domain. #1720 owns capture inclusion; #1724 owns the
conditional packed-operand arithmetic envelope. Their existing SIGGRAPH sources
and qualifications remain applicable; no new compiler or optical error theorem.
In particular, Duff, SIGGRAPH1992, *Interval Arithmetic and Recursive Subdivision
for Implicit Functions and Constructive Solid Geometry*, local
`doc/references/vegetation/siggraph/1992-interval-arithmetic-csg.pdf`, PDF page2,
was inspected again including the interval rules and rounding discussion.

## Small operands before fine IO

Keep these with the existing immutable source/product descriptor and leases:

| Operand | Units / binding / owner |
| --- | --- |
| Whole cohort identity, completeness and original remainder | Existing BaseKey, cell/source set, version and placement; generator/World. Unknown originals retain their owner. |
| Complete source hull | First use the capture-derived oriented hull below, with no added persisted operand. An optional tighter local AABB is two 3D endpoints, 48 bytes as Float64, in the SAME field coordinates as the plane. #1724's stored-position AABB path additionally requires binary32 normal-or-zero endpoints and producer attestation. |
| Capture volume and raster | Bound capture eye/basis, x/y extents, near/far, width/height, sample convention; #1720/#1724. Never infer a complete source hull from first-hit Raw depth. |
| Physical support plane | First candidate: the existing flat plane, c=Centre, d=TowardEye, n=d, DepthCoefficients=0. No fitted coefficient is needed. The optional tilted candidate uses its actual point/normal. |
| Current projection | Finite eye, right/up/forward basis in the same field frame; actual submitted lens matrix, including jitter, and current target width/height. Input rows use positive camera forward. |
| Error budget | Explicit geometric L1 pixel budget from the consumer. The study's 0.25 px is a diagnostic choice, not a product-quality rule. |

## First candidate from the existing header

The producer's complete source capture domain is |right|,|up|<=H and |toward|<=2H
around Centre. This is the domain used by native ImpostorBaker::ProjectionFor
and its padded projection, not a hull recovered from first-hit depth. Its binding
must cover all original sources, stored-position rounding and capture inclusion;
unknown or unsupported originals still keep their owner. The ordinary metadata
already contains Centre, HalfExtentM, selected direction and Pixels. The first
flat candidate requires no fit, no local tight hull and no new persisted field.

`field_capture_hull.py` derives the eight vertices of this existing oriented
domain once per immutable descriptor. It exactly inverts the finite capture
axes as rationals and rounds each vertex outward to binary64 intervals. The
mathematical vertices' convex hull encloses the complete source. Pass these
interval vertices to `projection_bound(source_hull=...)`; expanding them into
a world-axis AABB first discards important correlation. The study compares both
valid bounds and their admission breadth on the same supplied header.
Native code may use an appropriate proven interval inverse; the Python reference
does not prescribe a rational runtime implementation or a new metadata cache.
The derived numeric interval vertices occupy384B, held with the existing view
descriptor/lease, not written into its package. Pose changes reuse them.

Two optional tighter adapters must preserve coordinate/key ownership. The current
ImpostorViewAssets receipt bounds are ECEF (BuildingImpostorSource::ResolveMetadata),
whereas Centre/support are field-local; they cannot be used as one local AABB.
A tighter local hull or outward inverse-frame enclosure can be considered if
the header candidate proves too loose. Neither is required for the first path.
Similarly, fitted DepthCoefficients currently arrive from the fine atlas. Do not
mutate shared metadata after that fit: FieldKey depends on Receipt.Package, which
would change the address of the just-produced field. If a measured need justifies
a fit, its small bound view metadata must attach independently to the fixed
Fieldkey. This experiment adds no sidecar or format. Source/frame/capture changes
invalidate the derived hull; live World/light bindings remain master's concern.

For native Lens::Projection column-major P, positive-forward input uses
`P * diag(1,1,-1,1)`. Pass the actual packed values, not a lens reconstructed from
JSON FOV. Keep the native camera rebase, Field-to-World transform and shader
rounding within #1724/#990 or another qualified adapter. They are not certified
by this ideal geometric calculation. As a separate fixed-carrier check the study
uses the actual #1724 identity model, packed view and rounded shift directly:
eye = -shift, identity basis, projection = packed view. No matrix is recreated.

## Admission order and ownership

1. Bind actual live World/source/material/light state using the existing consumer
   contracts. Historical fixture revision nulls do not prohibit valid live bindings.
2. Check the whole original cohort capture hull once for its bound capture using
   #1720 and the conditional arithmetic scope #1724. Reuse that result while the
   binding stays unchanged. An inconclusive coarse hull can refine metadata; a
   partial subgroup cannot relabel an already captured whole-group field.
   In particular an expanded AABB's refusal does not revoke an independently
   established whole-source capture binding. The header hull is a conservative
   pose operand, not a replacement for that producer/capture qualification.
3. Evaluate the current pose/lens/raster against the whole source and chosen plane.
   No original vertex, texel, field, mesh or shape is loaded by this kernel.
   Unknown/invalid operands, overflow, projective-eye crossing, uncertain near/far
   inclusion or excess geometric error retain the complete original owner.
4. A positive result is a **geometric candidate**, permitting the existing bounded
   load/preparation path when its other qualifications hold. It does not assert
   coverage, disocclusion, source shading, filter integration or image error.
   These remain the existing #1797/#1570 contracts; no additional universal
   material campaign or missing historical revision is made a blanket prerequisite.
5. Publish only a ready qualified replacement for the same live revision, with
   exclusive ownership. Until then originals remain. When leaving the admissible
   region, keep/request the existing appropriate parent/view/refinement. Readiness,
   prefetch, hysteresis, cancellation and bounded work belong to streaming.

Current side-plane clipping is allowed: the existing raster support is clipped
by the viewport. Positive w and reverse-Z depth inclusion for both original and
support are required here. Native BuildProjectedRadianceSupport still owns
pixel-aligned support, one-pixel filter expansion, jitter and viewport clipping.
This function never emits main depth, shadow depth or occlusion authority.
Actual representation/UV reconstruction and GPU rounding need their own enclosure;
the consumer must not silently equate a different affine reconstruction with q.

## Geometric bound

For original point p, support point q=p-d*h(p), with
`h(p)=dot(p-c,n)/dot(d,n)`. The denominator must exclude zero. Let homogeneous
projection of p be X and the projection of direction d (homogeneous zero) be K.
Then Q=X-h*K, and the axis displacement is exactly

`(pixels_i/2) * abs(h * (K_i*X_w - K_w*X_i) / (X_w*Q_w))`.

h, X, Q and each parenthesized numerator are affine in p. Their ranges over the
complete convex source hull are enclosed by directed calculations at its eight
interval vertices (or ordinary AABB corners when supplied). Separate
maximum magnitudes and strictly positive lower w values bound their product and
quotient throughout the box. We do **not** assert that the displacement's maximum
itself occurs at a corner. The L1 sum also bounds Euclidean pixel displacement.
The same expression covers orthographic, perspective, unequal axes and jitter,
and a tilted physical plane without assuming zero depth displacement.

This only bounds continuous geometric point displacement. A first-hit field can
miss newly exposed surfaces despite a small bound. Capture sampling, support
construction and filter covariance remain separate from this number. There is
no new distance cutoff, camera calibration or claimed C/T/material tolerance.

## Checks and reproduction

The study prepares a control header once from the supported original cohort
centre and supplied capture extent. All1,891 complete source AABBs are separately
checked against its full source domain at preparation; this is not a relabelled
historical atlas or a new native capture. Pose queries consume the header-derived
hull, then open94,999 original stored vertices as an independent numerical
opponent. Fourteen tight-hull pose/plane controls and fourteen coarse header
AABB/oriented-hull controls are checked. The tilted plane is only an optional
supplied geometric control. The flat header path is the first candidate. Unresolved
sources 3825/3827 and Cell33's unqualified opacity remain outside replacement
authority. Exact-rational tests separately check corners and interior points,
rotated frames, dense projection, near/eye crossings and raster changes.
The opened diagnostic compares complete projected original points and their
plane counterparts. It is not a new native World render or image-quality gate.

Use the existing .venv, NumPy/Matplotlib and fixed input dependencies; Memray only
for costs. PYTHONPATH includes lib/src and test/experiments/{render,buildings,transport}.
Set OPENBLAS_NUM_THREADS=1 and OMP_NUM_THREADS=1. Outputs must be fresh host temp
paths, checked by the shared canonical experiment_output helper.

```sh
.venv/bin/python -B -m unittest test_field_capture_hull test_field_pose_geometry test_field_view_projection \
  test_capture_source_support test_capture_box_envelope
nice -n 10 .venv/bin/python -B test/experiments/render/field_pose_study.py \
  --output /tmp/outshine-field-pose/study
.venv/bin/python -B test/experiments/render/field_pose_cost.py \
  --output /tmp/outshine-field-pose/cost
```

The cost opponents include the existing #1663 special-case perspective bound on
the same tight whole hull, same raster/focal problem and error requirement, plus
capture-header AABB versus oriented hull and one-off header derivation. Seven
normal-priority rounds per method and separate native Memray traces report CPU,
wall, allocation events, requested bytes and scoped high-water mark. Inputs and
held opponents precede tracing. All pose methods visit eight corners and zero
fine vertices; header derivation is a descriptor-change scope, not a pose scope.
No native speedup, cold-IO, total copied bytes, physical traffic or process/UMA
peak is inferred. Numeric source bounds are 48 bytes; general supplied plane,
eye/basis, projection and raster occupy 304 additional bytes if all are stored
as separate Float64 values plus two UInt32 counts. They mostly already exist in
the consumer. The flat header choice adds zero on-disk bytes. Binding keys,
statuses, Python-object and metadata-container overhead are separate.

Architecture acceptance: one Python geometric kernel consumes the existing
CapturePlane and directed scalar operations. Source/capture preparation, fine
opponent, plots and costs remain in separate experiment drivers. No new library,
dependency, product cache, native implementation or frame scheduler. The old
specialized kernel stays valid; generality is not a performance win by itself.

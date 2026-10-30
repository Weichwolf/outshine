# Whole-field moving-pose geometry

Owner: #1821, Parent #1797, native consumer #1570. Python only. This extends
the #1663 projection identity, using its directed intervals, rather than fitting
another distance/angle domain. #1720 owns capture inclusion; #1724 owns the
conditional packed-operand arithmetic envelope. Their existing SIGGRAPH sources
and qualifications remain applicable; no new compiler or optical error theorem.

## Small operands before fine IO

Keep these with the existing immutable source/product descriptor and leases:

| Operand | Units / binding / owner |
| --- | --- |
| Whole cohort identity, completeness and original remainder | Existing BaseKey, cell/source set, version and placement; generator/World. Unknown originals retain their owner. |
| Complete stored-position source AABB | Two 3D endpoints, 48 bytes as Float64, in the SAME field coordinates as the plane. Must enclose every original stored vertex; #1724 also requires binary32 normal-or-zero endpoints and producer attestation for the whole stream. |
| Capture volume and raster | Bound capture eye/basis, x/y extents, near/far, width/height, sample convention; #1720/#1724. Never infer a complete source hull from first-hit Raw depth. |
| Physical support plane | Point c, capture-ray direction d and normal n; 72 bytes as Float64. Can be derived from the existing centre/direction/extent plus three fitted DepthCoefficients. Must describe the actual chosen support; a flat plane is an explicit choice, not an alias for an unknown fitted plane. |
| Current projection | Finite eye, right/up/forward basis in the same field frame; actual submitted lens matrix, including jitter, and current target width/height. Input rows use positive camera forward. |
| Error budget | Explicit geometric L1 pixel budget from the consumer. The study's 0.25 px is a diagnostic choice, not a product-quality rule. |

The native source audit at d58ed84a3 finds two concrete adapter gaps. The existing
ImpostorViewAssets receipt bounds are in ECEF (BuildingImpostorSource::ResolveMetadata),
whereas its Centre and fitted support are in field coordinates. They must not be
mixed. Retain the original local 48-byte hull at publication, or use a proven
outward inverse-frame enclosure of the existing world hull (possibly looser).
Do not rescan fine geometry each frame. Producer source bounds alone still need
their stored-position/rounding enclosure contract.

Secondly, RadiancePlaneFit computes the three DepthCoefficients from the loaded
atlas; ImpostorViewAssets::Description does not contain them. A pre-IO decision for
that fitted support needs those three bound coefficients (24 bytes as Float64),
or an already known support with its own geometric bound. No duplicate format,
index, scheduler or cache is proposed here; native owners choose the existing
metadata carrier. Source/source-frame/capture changes invalidate these operands.

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
complete AABB are enclosed by directed calculations at eight corners. Separate
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

The study first decides using one whole 48-byte hull for 1,891 supported sources,
then opens the 94,999 original stored vertices as an independent numerical
opponent. All fourteen pose/plane controls are checked. The tilted plane is a
deliberate supplied geometric control, not a new fitted native field. Unresolved
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
.venv/bin/python -B -m unittest test_field_pose_geometry test_field_view_projection \
  test_capture_source_support test_capture_box_envelope
nice -n 10 .venv/bin/python -B test/experiments/render/field_pose_study.py \
  --output /tmp/outshine-field-pose/study
.venv/bin/python -B test/experiments/render/field_pose_cost.py \
  --output /tmp/outshine-field-pose/cost
```

The cost opponent is the existing #1663 special-case perspective bound on the
same whole hull, same raster/focal problem and same error requirement. Seven
normal-priority rounds per method and separate native Memray traces report CPU,
wall, allocation events, requested bytes and scoped high-water mark. Inputs and
held opponents precede tracing. Both visit eight corners and zero fine vertices.
No native speedup, cold-IO, total copied bytes, physical traffic or process/UMA
peak is inferred. Numeric source bounds are 48 bytes; general supplied plane,
eye/basis, projection and raster occupy 304 additional bytes if all are stored
as separate Float64 values plus two UInt32 counts. They mostly already exist in
the consumer. Binding keys, statuses and metadata-container overhead are separate.

Architecture acceptance: one Python geometric kernel consumes the existing
CapturePlane and directed scalar operations. Source/capture preparation, fine
opponent, plots and costs remain in separate experiment drivers. No new library,
dependency, product cache, native implementation or frame scheduler. The old
specialized kernel stays valid; generality is not a performance win by itself.

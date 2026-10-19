# Original triangles visible from an observer region

Owner: [#844](https://github.com/Weichwolf/outshine/issues/844). This is an exact
small control for a possible replacement of point-capture visibility selection.
It does not enlarge the native capture's `EyeRadiusM` or validate its patches.

`CaptureBuildingSurfaces` traces six cube views from one eye, then emits captured
plane rectangles or sampled visible source faces. Previously hidden faces may
become visible after translation. A parallax bound on emitted points alone
cannot prove coverage of omitted source geometry.

The SIGGRAPH starting point is Schaufler et al.,
[Conservative Volumetric Visibility with Occluder Fusion (2000)](https://groups.csail.mit.edu/graphics/pubs/siggraph2000_consvol.pdf),
sections 2–3 and figure 2 (also visually inspected). It distinguishes point-sampled
visibility from a conservative potentially visible set for an observer region.
The control here uses a single original opaque triangle and supporting planes;
it implements neither the paper's voxel representation nor occluder fusion.

## Exact predicate and scope

`covered_from_ball(blocker, target, eye, radius_m, opaque=True)` consumes finite
Float64 triangles and an eye in one Euclidean frame. Radius is a caller-supplied
movement domain, not a world constant or changed pixel tolerance. `opaque=True`
attests that the complete blocker triangle is opaque and rendered from every
admitted eye (including material masks, culling and source lifetime). Unknown
opacity returns false. The proposed draw set retains all original triangles
except targets certified covered throughout the ball. Uncertain inputs keep
original geometry; they never become sampled patches or holes.

`cull_backfaces=True` additionally requires the original triangle winding to face
the whole observer ball. Its exact plane sign rejects a back-facing blocker;
the existing ball/plane separation proves that the winding cannot flip inside
the admitted ball. This supplements the caller's opacity/material admission.

The eye ball must lie strictly on one side of the blocker plane, with the whole
target strictly behind that plane. For each blocker edge A→B and target vertex P,
the side predicate is

`g(E,P) = sign * cross(B-A, P-A) · (E-A)`.

It is affine in E and in P. For fixed P its minimum over the closed ball centred
at E0 is `g(E0,P) - r*length(cross(B-A,P-A))`. Positive g and
`g*g > r*r*dot(n,n)` prove strict positivity without rounding a square root.
The three target vertices suffice by convexity. Thus every eye-to-target segment
crosses the interior of the blocker triangle. Float inputs are converted to exact
Python Fractions; these predicates have no floating-point acceptance epsilon.
This is a sufficient condition, not maximal visibility or an optimal radius.

At most 13 exact plane tests per pair. The predicate has fixed 3×3/3 input shapes;
no scene index, all-pairs search, cache or native code is introduced. Original
array operands are 168 bytes per pair; Python Fraction allocations, bit arithmetic
and total peak memory remain unmeasured. The Fraction implementation is an oracle,
not a proposed runtime implementation. Native arithmetic and candidate selection
need separate cost and correctness evidence before adoption.

## Reproduction

```sh
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error -m unittest test_capture_region_visibility
OPENBLAS_NUM_THREADS=1 PYTHONPATH=test/experiments/render \
  .venv/bin/python -W error \
  test/experiments/render/measure_capture_region_visibility.py \
  --output /tmp/outshine-capture-region-control
```

The driver saves its synthetic triangles and compares full, point-omission and
ball-omission geometry at identical 1280×720, focal1044.33155px, before and after
6m translation. Two geometrically scaled scenes distinguish near and far bounds.
An independent triangle-plane/edge reader compares every pixel's coverage and
exact depth; tests independently solve segment intersections at 4096 random
ball-boundary eyes. Sampling verifies the implementation; supporting-plane
inequalities supply the whole-ball argument.

Actual source/native capture pairs, stable Source/Part/Face ownership, original
materials/culling, thin/occluded world cases, patch coverage/parallax, rotation,
regional selection, native cost and full-world/device gates remain open. The
source geometry must remain available for shadows, reflections and interaction;
a primary-visibility omission grants no deletion of those consumers' geometry.

## Original native geometry control

`measure_native_capture_region.py --fine <explicit-original-Fine-export> --output
<system-temp>/native-region` reads hash-checked original streams and exact native
source ranges using existing readers. It preserves original source ordinals and
all triangle indices. Part/Face membership is not inferred. The original eye
position receives a declared cell-facing test orientation; a second declared
low-eye camera supplies the near countercase. Both retain 1280×720 and the
original focal length. Pose, rightward movement and a five-degree turn are inputs
to comparisons against the complete original triangles, treated as opaque.

`capture_region_candidates.py` finds candidate blockers through projected
triangle boxes and perspective-correct centroid depths. These are only proposals:
every omission still passes the exact predicate. Misses and near-plane crossing
retain originals. Input cap131072 triangles; query batches are chosen *before*
allocation so even all-overlapping boxes emit at most262144 query pairs. The
total limit8388608 pairs is also checked before a batch; remaining targets stay
unresolved and retain original geometry if that limit cannot admit the batch.
The index is constructed once per observer; no all-pairs ray arrays are needed.
GEOS/index allocation and total peak remain distinct from explicit array bytes.

The driver records proposals/plane tests/arrays, retained index bytes, timings and
process RSS peak, and saves face-to-source bindings plus point/region masks. It
checks each pixel's coverage and exact reversed depth, independently solves
segment/triangle systems at16 random ball-boundary eyes per admitted target, and
renders cropped nearest-neighbour diagnostics from the full-resolution checks.
Rasterized pixels and sampled segments test implementations; exact supporting
planes provide the whole-ball argument. Masked materials, native patch emission,
shader precision and device admission remain separate. An omission may itself
use an omitted blocker: each covered ray then meets a strictly nearer opaque
triangle; this chain terminates among the finite original triangles.

This control measures a sufficient single-blocker subset, not the best possible
PVS or native CaptureBuildingSurfaces output. Real benefit must be compared with
the actual captured geometry and its original source/part/face contract before
choosing a runtime replacement. Source-only oracles do not close #844.

# Static facing before vertex execution (#12)

Python comparison requested in [#12](https://github.com/Weichwolf/outshine/issues/12#issuecomment-6258173325).
The primary source is the local [SIGGRAPH 2015 GPU-driven rendering course](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
PDF pages 22–24, including the diagram. Those pages were inspected visually.
The paper's finite frustum heuristic is deliberately replaced with unbounded
regions; its published removal percentage is not an Outshine measurement.

## Inputs and eligibility

`facing_inputs.py` reads the existing SHA-checked schema-10 building artifacts:
24-byte vertices, 52-byte serialized cluster records, original level-0 cluster
ranges and wall/roof index streams. The cache serializer interleaves Self/Parent
centre components; the renderer's 48-byte cluster table has a different layout.
No vertex, index order, cluster or stream is rebuilt. Cell source catalogs remain
catalogs: they do not establish the source or material owner of each triangle.

The resident transport NPZ references retain their full bodies, source-ID lists,
top-face ownership and optional material arrays. Their contiguous 128-triangle
comparison ranges are experimental, not existing native transport clusters.
No native export pipeline or product change is introduced.

Culling requires explicitly static, one-sided, trusted-winding geometry and a
positive nonsingular affine model. Unknown state, deformation, two-sidedness,
reflection, singularity, ill-conditioning, invalid cameras and cameras inside
or on the original sphere retain everything. A sphere not proven to enclose its
actual vertices also retains everything; stored bounds are never enlarged here.
The query camera must already be in original object space. Computing that camera
and maintaining consistent winding/precision in native code remain consumer work.

The artifacts lack complete current runtime cull/material/motion/instance state
and exact per-triangle building ownership. Accordingly every actual-eligibility
query defaults to KEEP. Reported geometric reductions assume the explicit
eligibility condition; they are not reductions of the native Tokyo audit.

## Eight-byte cone candidate

The representation is three signed bytes `A`, one unsigned byte `K` and one
Float32 scalar `u`, eight bytes total. Let `S` be the original sphere centre,
`N_i` a triangle's unnormalized geometric normal and `p_i` its first vertex.
Shading normals are irrelevant. Choose a rounded axis near length 127 and bake
upward bounds such that

```
K >= max_i |127 N_i/|N_i| - A|
N_i dot A > 0
u >= max_i (N_i dot (S-p_i)) / (N_i dot A)
P = S - A*u
```

Then each plane satisfies `N_i dot (P-p_i) <= 0`. If
`A dot (C-P) + K*|C-P| < 0`, every triangle faces away from camera `C`.
Rounding enlarges `K` and raises `u`; wide/opposed/degenerate normals disable
the cone. Axis choice is a simple mean-normal heuristic, not an optimum claim.
All-zero reserved words decode as KEEP.

The two reserved float words in the native GPU table offer eight physical
bytes. Using them would require an agreed **bit-preserving** layout; arithmetic
float casts or NaN canonicalization must not corrupt packed bytes. Cache version,
bindings, instance eligibility and native Float32/FMA error bounds are undecided.
No native implementation is included.

## Six unbounded masks

Each triangle has one possible-visibility bit per cube face. For face axis `a`
and sign `s`, write `C-S = r*v`, with `v_a=s`, other components in `[-1,1]`.
Outside the enclosing sphere of radius `R`, `r >= R/sqrt(3)`. For each triangle,

```
b = N dot (S-p)
M = s*N_a + sum_other_axes |N_j|
```

If `M > 0`, retain the triangle: arbitrarily distant cameras could see it.
Otherwise discard only when the upper bound of `b + (R/sqrt(3))*M` is strictly
negative. Dominant-axis uncertainty and cube-face seams OR all possible faces.
Degenerate triangles remain possible. There is no finite far truncation.

Payload is `6*8*ceil(triangles/64)` bytes: 96 bytes for 128 triangles, versus
eight cone bytes. That packed payload count excludes any variable-stride address
table; a fixed 96-byte stride per cluster needs no such table but pads short
clusters. A normal query reads one face (8/16 bytes), seams up to three faces.
Mask compaction additionally needs bit tests, per-triangle prefix/rank work and
retained original index reads/writes. It must preserve triangle order and the
existing batch/material grouping. The existing native compactor already copies
indices; this experiment counts the candidate retained bytes but does not model
native scheduling, scan implementation, allocation events or measured bandwidth.

## Checks and reproduction

```
.venv/bin/python -m unittest discover -s test/experiments/render -p test_static_facing.py
.venv/bin/python test/experiments/render/compare_static_facing.py \
  --buildings /path/to/existing/frozen-inputs \
  --transport /path/to/existing/resident102 \
  --output /tmp/outshine-research-facing-<commit>-comparison
```

The oracle uses outward Float64 determinants, falling back to exact dyadic
`Fraction` arithmetic when the sign is uncertain. Independent scalar exact
determinants check both the filter and exclusions. Zero/tangent remains possible.
Tests include 128-bit masks, quantized apex planes, reflected/singular/unknown
states, translated geometry, cube seams, inside/boundary/near/far cameras and a
concrete false exclusion from finite frustum truncation.

Mesh comparisons check every cluster at six near and six far axis positions,
eight cube corners, its centre, and the saved building eye. Rotation at fixed eye
does not change facing or mask lookup: the report declares all 72 yaw poses in
5-degree steps with identical selection. Four actual raster views per selected
mesh group additionally compare original/cone/mask depth at 1280×720. This is
not an orbit or a native camera calibration. Crossing the near plane is clipped
before rasterization rather than dropped. Positive reference coverage is required.

Output remains exclusively in the explicit system-temp directory. Reports include
input/source hashes, source/material provenance limits, original batch groups,
metadata payload, query steps/reads, candidate index writes, index references,
unique referenced vertex IDs within each cluster, Python time and process peak
RSS. Summed unique IDs are not hardware vertex-shader invocations or a vertex
cache simulation. RSS includes the Python runtime, oracle and raster scratch;
it is not native additional memory. No native allocation/GPU-time claim follows.
Actual lighting, alpha, shadows, all-world coverage, runtime LOD and device gates
remain separate and open. Facing must be queried from each consuming view,
including shadow/reflection views; a camera-facing exclusion is not reusable
as a global removal of a shadow caster or logical/collision geometry.

## Measured comparison

Frozen input manifest `frozen-inputs/manifest.json` from the existing
`outshine-research-projection-b208e03e8` input set: three complete parent cohorts,
157 products and 714,098 building triangles. The existing resident102 transport
set adds 24 bodies and 19,288 triangles. All 5,915 original/reference ranges
passed 129,967 cluster-camera checks with zero visible/tangent removals; original
spheres all passed enclosure. Nine independent unit tests passed.

At each building cohort's saved eye, before frustum/occlusion selection:

| Parent prefix | Index references | Cone removed | Mask removed | Mask payload bytes |
| --- | ---: | ---: | ---: | ---: |
| bbd9aab74311 | 1,403,151 | 6 | 40,170 (2.863%) | 352,512 |
| 6d41e63e7328 | 635,229 | 0 | 37,905 (5.967%) | 161,760 |
| 035e2150a3c1 | 103,914 | 0 | 87 (0.084%) | 29,184 |

Combined masks remove 78,162/2,142,294 index references (3.649%); summed
cluster-local unique vertex IDs fall from 1,513,928 to 1,459,190 (3.616%).
The exact facing oracle identifies 1,066,299 back-facing index references, so
these coarse static bounds leave most potentially avoidable geometry intact.
This is a pre-frustum geometric opportunity, not actual post-selection VS work.
Cones remove only two triangles from the saved-eye building inputs.

Building payload is 46,016 cone bytes or 543,456 packed mask bytes (552,192
with fixed 96-byte cluster strides). Saved-eye masks read 90,216 payload bytes,
decode 711,229 triangle bits and would copy 8,256,528 retained index bytes,
versus 8,569,176 original bytes. These 312,648 avoided write bytes do not pay
for the separate predicate, scan and metadata work by themselves. Native cache
reuse and device timing have not been measured. No mask adoption is justified.

Across the six near/far axis directions, the transport references remove
9.408% with cones and 15.337% with masks. On cube corners the OR-ed conservative
masks remove zero; cones remove 28.214%. The different camera distributions are
not interchangeable percentages. Transport range payload is 1,304 cone bytes
or 15,024 packed mask bytes (15,648 at fixed stride). There is no transport
saved-native-eye comparison or native transport cluster claim.

The measured process used 197,456 KiB peak RSS, including Python, accumulated
reports, exact oracle and raster scratch. Summed Python geometry comparison time
was 119.59 seconds under `nice -n 10`; this is not native build or frame cost.
Six selected mesh/camera cases have four original/cone/mask depth comparisons
each, all unchanged; twelve forward original/mask images were actually opened.
Two additional close diagnostic poses address the tiny/dark initial third-cohort
and transport images. They do not change the saved-eye quantitative comparison.

Decision: retain this conservative reference and the negative building result;
do not implement the cone or masks natively on the strength of these data.
Any useful next variant must recover substantially more work on the same
original topology without growing batches or losing source/material ownership.
Current runtime eligibility, camera/model precision and target-device costs
remain the minimal missing consumer evidence.

[The normal-group follow-up](group_facing.md) derives the exact whole-cluster
removal limit and compares membership masks for orientation groups inside the
same original clusters. It preserves this whole-cluster result and its scope.

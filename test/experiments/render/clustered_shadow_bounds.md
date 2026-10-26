# Clustered pieces retain conservative shadow bounds

Owner: #819. Reference: e8d01bedd; shared native integration remains draft #531.

## Contract

`WritePiecePositions` copies StoredVertex positions unchanged into the Float32 GPU stream.
Only normals are packed. `geometryDepth.vert` applies the instance transform and camera shift;
there is no additional vertex displacement. Current native StructureBake and ShapeCook use
ClusterCook, whose bounds enclose the referenced positions after rounding the centre and
rounding the radius outward. Cluster bounds remain a producer contract, as in GPU selection;
this change does not validate arbitrary lying spheres by rescanning vertices at runtime.

The new LocalSphereOf overload checks finite normal-or-zero centres/radii and complete,
triangle-aligned index coverage. Ordered overlapping intervals are allowed; an uncovered
prefix, gap, suffix or invalid interval yields an unknown bound. All supplied LOD levels
participate because the shadow path draws the full original piece index range.

For balls `(c_i, r_i)`, use coordinate bounds `l=min(c_i-r_i)`, `u=max(c_i+r_i)`,
centre `c=(l+u)/2`, and radius `R=max(norm(c_i-c)+r_i)`. The triangle inequality
establishes containment. Existing PackSphere covers centre conversion and rounds the radius
outward. Two linear cluster passes, constant scratch, no vertex scan per frame or new cache
format/binding. Existing local/world sphere members hold the result. Existing guarded affine
transforms handle shear and instance placement. Single-placement batches reuse their bound;
multiple clustered placements have separate batch bounds.

The shadow pass uses its actual packed Float32 projection/shift with BatchFrustum's four
side planes and existing arithmetic uncertainty guard. It does not reject by Z because depth
clamping remains enabled. Unknown bounds still draw. Camera selection keeps its own projection.

## Actual inputs and controls

A private accepted-PieceMesh export from complete offline Flensburg contained 3,300 pieces,
1,562,879 vertices, 2,158,029 indices and 7,394 clusters. All cluster intervals covered the
complete shadow streams; all referenced positions were inside the declared spheres without
a tolerance. The combined spheres also contained all referenced positions. Original export:
46,921,700 bytes, SHA256 789773c9a20398d2aa8837a0449539c1138162743203f56a1b83326fd7e248bc.
Its instrumented PNG equals the uninstrumented reference. This is a real-input control,
not a proof for all possible providers. The producer algorithm and native boundary tests
supply the general contract.

Native coverage includes uploaded GPU indices after residency growth, invalid/incomplete
bounds, affine/sheared placements, unknown bounds, depth clamp and a moving distant instance.
The original archive fails exactly the two new whole-piece assertions (562/564 checks) and
one new skipped-work assertion (25/26); the other assertions pass unchanged.

## Fresh device comparison

A18 Pro / Metal, 1280x720/60, exact 60 frames and 360 degrees, 60 direct GPU samples;
zero source bytes/provider starts in every fresh offline process. Movement is 6 m in one second.

| Moving arm | GPU mean ms | GPU p99 ms | CPU p99 ms | Last-frame shadow batches |
|---|---:|---:|---:|---:|
| A0 reference | 12.073 | 28.730 | 14.717 | 3302 |
| B1 change | 12.395 | 31.804 | 12.412 | 56 |
| B2 change | 10.892 | 25.116 | 8.554 | 56 |
| A3 reference | 11.949 | 31.808 | 21.754 | 3302 |

Each mode's four PNGs are exact. Static worlds are complete; moving worlds remain 60/60
unrefined. Actual work and CPU peaks improve; these noisy GPU samples establish no general
GPU speedup or 16.67 ms admission. Static shadow reuse remains intact. Private original
inputs, commands, reports and failed initial style analyses: /tmp/outshine-shadow-bounds-819/.

# Independent clustered shadow-bound control (#819 / #820)

This reviews native candidate **0eb2e739d009cf12df5855f9f688dd0169a6de1c**.
It adds Python controls only. No native code, cache, shader, binding or runtime
quality is changed. The general goal and device/world gates remain open.

## Actual input and representation

The accepted-PieceMesh capture at e8d01bedd contains 3,300 pieces, 1,562,879
StoredVertex records, 2,158,029 original shadow indices, 7,394 clusters and 3,300
nonidentity instance matrices. The raw input is 46,921,700 bytes, SHA256
`789773c9a20398d2aa8837a0449539c1138162743203f56a1b83326fd7e248bc`.
The [transfer receipt](https://github.com/Weichwolf/outshine/issues/819#issuecomment-6263164522)
defines its format and capture provenance. Original raw/format/build/source
receipts remain together in the receiving agent's system-temp input directory.
Accepted-piece order is not an OSM/source ownership map.

`shadow_capture.py` checks the exact input hash, every header/stride/array extent,
indices, finite positions and EOF. It reads row-major captured Float32 matrices;
it does not reinterpret them as native column-major Mat4 storage. A difference
array independently checks complete index coverage, allowing overlaps and all
recorded LOD levels. This actual capture has level 0 only; mixed-level/overlap and
uncovered-prefix/gap/suffix controls are separate tests. Nothing selects only
camera-visible clusters or reduces the original shadow stream.

Pinned source review confirms that WritePiecePositions copies the three stored
Float32 positions unchanged; StoredVertex packs normals, not positions.
subjectDepth.vert applies the rigid instance matrix and shift, with no position
deformation. StructureBake/ShapeCook use ClusterCook; BoundCluster rounds its
centre first and includes that rounding in the radius before rounding outward.
The older ClusterDag.h BoundingSphere is not interchangeable: the test with
Float32 positions 16,777,216 and 16,777,218 shows its uncorrected centre/radius
can omit an endpoint. A sphere's datatype alone does not prove its producer.

## Independent checks and result

The source model follows existing LocalSphereOf/PackSphere/WorldSphereOf and
BatchFrustum guards. For cluster balls, form the coordinate envelope, choose its
centre and take the maximum centre distance plus member radius. Triangle
inequality gives containment before packing. PackSphere covers the centre's
Float32 conversion and rounds the radius outward. Invalid, incomplete or
unrepresentable bounds remain unknown and draw.

The oracle uses outward Float64 intervals and exact dyadic arithmetic for
ambiguous point boundaries. **Every member-ball inclusion** is checked with
exact rational squared distances and radius differences, without sqrt/tolerance.
All actual cluster references, complete piece references, member balls and
recorded transformed positions pass: zero violations, gaps or unknown bounds.
The referenced vertices' enclosing convex balls also enclose their triangles.

For every actual instance, five controlled light-side configurations check the
shader's sequential Float32 matrix/shift execution. All 16,500 queries pass;
13,200 reject and 3,300 contain visible vertices. Every rejection puts **all**
referenced vertices strictly beyond **one common** clipping side. These are
controlled lights, not captured original shadow uniforms or GPU execution.
Z is never an exclusion criterion. Real depth-clamp/device behavior remains
covered by the separate native review, not by a Python frame claim.

Ten tests additionally cover changed/multiple instances, shear, reflection,
singular affine transforms, centre rounding, exact tangency, NaN/infinity,
subnormal/overflow fallbacks, bad transfers, and 512 seeded Float32 clip trials.
A deliberately lying producer sphere is accepted structurally but fails the
independent geometric oracle: runtime metadata checks do not validate arbitrary
providers. Sequential Float32 samples are not a universal fast-math proof.
SetPieceInstances/Retable source review confirms that changed rows rebuild
world/per-placement bounds; this is not a new execution of native state tests.

An additional 314 legacy native wall/roof products (three saved cohorts),
1,411,109 vertices / 2,142,294 indices / 5,752 cluster balls also pass. Their
serialized Self/Parent layout differs from the accepted-PieceMesh capture;
the existing SHA-checking cache reader handles that input separately.

## Work, reproduction and remaining scope

The native proposal has two cluster passes, constant scratch and existing
local/world bound storage. Logical input operands in the actual capture are
40 × 7,394 = 295,760 bytes versus 12 × 1,562,879 = 18,754,548 position bytes for
the existing full-vertex AABB alternative. These are field/work counts, not
physical cache traffic or complete allocations/copy accounting. The independent
Python audit additionally reads indices/positions and allocates oracle arrays;
its process cost is not a proposed per-frame cost. No GPU acceleration, frame
budget or global optimality follows from these counts.

```sh
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p 'test_shadow_*.py'
OPENBLAS_NUM_THREADS=1 .venv/bin/python -W error test/experiments/render/check_shadow_bounds.py \
  --capture "$ORIGINAL/pieces.bin" --output "$FRESH_SYSTEM_TEMP"
# Separate legacy input: --buildings "$FROZEN_BUILDING_CACHE" instead of --capture.
```

Reports bind inputs/code, complete memberships, instance checks and cost scope.
The local receipt binds terminal status and the pinned native source snapshot.
Decision: no geometric counterexample found for #820's bounded producer contract;
the independent input prerequisite is satisfied on this capture. Native Vulkan/
Metal execution, images, original light uniforms and whole-world/device budgets
retain their own evidence and owners; they are not re-certified here.

SIGGRAPH-first context is the local [GPU-driven rendering course, PDF page 18](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
visually inspected: instance/cluster selection precedes index compaction/draw.
The ball-inclusion argument and numerical controls above are independent; the
course diagram is not a proof of Outshine's rounding or producer contracts.

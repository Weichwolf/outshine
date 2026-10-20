# Source/category-protected QEM: stopped at the first real cost comparison

Owner #942, parent #899/#844. This is an index-only 3D alternative to the rejected
sampled fields, tested on the complete supplied native cell:576 logical sources,
28,672 vertices,14,169 triangles,1,152 source/part/stream lineages. It changes no
native product, shader, material, contact or physical world. The nominal candidates
are exactly unchanged, so no complete128-pose or native port follows this preflight.

## Method and bounded conclusion

Use the established meshoptimizer1.3 attribute-aware quadric simplifier with an
aspirational25% triangle target and0.001 absolute combined metric tolerance. This
is a **QEM/attribute score**, not a Hausdorff or screen-coverage certificate.
Position, raw UV, packed normal, metres and RGBA corners are welded only when all
their bits and categorical lineage/material code match. In this input all28,672
vertices remain distinct. New indices reference existing attribute vertices;
there is no categorical interpolation or new vertex position fitting.

The strict variant retains every attribute seam. The permissive variant allows
continuous normal/UV/metre/colour seams to change, while protecting positions
shared by different categorical lineages/codes. Zero signs are canonicalized only
for detecting those categorical position groups; stored position bits are retained.
24,172 vertices receive protection. Each returned triangle is independently checked
for a single lineage and material code. Original logical source/part/stream IDs
remain; a new primitive does not invent an original native face tag.

| meshoptimizer1.3 mode | Triangles | Vertices | Dense diagnostic arrays |
|---|---:|---:|---:|
| Original, same uint32 index layout | 14,169 | 28,672 | 2,574,468B |
| Strict,0.001 metric | 14,169 | 28,672 | 2,574,468B |
| Permissive with category protection,0.001 metric | 14,169 | 28,672 | 2,574,468B |
| Permissive, effectively free error, cost control only | 10,765 | 22,365 | 1,997,820B |

The final row has target error1e20 and reported combined score270.303. It is never
rendered/admitted as a quality candidate. Even this optimistic greedy run removes
only24.02% of triangles rather than reaching its requested quarter. This is a
measurement of this implementation/constraints/input, not an optimal lower bound.
Source/category protection is an explicit restriction of this experiment; logical
identity retention in #899 does not require one immutable render component per source.
Joint far representations can retain their logical source table while aggregating
subpixel appearance. General QEM, different grouping policies and larger geographic
inputs are not settled by this case. No tolerance/weight/packing sweep rescues this
nominal candidate, which provides no reduction at its declared metric target.

## Independent checks and actual images

Both nominal outputs reproduce **every ordered triangle corner's Float32 position,
UV/metre/RGBA bits and Uint32 normal word**, plus its original categorical lineage.
This is stronger evidence of no change than selected image similarity alone.
Five controls exercise the real strict/permissive library ABI on an affine planar
grid (4→2 triangles), retention of a non-affine UV centre, touching source/material
seams, signed-zero seam detection and invalid input refusal before the native call.
They verify that a broken/no-op ABI is not responsible for the native-cell result.

The same original1280×720 profile and four regular samples per pixel render
static020/moving020/moving021. No lost/added source hits, changed categorical codes,
depth, UV/metres or colour. The existing angular diagnostic reports1.21e-6degrees
from arccos rounding despite equal normal component words. Material-ID and normal
panels were actually opened. Their enlarged48×10/47×10/49×10pixel ROIs illustrate
this particular far cell; they are diagnostic fields/two Lambert lights, not native
PBR/world images. The complete motion/interior/world/device requirements stay open.

## Work, memory and limits

Per construction:42,507 corners,2,380,392B complete-corner key,1,261,568B continuous
attribute input,170,028B destination indices, plus grouping/compaction arrays and
library work. Single Linux/x86-64 trials under concurrent load cost90.8ms strict
and151.4ms permissive, including Python preparation/compaction and dependency reads.
Independent word-identical tracing replays peak at12,683,029/12,683,053B of traced
Python memory. Native simplifier allocations, total allocation/copy counts, process
and device peaks remain unknown; no native runtime or GPU timing is inferred.

The dense diagnostic arrays include unpacked normal vectors and face lineage.
They differ from the original actual native1,028,316B payload. The reference's
int64 indices are normalized to uint32 in the comparison to avoid claiming an
index-width saving as simplification. No coarse persistent format is introduced
after this zero-gain preflight. Fine/cache acquisition, caster replacement and
global amortization are still #899 work; a hypothetical additional coarse asset
would coexist with fine casters unless a separate consumer changes that demand.

## Standard dependency and reproduce

Only Python experiment code is supplied. meshoptimizer is an external standard
dependency, not bundled or implemented in this repository. Use the official
[v1.3 release](https://github.com/zeux/meshoptimizer/releases/tag/v1.3), commit
`9e1f07b159d3cb777f1c67ed31fc11fd117986f4`. Build its shared library with CMake's
`MESHOPT_BUILD_SHARED_LIBS=ON`, not the unrelated `BUILD_SHARED_LIBS` switch.
Install to the Python environment prefix; its public header/version1030 and
complete ctypes signature are checked. The library binary hash is recorded.
An initial PyPI0.2.30a0 probe actually used old library0.22; its missing attribute
ABI declarations and unavailable permissive mode are not assumed equivalent to1.3.
That obsolete probe is not a compatibility branch or required runtime dependency.

```sh
# Extract the pinned upstream release in a private system-temp directory.
cmake -S "$meshopt_source" -B "$meshopt_build" \
  -DMESHOPT_BUILD_SHARED_LIBS=ON -DMESHOPT_BUILD_DEMO=OFF \
  -DMESHOPT_BUILD_GLTFPACK=OFF -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$python_environment_prefix"
nice -n 10 cmake --build "$meshopt_build" -j 2
cmake --install "$meshopt_build"
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/render \
  .venv/bin/python -B -W error -m unittest test_constrained_mesh_simplification
OPENBLAS_NUM_THREADS=1 PYTHONPATH=lib/src:test/experiments/render nice -n 10 \
  .venv/bin/python -B -W error test/experiments/render/compare_constrained_mesh.py \
  --output /tmp/outshine-research-constrained-mesh-<commit>/release
```

SIGGRAPH-first: [Garland/Heckbert1997 QEM](https://www.cs.cmu.edu/~garland/Papers/quadrics.pdf),
local PDF p2/Fig1–2 optically inspected; [Cohen etal1996 Simplification Envelopes](https://www.cs.umd.edu/gvil/papers/simp_env.pdf),
local p2/assumptions inspected. Their geometric/topological conditions are not
automatic guarantees of this score. The version-pinned author's [simplification
documentation](https://github.com/zeux/meshoptimizer/tree/v1.3#simplification) and
public header define permissive/protection behaviour. All private diagnostics,
receipts and original input digests stay in system temp; no pictures published.

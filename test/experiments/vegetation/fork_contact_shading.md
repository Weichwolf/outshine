# Joined fork contact shading — #687

The rounded #694 junction still shows a dark angular band where collar and
octagonal wood meet. This experiment reuses the existing local normal-weighting
method only at actual shared collar/wood vertices. Positions, faces, provenance,
UVs, material selectors, complete foliage and source skeleton remain exact.
No geometry, shader, species default or library/native/cache API is changed.

## References and choice

SIGGRAPH 1985 [Modeling the Mighty Maple](../../../doc/references/vegetation/siggraph/1985-modeling-the-mighty-maple.pdf),
page 6/Figure 6, was inspected optically again: a ramiform's geometric tangent
binding is a separate problem from smoothing its displayed normal field. This
does not implement or establish that geometric continuity.

The existing Ghidra export of SpeedTree `CBranchNode::AverageNormals` at
`0x1ca3720` was reread locally. An isolated original-binary micro-oracle then
processed 35 finite vector cases, including zeros/opposites. Both endpoint
records receive identical results for three independent vector pairs; maximum
component error against independent Float64 normalized-sum arithmetic is
8.762e-8. This confirms pair averaging only, not the semantics of every stored
frame field, whole branch lighting or SpeedTree's multi-face weighting.
The existing project/export and micro-oracle remain under
`~/Downloads/outshine-speedtree-analysis/`; no new headless decompilation run is
claimed and no vendor code, binary or asset is bundled. Binary SHA256 is
`9bd45d76a249bac17ce51680d7a7507f60f75d26f44f96a1ed998e58c1eb970c`;
oracle-script SHA256 is
`367fe9072bbd75b758daebf067eb911734a76c01a28c6d129150d0a30e7c8482`.

Our independent multi-face choice is corner-angle weighting, already present in
`join_normals.py`. A centroid split of one source triangle preserves its angular
contribution at the old corner. Uniform and area weights change that corner's
contribution relative to other incident sources; the new scalar control measures
both changes above 0.01 in normal-vector distance. Existing scalar angle, edge
split and hard-boundary controls remain. No global optimality claim follows.

Actual source-kind rows identify vertices incident to both collar and original
wood. The existing normalized weighted mean is accepted only when every incident
corner stays within 45 degrees of its original normal and faces outward against
its own triangle. Otherwise the entire vertex keeps its original corner normals.
There is no flipping, tolerance increase or smoothing of unrelated vertices.
A no-contact helper call returns the same mesh without weighting/normal copies.

## Complete saved product and independent verification

The same group has 2964 vertices/5924 faces and 17772 corner contributions.
All 1048 contact vertices pass; 5108 corner normals change. An independently
enumerated scalar angular sum matches within absolute 2e-14. Shared-vertex
maximum component spread changes 0.6769209890 → exactly 0. All non-contact
normals are exact. Shared endpoint normals therefore agree along a common edge
under the same linear normal interpolation; this does not make the geometric
surface C1 or prove every downstream shader uses that interpolation.

Geometry identity remains
`a96a082555d70a2656d896bd227afe83aed1b3c77ea170a74caeb355b36d4056`;
appearance-product identity becomes
`d40b2897dc857991e5488c062c83a60b16a8706109958b5b4a0064b5082206ea`.
The recipe binds the source product/wood bytes and declared weighting policy.
These hashes are integrity/reproduction evidence, not an external cache trust root.

All 13 non-wood/non-entry files, all other whole-wood arrays and all non-normal
patch arrays are byte/value exact. This includes 90000 leaf rows, eight Blade
meshes, materials/maps, skeleton, source/patch IDs and topology. The existing
independent #694 topology/volume/field-occupancy replay also passes unchanged:
one local genus-0 body, 384 source-OR-collar samples, unchanged ray probes and
330880 untouched whole-tree faces. Separate unrequested bodies still remain.

Named generation/weighting functions are guarded during real cache decode;
the scalar normal verifier runs separately afterwards. It loads both old/new
products, 127629896 B of arrays; this is neither a single runtime-hit measurement
nor an alias-proof sandbox. Source/installed normal checks agree exactly;
paired guard/decode 0.514/0.724 s, separate scalar verification 1.098/1.696 s,
process peaks 299606016/299491328 B. The separate existing geometry replay also
passes in both import modes; these finite checks do not establish global
self-intersection freedom, Float32, LOD, wind or device acceptance.

77 source and 77 installed-library consumer tests pass, including independent
centroid/edge splits, contact selection, common endpoint normals, hard-angle
refusal, orientation refusal without flipping, no-contact work avoidance and
preserved geometry/attributes. The #690 wheel is reused: all 207 Python library
files match current source; installed PYTHONPATH excludes lib/src. No new wheel
or shared library change is claimed.

## Image and costs

The matched 2560×1440 fork view is opened: the dark angular band is largely gone
and the outgoing transition looks calmer. The octagonal parent and generic bark
remain visible. Current whole/leaf-near images are also opened: overall crown,
leaf fans and silhouette are unchanged; under dense foliage the improvement is
small. Cameras, fine-leaf IDs, materials and light are identical to #694. Whole
geometry is still 336804 wood faces /3396804 coarse instanced references;
finished wood arrays remain 51799812 B. No extra triangles or payload are added.

Frozen producer `f32b8df76`: source wood decode 0.156 s, normal/mapping work
0.033 s, export 1.023 s, process peak 187752448 B. A complete reference export
copies 24249888 B of old whole-wood normals and scatters 426528 B of patch
corners; only 122592 B of corner-normal components differ. Those are explicit
NumPy payload counts, not a full allocator/traffic measurement. Other arrays
are reused during production; compression and verifier temporaries cost more.
No growth, union, field meshing, leaf placement or texture generation is repeated.

On the same local input, three exploratory nice-10 shared-host trials give
19.408/16.657/16.865 ms median for angle/area/uniform including surface checks.
The angular choice follows the split criterion, not these noisy timings. Cache
hits load finished normals and do not repeat averaging. All observations are
Linux/WSL x86_64, Python 3.13.5/NumPy 2.5.4, one numerical thread; EEVEE receipts
identify Mesa llvmpipe, not RTX. Native/device budgets remain separate.

## Reproduction and remaining work

Exact commands/commits, private current products/PNGs, oracle inputs/results and
terminal receipts are retained under
`/tmp/outshine-vegetation-fork-normals-proof-8e514ef86`; the PR links their scope.
Its current `products` folder is render-input equivalent to the first inspected
fork export. Source products remain as described in [oak_local_forks.md](oak_local_forks.md).
Keep their original symlink targets. No gallery/server/image is published.

```sh
export TMPDIR=/tmp PYTHONDONTWRITEBYTECODE=1 OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1
export PYTHONPATH=lib/src:lib/tests:test/experiments/vegetation
.venv/bin/python -m unittest fork_shading_checks oak_fork_checks source_limb_checks limb_checks crown_collar_checks crown_union_checks crown_shoot_checks test_spatial_axes test_plant_plans test_blades oak_scaffold_checks posture_checks
.venv/bin/python test/experiments/vegetation/fork_shading.py \
  --inputs /tmp/oak-local-forks --output /tmp/oak-contact-shading
.venv/bin/python test/experiments/vegetation/fork_shading_replay.py \
  --inputs /tmp/oak-local-forks --products /tmp/oak-contact-shading --output /tmp/oak-normal-replay.json
```

Unrequested side/short-shoot connections, arbitrary parent-skin tangent binding
and realistic crown/bark/leaf morphology remain at #687/#682. This is a local
shading improvement, not completed botanical oak or worldwide vegetation quality.

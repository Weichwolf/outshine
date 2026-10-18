# Actual world tree-detail comparison — #1 / #23 / #188

The native world still draws whole-tree cards close to the camera. This experiment
compares possible replacements in the actual unchanged offline world before a
new near-product/cache path is implemented. None of these candidates is admitted.

## Reproduce

Use a built native checkout with its own current compilation database, archives,
shaders and complete warm world/atlas caches. No build, source checkout or product
shot is changed. Each process renders one Place. Outputs, copied module variants,
objects, commands, raw world input, receipts, logs and PNGs stay in system temp.

```sh
python3 test/experiments/render/native_tree_detail_world.py \
  --native-root /tmp/<built-native-checkout> \
  --output /tmp/outshine-tree-detail-<fresh-name> \
  --place Koerbersee --bearing-deg 240 --pitch-deg -0.5
```

The caller owns the Place name and camera parameters. The cone is an explicit
selection control (`abs(side/depth)<0.65`), not a production frustum or LOD rule.
`--selection near` instead takes the closest roots in all directions. Count and
old mesh rank are independent experiment parameters; defaults are four and2.

The driver reads complete real unit flags from the native compilation database
and the declared engine/backend link libraries. Main retains its actual process-
boundary exception flags; the changed runtime module retains `-fno-exceptions`.
Only the single module variant is substituted into the existing native program;
class definitions/layouts, renderer/backend contracts, shaders and world producers
are unchanged. The generated owner calls an experiment helper; no access-control
macro, product debug hook or runtime management service is introduced.

The helper is deliberately limited to one VegetationStreaming owner per process.
It releases its added GPU pieces before owner destruction. Actual original model
matrices are checked exactly before replacing selected rows. The original world
and the other4080 trees remain. A rebuilt flat-card control must reproduce the
unmodified image. No test oracle, cache identity or source property is rewritten.

## Actual controlled inputs and output

Native `f1b65621b`, source baseline `8a57b0171`, Apple A18 Pro/Metal,1280×720/30.
Same original models, species, camera, fixed time, lighting, materials and360°
turn. Each run records exactly30 frames/timestamps and saves only its final PNG.
Four front candidates are oak at32.474m, pine at81.280m and two ash instances at
84.005/85.324m. Their exact group/row identities and matrices are in the receipt.

The earlier nearest-root control chose four trees behind the final camera. Its
final PNG stayed unchanged despite expensive work. This is a useful negative
selection control, not evidence that geometry had no effect on other turn frames.
Demand must account for projected contribution before instance/geometry work.

| Same four front trees | Requested matrix bytes | GPU mean/max ms | Opened final PNG |
|---|---:|---:|---|
| Mesh, old rank2 |110094848|227.067/429.031|`f6f4adf0`: continuous branches, sparse old foliage; far over budget|
| Rebuilt flat control |512|18.517/37.633|`076343f4`: exactly identical to baseline|
| Existing adaptive depth planes |512|19.829/36.413|`306728f3`: severe long spikes|
| Constant-depth patches |512|20.143/44.592|`d0c98a9a`: spikes removed; pixel blocks/gaps remain|
| Normal-coherent planes, point fallback |512|20.210/39.699|`fd722523`: similar pixel/gap limits|

These are sequential observations with no other own GPU work, not stable timing
rankings or device acceptance. The30-fps budget remains33.333ms including p99;
all measured maxima exceed it. No current device gate is closed by this study.
The source-dump run performs diagnostic IO and is excluded from timing comparison.

The mesh path requests860116 Mat4 rows:110094848/128, including four woody roots.
It uses12625 unique prototype vertices but109432260 source model triangle
references. This is already rank2, not the finest old rank. The probe refuses more than128MiB of requested matrix rows before expanding
a selected prototype. Old foliage count stays unchanged across ranks; `Leaf.CardBudget` is parsed but never consumed by
TreeFoliage. Physically motivated source leaf count is not a runtime draw budget.
Do not fix this by deleting source leaves or silently enlarging their physical size.

Cards require only four model rows. Their reported references64/209242/325462/
330280 sum **all retained views** weighted by those rows; only selected views are
active. They are different counters from the mesh's model references, not a
claimed exact GPU primitive ratio. Matrix bytes exclude allocator/capacity,
source geometry, CPU copies, textures, GPU allocations, driver reserve and RSS.

The capture ROW triangle field remains22696 in every case because PlaceCamera
reads `building triangles the world meshed`. It is not total draw work. Additional
piece/reference metrics are needed in #15; the experiment reports its own scope.

## Independent counterexample and depth limits

`NativeTreeDepthCounterexample.cpp` constructs two separate parallel depth samples
with identical material labels/normals. The unchanged builder joins them into one
sloped plane with exactly zero centre-depth residual; the normal control keeps two
separate samples. Compile it with the native DepthSurfacePatches unit flags and
link the existing native archive. The finite native counterexample passes.

BuildDepthSurfacePatches checks depth residual at covered texel centres and a
normalized corner interval. Matching that criterion does not establish that
neighbouring samples belong to one connected physical surface. Two unrelated
leaves can define a steep accepted plane; viewing it obliquely exposes long
connecting triangles. Setting slopes to zero removes these particular spikes
while preserving the original depth sample at each resulting constant cell.

The normal control rejects candidate planes that disagree with captured surface
normals by more than0.26rad, then uses separate constant-depth texels. The angle
is the existing group-cloud experiment's comparison parameter, not a new global
LOD allowance or a proof of connectivity. Its geometry is selected before mesh
emission. The same pixel/gap artifact remains; shading normals and one visible
depth layer cannot reconstruct hidden branches/leaves or missing near resolution.

[Layered Depth Images, SIGGRAPH1998](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf)
was reread locally and PDFpage2/Figures1/2 opened optically. It explicitly separates
distant sprites, depth sprites, multiple depth layers and near polygons; depth
discontinuities/disocclusion need their own handling. Our single-layer candidates
are not full LDI. No smaller surface/screen/radiance error bound follows.

## Integration decision

Reject the old full-foliage expansion as a near fallback. Do not solve the
near-world gap by retaining every old leaf matrix or increasing a cache budget.
Whole-tree cards stay a distant representation. Do not promote these whole-tree
single-layer depth candidates to near geometry.

Use the cooked spatial skeleton and demanded CrownShoots groups instead:
continuous woody paths selected before tessellation; local foliage groups chosen
before poses/meshes/uploads; fine source blades only where they contribute.
Original owners, physical area, positive normal distributions and R/T remain
separate from representation/lighting. #697 supplies the direct group comparison;
master ports C1 wood and the native consumer. A repeated prototype/instance table
must not replace the required hierarchy. Current botanical and world acceptance
remain open at #1/#682.

The original probe and independently reproduced controls have the same PNG hashes.
Original inputs/commands and opened images remain under
`/tmp/outshine-near-trees-3ee8d74d8/`; the self-contained rerun is `reproduction/`.
No diagnostic image is published or installed as an accepted Place.
The driver also checks30 frames, complete preload/no bare tiles, the saved hash
name and exact flat-control equality. A deliberately changed original matrix is
refused before any near publication. The ordinary native client was rerun afterward
and restored the accepted076343f4 Place.

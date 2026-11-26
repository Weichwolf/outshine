# Directional depth cards for middle-distance groups

The native baker captures orthographic albedo, coverage, normal, material and depth.
The Python reference reconstructs 11 held-out angles from eight horizontal views,
four oblique views and one overhead view. Two selected cards resolve by depth.
Planar blocks fit covered samples only and subdivide when the sampled residual
exceeds half a capture texel or the plane slope exceeds two. Coverage remains in
the texture. The triangle count assumes two triangles per planar block.

```sh
cmake --build build --target outshine_experiment_group_impostors
probe=$(mktemp -d "${TMPDIR:-/tmp}/outshine-group-impostors.XXXXXX")
mkdir "$probe/block" "$probe/tree" "$probe/stand"
reference=test/experiments/render/group_impostor_views.py
python3 -B "$reference" directions > "$probe/views"
bake=build/outshine_experiment_group_impostors
spruce=data/world/species/spruce.json
"$bake" buildings "$prepared_surface_tile" "$probe/views" 128 "$probe/block" 64 13
"$bake" tree "$spruce" "$probe/views" 256 "$probe/tree"
"$bake" stand "$spruce" "$probe/views" 128 "$probe/stand"
python3 -B "$reference" evaluate "$probe/block" "$probe/block-results"
python3 -B "$reference" evaluate "$probe/stand" "$probe/stand-results"
python3 -B "$reference" hierarchical "$probe/tree" "$probe/stand" "$probe/composition"
```

At 64 texels per view, coverage compares fractional intersection and union. The
building overlays use Tokyo source tiles `19-465596-206493` and `17-116398-51622`.
The canopy control uses 25 native spruces on staggered roots about 5 m apart.

| Group | Flat ring median | Two patched views median / worst | Peak triangles / source |
|---|---:|---:|---:|
| Tokyo block | 45% | 93% / 80% | 1,050 / 10,008 |
| Tokyo district | 44% | 81% / 72% | 2,074 / 5,776 |
| Spruce stand | 59% | 65% / 61% | 3,870 / 235,452,600 |

Start the building runtime trial at block size. Keep the source representation
where the oblique angles exceed the visible error budget. The district at 128
texels can already cost more triangles than its source. The two-view spruce result
loses too much internal canopy coverage for the same selection policy.

The optional last argument also bakes each native packed card at the requested
side. The reference reads back the packed cards. At 64 texels their coverage and
triangle results match the building rows above to the reported precision. Across
the block's probes, the two selected files occupy 93,976 bytes at the median and
97,956 bytes at the peak. The full set of 13 views occupies 613,464 bytes. This
supports making each direction an independently resident product.

## Metal control

```sh
cmake --build build --target outshine_experiment_metal_groups
build/outshine_experiment_metal_groups "$probe/block" "$prepared_surface_tile" \
  "$probe/metal" 950 28
```

This 1280×720 control moves through 86° at the given distance and elevation over
120 frames. The ground grid is the same in both runs. The group draw uses two
views of instanced planar quads with the sampled coverage mask and opaque depth.
Three paired A18 Pro runs of the Tokyo block measured a median GPU p99 of
0.592 ms for geometry and 0.118 ms for the group. Peak triangles fell from
12,056 to 2,878 including the shared terrain grid. All 13 views were resident
in this control, yielding 668,012 uploaded bytes including ground versus
729,704 for geometry. Device allocation increased from 5,701,632 to 5,865,472
bytes, so the upload reduction alone does not establish a residency saving.
The larger district has visibly broken roof details and uploads more bytes as
cards. The indexed replay adds projected-size admission and selective residency.

## Indexed world replay

Import cards alongside their source surface, using the same render-set revision.
The capture records the surface tile address and immutable revision. Import
extracts the original ground grid and publishes it with the descriptor and
independently readable views in one transaction. The descriptor is bound to the
exact indexed source asset.

```sh
outshine cache import-group "$database" "$revision" "$probe/block"
outshine benchmark --scenario "$camera" --frames 120 --linear-velocity '4,0,0' \
  --product-store "$database" --product-revision "$revision" --image "$png" --output "$report"
```

The loader checks the projected texel size over the sweep and compares the
selected card bytes with the overlay. Pair rankings are affine along a straight
eye sweep. The reference checks the endpoints and the intervals between score
crossings. The cut loads ground and selected views before committing their shared
coverage. A missing ground or view falls back to the whole source tile. Terrain
and the resident group share depth with the clipped far panorama.

The A18 Pro replay of two Tokyo groups used 128-pixel cards and 4 of 26 available
views at 720p30. The same 27 surface tiles and 1,510–1,760 m panorama overlap
were used before and after. Peak triangles fell from 107,273 to 93,471, shared
uploads from 23.84 to 23.28 MB, and compressed SSD reads from 4.80 to 4.55 MB.
Split ground and cards produced the same frames as unpacking the original tiles
and replacing their overlays before upload.

| Motion | Median of three paired GPU p99 runs, geometry / group |
|---|---:|
| Still | 3.17 / 1.88 ms |
| 360° turn | 0.95 / 1.09 ms |
| 16 m lateral flight | 1.96 / 2.33 ms |

The prepared surface fixture covers a limited footprint and leaves surrounding
buildings missing in the clipped near region in both variants. This run checks
independent view residency, shared scene depth and retained silhouettes. Full
city coverage remains open.

## Near-to-mid handoff

Admission starts at one projected texel using distance to a conservative group
bound. A cubic ramp completes at 0.985 projected pixels. Crossing sweeps retain
the full tile and selected cards. Complementary screen-space masks share the
same depth test, so coincident coverage survives while the actual occluder in
each pixel supplies depth. Full group weight submits only the terrain indices
from the tile. Only the cards and separate ground are loaded for an entirely
mid-distance sweep.

At 720p30, a 36 m Tokyo flight with a 43.29° vertical field of view crossed a
20.1 m band around one of the two groups. The frame report showed 71 blended
frames in either direction. Near and group submissions combined fell from
101,664 to 93,470 triangles while retreating, with 103,478 at peak overlap,
plus one panorama triangle in each case. The three-run median GPU p99 for
geometry and groups was 3.91 / 3.90 ms while retreating and 4.07 / 1.96 ms
while approaching. The two directions give different timing gains. Inspected
snapshots preserve the block outline with small roof and facade differences.

With `--stream-products`, the renderer keeps the initial resident cut until the
worker has loaded and uploaded every tile and group in the replacement. Both
resource lists are switched together. In three paced 720p30 flights the initial
12 tiles gave way to 27 tiles and two groups at frame 1, with 11 tile buffers
reused. The median GPU p99 across the paired runs was 2.85 ms for geometry and
2.81 ms for groups. The final grouped frame matched the fully preloaded path.
A one-frame run kept the initial cut even though the replacement loaded. Removing
a required view from the indexed package kept that group's geometry and allowed
the independent group to activate.

Compositing the single spruce captures at the 25 roots reaches 82% median and
64% worst coverage overlap against the direct stand captures. One native bake of
24 views took 0.98 s for the tree and 21.02 s for the stand. The Python composition
of all 24 stand views took 0.22 s. This measures a bake candidate, with native
capture including GPU submission and readback. Runtime GPU cost and continuity
through both world handoffs require separate measurements.

## Canopy directions

```sh
canopy=test/experiments/render/canopy_group_angles.py
python3 -B "$canopy" directions > "$probe/canopy-views"
mkdir "$probe/canopy-tree" "$probe/canopy-stand"
"$bake" tree "$spruce" "$probe/canopy-views" 256 "$probe/canopy-tree"
"$bake" stand "$spruce" "$probe/canopy-views" 128 "$probe/canopy-stand"
python3 -B "$canopy" evaluate "$probe/canopy-stand" "$probe/canopy-results"
python3 -B "$canopy" compose "$probe/canopy-tree" "$probe/canopy-stand" "$probe/canopy-compose"
```

| Card side | 13 stored views | 29 stored views | 53 stored views |
|---|---:|---:|---:|
| 32 texels | 0.658 | 0.750 | 0.777 |
| 64 texels | 0.645 | 0.717 | 0.751 |

The entries are median fractional coverage IoU at 11 held-out angles using two
depth-reprojected views each. At 32 texels, drawing four views from the 53-view
set reduced IoU to 0.765 and increased median excess coverage from 11.7% to
17.9%. Start with two active views and the 29-view bank at a small projected
footprint. The denser bank can serve foliage whose coverage still falls short.

Across the 64 capture and control directions, reusing the single-tree captures
to compose 25 positions took 0.57 s in Python, including comparison. Median
coverage IoU against the directly captured stand was 0.845, with a worst case
of 0.634. Native capture took 2.41 s for the reusable tree and 55.29 s for the
full stand. This control measures one species and one placement.

```sh
cmake --build build --target outshine_experiment_compose_groups
mkdir "$probe/composed"
build/outshine_experiment_compose_groups "$probe/canopy-tree" \
  "$probe/canopy-stand/group.json" "$probe/composed" 32
python3 -B "$canopy" evaluate "$probe/composed" "$probe/native-canopy-results" \
  --reference "$probe/canopy-stand"
```

The C++ baker copies the nearest transformed depth sample, along with its
normal and material. Each source view is reused for translated or uniformly
scaled instances. Three runs on A18 Pro composed all 64 views in a median
24.55 ms after reading the reusable atlas, before fitting, encoding and writing
the cards. Two 32-texel cards selected from 29 composed directions retained a
median coverage IoU of 0.750 against the 11 directly rendered held-out views.

## Terrain-attached canopy

`canopy_group_layout.py` samples roots on the actual triangles of a prepared
surface grid using the source forest classes and declared tree densities.
It groups two-by-two terrain cells. The replay uses one spruce prototype.

```sh
python3 -B test/experiments/render/canopy_group_layout.py \
  "$surface" "$ground_samples" data/world/vegetation.json "$prototype/group.json" \
  "$probe/layouts"
mkdir "$probe/hillside"
build/outshine_experiment_compose_groups "$prototype" "$layout" "$probe/hillside" 32
outshine cache import-group "$database" "$revision" "$probe/hillside"
```

The descriptor binds to the exact source surface asset. Its request key
identifies the cell, and the package key identifies the immutable payload.
The cut loads the directions needed by the eye sweep. The shader applies
fractional coverage and shares depth with the ground and far panorama. Deleting
one canopy group's card bank kept all 24 ground tiles and the other 18 groups.

The Körbersee replay placed 2,332 roots in 19 groups. It kept 38 of 551 views
resident with up to 6,942 group triangles per frame. The median GPU p99 of
three paired 120-frame A18 Pro runs at 720p30 was 1.65 / 1.75 ms in a 360° turn
and 1.87 / 1.88 ms in a 16 m retreat, with the canopy enabled in the second
run of each pair. Initial shared uploads rose by 764,460 bytes. The refined
cut activated at frame 1 in each replay. The shots show visible stipple in
thin crowns. Near geometry and continuity across ground refinement still
require acceptance.

## Canopy in the far panorama

`compose_canopy_panorama.py` reads the bound groups and their exact card assets
from a prepared store. It rasterizes the two closest directions over the
linear colour and radial depth cube with four coverage samples per texel.
The terrain and cards resolve depth in metres. The resolved colour averages
the four samples and leaf depth represents texels with at least half coverage.
The prototype uses the panorama's irradiance and exposure for matching shade.

```sh
python3 -B test/experiments/render/compose_canopy_panorama.py \
  "$terrain_panorama" "$canopy_panorama" "$database" "$revision"
outshine cache import-panorama "$database" "$revision" "$canopy_panorama"
```

Set `ZSTD_LIBRARY` to the system libzstd path if Python cannot discover it.
The bake records a group only when it contributes to the cube and all its
card bounds clear the overlap plus the eye translation radius. The loader
skips those exact descriptor keys before reading manifests and cards. During
an anchor blend the keys must be present in both panoramas.

At Körbersee, with 512 px faces and a 700–900 m overlap, each anchor recorded
five groups. At 720p30 the cut retained 14 of 19 groups. Peak card triangles
fell from 6,942 to 5,186, requested group bytes from 883,405 to 590,253.
Three paired A18 Pro runs measured GPU p99 at 1.77 / 1.78 ms for a 360° turn
and 2.31 / 2.28 ms for a 32 m flight across two anchors. The paired PNGs
at rest, during the anchor blend and at the motion endpoints were identical.
A missing key in the second anchor retained that group's cards.

## Instanced near geometry

The source spruce costs 161,340 bark triangles plus 140,254 shoots at 66
triangles each, for 9,418,104 triangles. The coarsest native bark rank has
1,964 triangles. Shoot geometry and placement counts stay the same across
the four existing ranks.

`near_canopy_geometry.py` preserves that bark and clusters shoot area by
voxel and dominant normal. Each oriented plane represents the projected
coverage of its group using exponential transmittance. An independent
orthographic comparison aligns both meshes in world space before measuring
64×64 fractional coverage. At 0.65 m cells and 1.4 area gain, the proxy
uses 10,928 triangles. Across 29 captured views, coverage IoU is 0.826
at the median and 0.650 at the minimum. Axis-aligned planes showed clear
grid patterns. The oriented planes still show thin, evenly spaced tiers
at the closer control distance. This comparison covers one species.

```sh
build/outshine_experiment_tree_prototype "$species" "$prototype"
python3 -B test/experiments/render/near_canopy_geometry.py \
  "$prototype/3" "$flat_surface" "$one_tree_layout" "$tree_mesh" \
  --cell-m .65 --coverage-gain 1.4
build/outshine_experiment_group_impostors overlay "$tree_mesh" "$directions" 256 "$mesh_capture"
python3 -B test/experiments/render/near_canopy_coverage.py "$source_capture" "$mesh_capture"
build/outshine_experiment_metal_groups "$group" "$terrain" "$output" \
  1190 12 "$tree_mesh" "$group_layout" 1260
```

The Metal control instantiates one mesh at the same 17 roots used to
bake the Körbersee group. During the 720p30 arc, 31 of 120 frames blended
over 17.9 m. The crossing reduced endpoint submissions from 188,125
to 2,753 triangles, including the shared terrain. The median GPU p99 of
three paced runs was 1.25 ms for geometry, 0.22 ms for the cards and
1.25 ms for the handoff. Measurements use the isolated prepared tile.

## Indexed near geometry

The composer records the exact layout digest. To import a near representation,
place the original layout at `near-layout.json` and the one-tree prototype
at `near.surface` alongside `group.json`. The same `cache import-group`
transaction binds the mesh bank to the group descriptor and validates the
layout digest. Groups sharing a prototype load and upload it once per cut.

A geometry-only sweep reads the mesh bank and skips that group's cards.
A card-only sweep skips the mesh bank. In the overlap both representations
use complementary masks. A missing prototype falls back to the cards.
Missing cards fall back to the mesh. A rejected layout leaves the existing
request bound.

The prepared Körbersee replay used up to three mesh groups and one shared
prototype for 55 trees. Its 16 m handoff flight used 37 mesh instances in
two groups, with one group blending for 62 of 120 frames. The initial coarse
cut exchanged at frame 1 in each run. The flight requested 664,166 native
canopy mesh and placement bytes. The card-only view requested zero.

At A18 Pro 720p30, three paired runs with the same camera measured median
GPU p99 of 1.98 / 4.12 ms for the flight and 1.79 / 2.96 ms for the full
turn, with indexed meshes enabled in the second run. At the flight endpoint
the affected groups covered 757 pixels versus 788 with cards, at 0.642
intersection over union against the same background. Thin crown coverage and
mesh scaling in dense forests remain open.

## Stable sites across terrain refinement

The loader queries canopy footprints once over the swept eye domain. It checks
the bound source version and complete quadtree ground coverage. The site uses
its baked origin with either a covering terrain ancestor or its refined
partition. A staged cut has to retain every active site key before activation.
The prepared set assigns each plant to one source site.

In the Körbersee card-only replay the old cut had 6 sites over 9 coarse terrain
tiles and 14 sites over the 24 refined tiles. The new cut kept the same 14 sites
at both stages. Comparing the eight affected sites against matching terrain
controls gave 1,561 / 1,615 covered pixels and 0.946 IoU across the terrain switch.
Unbinding their source tile excluded all eight while keeping the other six.

Three paired A18 Pro 720p30 runs measured GPU p99 at 4.12 / 4.14 ms on the
16 m flight and 2.99 / 2.99 ms on the 360° turn. The refined still and the motion
endpoint PNGs were pixel-identical across versions. The refined cut activated
at frame 1 in every motion run.

## Viewport rejection and forest stress

Metal derives mesh bounds from the uploaded vertices after terrain stitching,
expands them by the instance transforms, and takes the union of the resident
impostor views. Four homogeneous side planes reject a draw only when its
entire bound misses the viewport. The allowance scales with the absolute
terms of the float transforms. A Python comparison checked 400,000 plane
and box cases against explicitly transformed float32 corners.

Eight sampled headings and the paired motion endpoints were pixel-identical
in Körbersee. The Tokyo handoff was also identical. The normal Körbersee turn
at 720p30 changed from 2.92 / 2.97 to 1.55 / 2.85 ms GPU p50 / p99 across
three paired A18 Pro runs. Flight p99 changed from 4.13 to 4.01 ms.

The stress store used all 2,332 prepared spruce placements and held 1,091
mesh instances during the turn. Its paired PNGs were identical. GPU p50 /
p99 changed from 19.26 / 23.75 to 1.56 / 27.72 ms. During the flight, p99
changed from 27.69 to 29.95 ms. At peak the visible slope still submitted
about 12 million triangles.

Separate, paired mesh-density sweeps on the same 16 m flight kept the same
roots, cards and panorama. The 0.65 m, 0.90 m and 1.20 m prototypes submitted
12.14 M, 6.64 M and 4.41 M peak triangles at 25.57, 28.61 and 22.14 ms GPU p99.
The 1.20 m variant covered 5,039 pixels against 5,017 in the 0.65 m control,
at 0.753 coverage IoU. This measures the distant slope. Runtime selection of
mesh density uses this slope as its first calibrated case.

## Projected canopy mesh detail

The importer accepts an optional `near-coarse.surface` and
`near-coarse.json` with its authored `cell_m`. Both prototypes use the
same roots, source layout and bark. The coarse prototype must have fewer
indices. Rotation keeps one selection. For translation, the loader computes
the closest and farthest distances over the eye segment before opening the
prototype packages.

The size proxy is focal length in pixels times voxel size times the largest
instance scale, divided by the distance to the group centre minus a sphere
enclosing all placed prototypes. At 1.2 px or larger the loader selects fine;
at 1.1 px or smaller it selects coarse. The interval keeps both and splits
the mesh coverage left by the directional cards. This is a size heuristic.
It measures neither signed silhouette error nor depth error.

Three paired A18 Pro 720p30 runs in the prepared dense Körbersee fixture:

| Motion | Fine-only → selected peak triangles | GPU p99 | CPU p99 |
|---|---:|---:|---:|
| 16 m lateral flight | 12.14 M → 4.41 M | 26.66 → 22.44 ms | 0.40 → 0.28 ms |
| 360° turn | 11.95 M → 4.35 M | 30.91 → 21.21 ms | 0.39 → 0.27 ms |

Flight mesh-package requests fell from 747,100 to 274,837 bytes. The visible
stand covered 4,862 / 4,874 pixels in the same crop against matching ground,
at 0.762 coverage IoU. A 54° vertical field of view exercised eight mesh
handoffs. At 36°, the visible endpoint matched the fine control. Removing
the coarse prototype reproduced the fine-control flight endpoint. The Tokyo
handoff was pixel-identical. Thin crowns still stipple and the transition
can submit both meshes. These prepared products do not establish full-world
completeness or the frame budget with the rest of the environment enabled.

[Billboard Clouds](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
[Layered Depth Images](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
[Hierarchical Image Caching](../../../doc/references/geometry/siggraph/1996-hierarchical-image-caching.pdf).

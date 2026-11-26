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
build/outshine_experiment_compose_groups "$prototype" "$layout" "$probe/hillside"
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

## Moving the canopy card boundary inwards

At the Körbersee flight endpoint the 64-texel cards of three oversized sites
projected to 131, 162 and 206 pixels across, using the same nearest-distance
support as the runtime ramp. At 128 texels they still needed 567 geometry
instances. Rebaking those sites at 256 texels put them inside the existing card
limit. The five panorama-baked sites kept their exact descriptors. The other
sites used 128 texels. Roots, directions, lighting and ground did not change.
Reproduce by setting `pixels` in the layout to the desired power of two before
running `outshine_experiment_compose_groups` with the same card side and
importing its output and layout digest.

The prepared store held 14 visible-demand sites in each case. Three 120-frame
runs per variant at A18 Pro Metal 720p30 gave:

| Card bank | Peak combined triangles in flight | Flight / turn GPU p99 | Group bytes requested |
|---|---:|---:|---:|
| 64 texels | 4.41 M | 22.44 / 21.21 ms | 1.77 MB |
| 128 texels | 2.32 M | 13.02 / 12.80 ms | 9.90 MB |
| 128 and 256 texels | 0.12 M | 2.99 / 2.87 ms | 18.04 MB |

Requests include both prepared cuts and count decoded payload size.
The indexed compressed reads for the whole flight grew from 5.47 MB to
6.86 MB, and shared uploads from 22.51 MB to 38.58 MB. Median staged-cut
readiness for the mixed bank was 9.71 ms in flight. Native CPU composition
of all 29 views took 24–25 ms per 256-texel site after prototype load and
before card fitting and encoding.

The mixed bank covered 4,580 pixels in the flight crop against 4,862 for fine
geometry, with 0.625 IoU. The opened frame still shows thin, speckled crowns.
At 39° vertical field of view the same 16 m flight blended one of the
256-texel sites with geometry for 65 of 120 frames. The opened start, midpoint
and endpoint preserve the stand silhouette at a glance, with visible grain
inside the crowns.
Card density shifts most of the cost from frame submission to product loading.

## Camera-independent card density

`canopy_group_layout.py` defaults to a target of 0.8 m per texel. It divides
the authored diameter by the pitch and rounds up to a supported side from 32
through 256. Larger sites use the 256 cap and their actual texel footprint
still governs the runtime transition. The native composer uses that authored
card side. An explicit side is available for controlled comparisons.

With the same group roots and panorama, the fixed pitch chose ten 128-texel
and four 256-texel sites. Three paired runs kept every site in the group tier.
The median GPU p99 was 3.54 ms in flight and 3.09 ms in the full turn versus
3.02 and 2.86 ms with the previous hand-picked sides. The flight screenshot
matched in the measured stand crop. Whole-flight compressed reads were 7.18 MB
and peak submitted geometry was 139,369 triangles, versus 6.86 MB and 120,855
with the hand-picked sides.

A separate Metal control orbited the 151-root site from 488 to 522 m at
elevations of −12°, 37° and 82°. The geometry-to-card transition lasted 21 of
120 frames. Its final frame matched the cards-only control in each view.
Against matching ground, coverage IoU with the fine mesh was 0.631, 0.585
and 0.641 at the three elevations. Composing at twice the side before reducing
to the same card size raised the flight crop from 4,580 to 4,963 pixels against
4,862 for the mesh, but gave IoUs of 0.628, 0.563 and 0.577 in the orbit.
Keep one sample per authored texel. Better angular coverage remains open.

## Dense city coverage

The earlier Tokyo replay baked buildings only within 350 m of Shibuya. The
1510–1760 m cubemap overlap starts at a different camera position. Extending
the local cut to the complete 20 by 20 grid of zoom-17 tiles around that
overlap fills the missing city in all tested directions. There were height
samples from the cached Tokyo ground region for every vertex. Zoom 16 hit
the packed surface's vertex limit even for one 975-plan tile at shell detail.

The expanded build used 51 intersecting prepared OSM plan packages. Of the
224,368 supported records inside the grid, 63,341 had distinct source,
footprint, rings and minimum height. The replay's `--deduplicate` option
handles equivalent winding and ring starts. Four duplicates differed in
height by over 5 cm. The test kept the first package in key order for each
matching footprint. Native shell baking and packing took 21.13 s.

At 720p30 and a 66° vertical field of view, eight 128-texel banks were
eligible for the turn and seven for the 16 m lateral flight. Three paired
A18 Pro Metal runs with the same 240 km panorama:

| Motion | Peak submitted triangles, mesh → cards | GPU p99 | Compressed reads |
|---|---:|---:|---:|
| Full turn | 1,503,911 → 1,461,335 | 17.97 → 17.80 ms | 118.23 → 116.73 MB |
| Lateral flight | 910,647 → 910,647 | 13.51 → 13.45 ms | 118.23 → 116.87 MB |

Individual card captures took 128–180 ms before writing. The turn's
replacement cut took 169 / 164 ms to prepare, and uploads grew from
183.07 to 186.42 MB. The time differences between frame variants are
within run-to-run variance. Three additional opened headings and both
motion endpoints show roof-edge differences in the replaced blocks.
At the original 55° framing every sampled site's projected texel exceeded
one pixel. Larger banks exceed the decoded overlay-byte budget in this set.
The larger city is now a load and coverage test. A more compact card bank
is needed for the middle tier to replace a meaningful part of it.

## Trimmed building card textures

On import, the building group keeps its square capture grid and depth patches.
Each stored texture can use a smaller power-of-two rectangle. The rectangle
aligns to 16 capture texels and keeps a 16-texel filter border at interior
edges. It copies the original samples through mip 4; deeper sampling clamps
to that mip. When the padded rectangle fills the capture the format keeps
the original square and all mips. Old square cards still decode.

Forty candidate Tokyo tiles used 256-texel views. The 520 views went from
390.92 to 245.45 MB decoded. A separate full-bank Metal control fell from
10.13 to 5.93 MB uploaded. At 1,850 m at 20°, 43° and 83° elevation and at
5,000 m at 43°, comparing identical square and trimmed captures gave three
pixel-identical results and one pixel differing by 1/255. The Körbersee
square-card flight remained pixel-identical.

Three paired 720p30 runs at the original 55° Tokyo lens with the same 400
terrain tiles and 240 km panorama:

| Motion | Active groups | Peak submitted triangles | GPU p99 | Compressed reads |
|---|---:|---:|---:|---:|
| Full turn | 0 → 17 | 1,329,065 → 1,280,836 | 17.43 → 17.90 ms | 118.28 → 115.38 MB |
| Lateral flight | 0 → 16 | 767,473 → 765,456 | 12.41 → 12.30 ms | 118.28 → 115.52 MB |

The square bank exceeded the overlay-byte budget. The trimmed bank lowered
retained Metal resources by about 2.7 MB after the cut. Total turn uploads
rose from 183.07 to 191.82 MB because the initial geometry cut was also
uploaded. A p99 gain in the whole turn remains unproven. At the replacement
boundary the cut still includes a broad geometry preload.

## Admit ready groups before the first frame

For the initial cut, a building group that covers the entire eye sweep can
replace its overlay immediately once its views and split ground validate.
A sweep crossing the mesh-to-card band keeps geometry in the initial cut.
It stages the handoff asynchronously. A terrain partition with potential
children and a canopy site with a near layout can also defer refinement.
If there is no deferred work, the first cut is final for that replay and
the pacing stays active. There is no second decode or GPU upload for it.

On A18 Pro Metal at 720p30, the same loaded Tokyo fixture at 55° with a
240 km panorama measured:

| Motion | First refined frame | Compressed reads | Total uploads | GPU p99 |
|---|---:|---:|---:|---:|
| Full turn | 5 → 0 | 115.38 → 57.71 MB | 191.82 → 179.70 MB | 17.99 → 19.02 ms |
| 16 m lateral flight | 5 → 0 | 115.52 → 57.84 MB | 191.39 → 179.81 MB | 12.30 → 12.40 ms |

Three paired 120-frame runs kept identical endpoints. The median startup
preload was 165 → 166 ms in the turn and 164 → 167 ms in the flight.
The frame timing runs still include startup. The win here is asset
traffic and availability from frame zero.

Removing, corrupting or oversizing one selected view made its tile fall
back to the same 7,433 overlay triangles in all three cases. The other
16 groups and all 400 terrain tiles stayed resident. The separate Tokyo
and Körbersee refinement flights still promoted at frame 1 and reached
pixel-identical endpoints. Across the 17 admitted Tokyo groups, two
selected views per group replace 162,078 source overlay triangles with
82,772 patch triangles before viewport rejection.

## Keep fully opaque tiles on an opaque Metal pipeline

In eight headings on the same Tokyo cut, the isolated far panorama had
GPU p99 between 0.51 and 1.84 ms. The dense westward view submitted
about 1.27 M triangles with cards and still took over 20 ms in the short
combined probe. Meshes with full coverage had been using a fragment
shader with a possible discard for LOD handoffs. A separate opaque
pipeline removes that branch for fully covered draws. Active handoffs
keep the original fragment mask.

Three paired A18 Pro 720p30 replays used the complete 400-tile test fixture,
17 eligible groups and the same panorama as before:

| Motion | GPU p50 | GPU p99 |
|---|---:|---:|
| 360° turn | 8.56 → 7.31 ms | 16.77 → 13.16 ms |
| 16 m lateral flight | 10.19 → 8.52 ms | 12.38 → 9.13 ms |
| 60-frame west view | 12.49 → 11.54 ms | 34.11 → 29.37 ms |

The west probe had high run-to-run variance. All nine pairs had the
same triangle counts and were pixel-identical at their endpoints.
The Tokyo building handoff and Körbersee canopy detail handoff also kept
identical endpoints and continued to exercise the masked draws.

## Dense city handoff exposes missing roof coverage

Tokyo tile `17-116393-51620` exposes a failure of the original view bank.
At bearing 245.21°, pitch −21.77° and vertical FOV 50°, a 36 m east-west
sweep crosses the group's 1642.6–1662.0 m handoff in both directions. All
400 terrain tiles remain present. Each 120-frame A18 Pro run blends for
75 frames after refinement at frame five. Removing only that group
binding gives an exact mesh control for the affected tile. The initial
image agrees with the control. The final image loses roofs across a
150 × 41 px region where the rest of the frame still agrees.

The eye is 16.75° above the group centre. Its two admitted directions are
horizontal and miss the tops of buildings. Bake the same tile at the
following azimuth/elevation pairs in that order: (135°, 0°), (90°, 0°),
(105°, 16°), (120°, 16°), (114.79°, 16.75°). Capture all five at 256
texels and pack the first four at both 128 and 256 texels. Compare the
reprojected pairs against the fifth capture on the same 128 × 128 grid.

```sh
python3 -B test/experiments/render/group_impostor_views.py compare-pairs \
  "$capture256" "$result" --probe 4 --side 128 \
  --pair "$capture256" 0 1 --pair "$capture128" 2 3 --pair "$capture256" 2 3
```

| Direction pair | Coverage IoU | Missing coverage | Raw card bytes | Patch triangles |
|---|---:|---:|---:|---:|
| Horizontal, 256 | 28.3% | 71.1% | 1,428,516 | 3,034 |
| Oblique, 128 | 89.8% | 4.0% | 381,820 | 3,222 |
| Oblique, 256 | 89.4% | 4.8% | 1,500,916 | 10,274 |

The raw byte counts include untrimmed mip chains. The original horizontal
pair occupies 379,644 bytes after trimming. The 128-texel oblique pair
fits within the 411,856-byte source overlay and preserves roofs in the
opened Metal replay. It still brightens roof and wall edges.

A temporary 2 px projected-texel trial replaced all 40 authored banks
with 128-texel cards. The 33 bake directions use rings of 8, 12, 8 and 4
azimuths at elevations 0°, 16°, 40° and 65°, plus overhead. The 16° ring
is offset by half a sector. On the same 400 tiles at 720p30 and 55°:

| Motion | Peak submitted triangles | GPU p99 | Compressed reads |
|---|---:|---:|---:|
| 360° turn | 1,280,836 → 1,198,936 | 14.07 → 12.61 ms | 57.71 → 107.02 MB |
| 16 m lateral flight | 755,486 → 738,707 | 9.17 → 9.19 ms | 57.84 → 107.02 MB |

Medians of three pairs. The prototype admits all 40 groups and refines
at frame five. The current cut re-reads the tile set for that refinement.
Keep the 1 px default while testing directional coverage, relighting
and partial refinement on independent views.

Separately, a native 1024-face recapture improves the visible city band
beyond the overlap in the 36 m flight. For 512 → 1024 texels the median
GPU p99 over three pairs is 12.04 → 11.93 ms, within the spread of the
512 runs. The colour/depth payload grows from 18 to 72 MiB
(`6 × side² × 12 B`). Use the larger capture as a visual control.

## Preserve the lighting of mixed roof and wall texels

Filtering albedo and normal independently can light a bright wall as a
roof. Accumulate each captured normal with its Rec. 709 albedo luminance
as its weight, then normalise for storage. The existing alpha coverage,
texture layout and depth patches stay the same.

On six Tokyo tiles at four held-out angles per tile, apply the same sun,
sky and ground terms to both sides. The reference shades the capture
samples before reducing them. Measure only samples with at least half
coverage in both images. The mean absolute RGB error after tone mapping
fell from 8.36 to 6.02 percentage points. The mean brightness excess
fell from 5.52 to 0.35 points. Coverage and patch counts stayed the same
in all 24 comparisons. Two low-sun orientations in the Python reference
had less improvement. Using the frontmost material sample alone was
worse than the weighted mean for both of those orientations.

```sh
python3 -B test/experiments/render/group_impostor_views.py compare-pairs \
  "$capture" "$result" --probe "$probe" --side 128 \
  --pair "$before" "$left" "$right" --pair "$after" "$left" "$right" \
  --light "$panorama/panorama.json"
```

In the native 720p Tokyo handoff endpoint, the same 168 × 50 px crop
against the mesh control had 1,088 → 656 pixels more than 0.1 brighter
on the encoded brightness scale. Mean brightness excess dropped from
2.97 to 0.23 points. Some silhouette and shading differences remain.
Three paired A18 Pro replays of the same 40-bank, 128-texel trial kept
the triangle and upload counts unchanged. GPU p99 was 12.84 → 13.01 ms
for the turn and 9.17 → 9.19 ms for flight, within run-to-run spread.

## Bind cached building handoffs in the initial cut

If the views for a building handoff are ready, load them with the
resident geometry in the first cut. Both endpoints already belong to
the same coverage partition. Missing or invalid views retain the mesh.
Terrain child partitions and canopy mesh refinement still stage in the
background.

Three paired A18 Pro replays of the 36 m Tokyo flight, at 720p30 with
400 tiles and a 240 km panorama, gave the same peak triangle count
(1,009,092), the same 75 blended frames and pixel-identical endpoints.

| Metric | Staged | Ready in first cut |
|---|---:|---:|
| First frame with both representations | 5 | 0 |
| Compressed reads | 113.03 MB | 57.97 MB |
| Shared uploads | 187.29 MB | 180.23 MB |
| Median GPU p99 | 11.98 ms | 12.01 ms |
| Median preload | 164.95 ms | 165.81 ms |

This eliminates the second scan for the cached handoff alone. A flight
that also needs terrain or canopy refinement can still re-read products.
Corrupting one selected card gives the same 400-tile image as removing
its group binding. The Körbersee terrain and canopy flight still refines
at frame one and reaches the same image as before.

## Keep elevated coverage throughout the eye sweep

A building group viewed from above needs an elevated capture in the
selected pair throughout the sweep. The second-best level direction
and best elevated direction compete at every ordering change, including
ties. If the pair can lose elevated coverage, keep the source mesh.
Cards that pass that check still need to fit the overlay byte budget.

The default projected texel limit stays at 1 px. A validated bank can
set `maximum_projected_texel_px` in its manifest. The accepted range
is 1–2 px, covering the original limit and the 2 px city trial above.
With the 40 relit oblique banks imported at 2 px, the normal loader
admitted all 40 on the 55° replay. The original horizontal banks stayed
on geometry. Each pair below uses the same 400 tiles, camera and
512-texel panorama at A18 Pro Metal 720p30.

| Motion | Peak triangles, geometry → cards | GPU p99 | Compressed reads |
|---|---:|---:|---:|
| 360° turn | 1,329,065 → 1,198,936 | 14.22 → 13.20 ms | 60.60 → 54.99 MB |
| 16 m lateral flight | 767,473 → 738,707 | 9.06 → 9.25 ms | 60.60 → 54.99 MB |

The table uses medians of three pairs. The 36 m route with the clearer
1024-texel panorama loaded three mesh/card handoffs from frame zero.
It blended for 97 and 98 frames in the two directions at 11.67 and
11.65 ms GPU p99. The opened frames preserved the roof band through
the transition. Small silhouette and shading differences remain.
The 14-group Körbersee turn and flight retained their pixel-identical
endpoints and unchanged triangle counts.

## Locate the far transition in the image

A native diagnostic replay stores far weight in red and near and far
axial depth in green and blue. Decode sRGB and scale both depths by
twice the overlap end. At the west-facing 36 m endpoint, no sampled
pixel in the far overlap lacked the local depth. The median number of
pixels per column between 10% and 90% far weight was 20 at 512 texels
and 19 at 1024. The higher resolution softened the apparent break in
detail with essentially the same band. A recapture widening the
overlap from 1510–1760 m to 1000–1760 m raised the median to 84 px
and exposed double contours in buildings. Keep the narrower overlap.

[Billboard Clouds](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
[Layered Depth Images](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
[Hierarchical Image Caching](../../../doc/references/geometry/siggraph/1996-hierarchical-image-caching.pdf).

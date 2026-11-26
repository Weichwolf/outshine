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
city coverage and near-to-mid switching during movement remain open.

Compositing the single spruce captures at the 25 roots reaches 82% median and
64% worst coverage overlap against the direct stand captures. One native bake of
24 views took 0.98 s for the tree and 21.02 s for the stand. The Python composition
of all 24 stand views took 0.22 s. This measures a bake candidate, with native
capture including GPU submission and readback. Runtime GPU cost and continuity
through both world handoffs require separate measurements.

[Billboard Clouds](../../../doc/references/vegetation/siggraph/2003-billboard-clouds.pdf),
[Layered Depth Images](../../../doc/references/vegetation/siggraph/1998-layered-depth-images.pdf),
[Hierarchical Image Caching](../../../doc/references/geometry/siggraph/1996-hierarchical-image-caching.pdf).

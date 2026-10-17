# Local terrain geometry comparison

Owner: [#515](https://github.com/Weichwolf/outshine/issues/515), representation in
[#12](https://github.com/Weichwolf/outshine/issues/12).
`terrain_patch_lod.py` uses existing native height-page exports. No regenerated
assets, altered world inputs, reduced visibility or runtime tolerance change.

## Method

Split each 33x33 page into four-, eight- or sixteen-cell square patches. Each
candidate stride retains every original patch boundary vertex and edge. Adjacent
patches therefore share their exact fine boundary regardless of selected stride.
All 256 original outer skirt triangles remain; there are no interior skirts.

Use `terrain_grid_lod.stencil` for the exact common refinement. Patch axes are
normalized independently, including irregular sampling fractions. The Euclidean
position difference reaches its maximum at a common-refinement vertex because
it is affine on each intersection polygon and its norm is convex. Bounds include
native local positions and sag. Camera distance uses each patch's enclosing
sphere, rather than the entire page's sphere. The same conservative projection
formula and pixel comparisons as the page experiment apply.

Four independent checks cover dense vector interpolation on different irregular
axes, all shared boundaries, an omitted interior peak and a camera inside the
bounding sphere. The existing six full-page checks also remain unchanged.

## Measurements

1280x720, same camera height, one-pixel **positional comparison**, eight-cell patches.
These are CPU counts before any depth rejection; they are not GPU timings.

| Place | All-resident original triangles | Selected triangles | Reduction |
|---|---:|---:|---:|
| Tokyo | 8,656,128 | 4,752,024 | 45.10% |
| Wien | 8,393,472 | 4,092,176 | 51.25% |
| Koerbersee | 37,451,520 | 19,520,584 | 47.88% |

For Tokyo's two opposite views, original frustum counts are 2,024,192 / 2,419,456;
selected counts are 1,042,696 / 1,416,920. Körbersee changes 7,607,552 / 6,820,480
to 3,996,040 / 3,554,128. Each includes skirts for frustum-intersecting pages.

Tokyo's same one-pixel all-resident comparison by patch size:

| Patch cells | Selected triangles |
|---|---:|
| 4 | 5,364,040 |
| 8 | 4,752,024 |
| 16 | 5,782,776 |

Eight cells give the best measured balance of local bounds and retained boundary
cost. The CPU process peaks at about 34 MB for Tokyo, reading 18,409,300 height
bytes; reports record actual processed pages/patches, fingerprints and peak RSS.
Concurrent CPU runs were used; their wall times are not native budget estimates.

## Integration decision and remaining proof

This supports a bounded native prototype on the existing eight-cell patch path.
It does not approve a smaller runtime error limit or establish image equivalence.
Keep full-detail normals independent of geometric stride: interpolating fewer
vertex normals changes lighting even if positions remain within a pixel.
Native selection must include rounding margins, preserve outer skirts and shadow
behavior, and account for extra index ranges, compaction and indirect draws.
Compare complete images and total GPU time on Tokyo/Wien/Körbersee before adopting.

```sh
python3 test/experiments/render/verify_terrain_patch_lod.py
python3 test/experiments/render/terrain_patch_lod.py \
  --pages "$PAGES" --scenario "$SCENARIO" --cells 8 \
  --output "$TMPDIR/outshine-local-terrain.json"
```

[Geometry Clipmaps, SIGGRAPH 2004](../../../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf)
provides the geometry/normal-detail separation reference. These boundary fans
and exact common-refinement comparisons are this repository's experiment.

# Terrain index LOD comparison

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12).
The input is the existing `terrain_cache_reference.py` export of a native cached
ground region. No source download, generator change or renderer replacement.
Requires NumPy and Shapely; verification additionally uses Matplotlib's independent
triangle interpolator. Outputs belong in system temp.

```sh
python3 test/experiments/render/verify_terrain_grid_lod.py
python3 test/experiments/render/terrain_grid_lod.py \
  --pages /tmp/outshine-terrain-pages \
  --scenario src/assets/places/Tokyo.scenario \
  --output /tmp/outshine-terrain-lod.json
```

## Method and limits

Shared index sets select interior grid strides 1/2/4/8/16/32. Boundary cells use
triangle fans, retaining every original boundary vertex and edge. The original
256 skirt triangles remain unchanged. Topology checks cover winding, edge
multiplicity, square coverage and the complete boundary. Counts per patch,
including skirts: 2304/952/536/424/392/384 triangles.

The reference includes native sampling fractions, tile-local WGS84 geometry and
the renderer's spherical sag. GEOS constructs the common refinement of the fine
and coarse triangulations. Their positional difference is affine on each common
polygon; its Euclidean norm is convex. Its maximum therefore occurs at a polygon
vertex. Identical common vertices are evaluated once; retained coarse corners
have zero difference. This also bounds the unnormalised interpolated normal
vector difference, which is reported separately.

Checking only original vertices is insufficient: the 3x3 negative control retains
all nine vertices but flips two cell diagonals. Both meshes agree at every vertex
and still differ by one metre at an edge crossing. Verification also covers an
omitted peak, irregular fractions, affine fields and independent dense vector
interpolation. These checks do not establish globally optimal triangulation.

For geometry error E, focal length f, diagonal-frustum cosine c and lower distance
bound d, the comparison uses fE / (c(dc-E)); nonpositive denominators retain the
original grid. The distance bound comes from a sphere enclosing the original
surface. This is conservative in the ideal piecewise-affine model. Shader float
rounding, jitter, actual projection and runtime images remain separate checks.
Normal-vector error does not establish a lighting/material error bound.

## Measured comparison, 2026-10-16

All-resident 360-degree region at 1280x720, same scenario camera and heights:

| Place | Pages | Original triangles | 1 px comparison | Reduction |
|---|---:|---:|---:|---:|
| Tokyo | 3757 | 8656128 | 7558864 | 12.68% |
| Wien | 3643 | 8393472 | 6515048 | 22.38% |
| Koerbersee | 16255 | 37451520 | 32148304 | 14.16% |

These are all-resident counts, not per-view submissions or GPU timings. The
0.25/0.5/1/2/4 px cases are comparisons, not new product limits. Four pixels save
68–72% but admit large interpolated-normal differences; they are not approved.
The input cache headers were checked against each scenario's exact world anchor.

Duplicate-point removal reduced the Tokyo calculation from 55.8 to 10.0 seconds
under a concurrent native gate. Streaming pages preserves all comparison counts;
Koerbersee took 42.2 seconds at 67534848 bytes peak process RSS. It does not retain
all expanded page positions/normals. CPU preparation and native runtime remain
different costs; no A18 frame-budget or image-improvement claim follows.

This boundary-preserving variant does not provide an order-of-magnitude reduction
at one pixel. Compare neighbor transitions and current-frame hierarchical occlusion
before admitting extra renderer complexity;
keep shading normals independent of geometric detail as described in
[Geometry Clipmaps, SIGGRAPH 2004](../../../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf).
The local paper's transition and independent normal-map discussion was inspected.
No production LOD bound, shader, asset version or image pin changes here.

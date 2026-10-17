# Terrain visibility against current building depth

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12).
Uses the [native building export](current_visibility.md) and existing
`terrain_cache_reference.py` height-page export. No source requests, asset rebuild,
renderer change or relaxed geometric error tolerance.

```sh
python3 test/experiments/render/verify_current_visibility.py
python3 test/experiments/render/verify_terrain_visibility.py
python3 test/experiments/render/terrain_visibility.py \
  --structures /tmp/outshine-building-export --pages /tmp/outshine-terrain-export \
  --output /tmp/outshine-terrain-visibility --width 1280 --height 720 --cells 8
```

## Method

Rasterize the complete exported opaque building mesh into current-view reversed
depth. Partition each unchanged 32x32-cell terrain interior into 32/16/8/4-cell
blocks. No triangle changes, simplification or additional internal skirt occurs.
Every patch's indexed vertices determine a conservative AABB. Query the min-depth
pyramid with that bound; retain uncertain and near-crossing patches.

Rasterize all terrain interiors into the reference, regardless of frustum
selection. Rasterize only retained patches into the result. Both images include
the same building depth. Compare every pixel. Two additional tests check the
exact triangle union and enclosure of triangle interiors at every partition size.

The native page geometry includes its sampling fractions, tile-local WGS84 axes
and spherical sag, using `terrain_grid_lod.py`. The reference keeps its previously
documented limits: pixel-centre single-sided opaque rasterization, omitted
near-crossing triangles and no material alpha, MSAA or shader-rounding proof.
Skirts are excluded from both terrain streams and all reported triangle counts.
This experiment does not permit removing native skirts or modifying shadows.

## Tokyo, 2026-10-16

Same building asset and selected eye as `current_visibility.md`; the terrain
export contains 3757 actual cached pages. Input: 3757 x 2048 = 7694336 terrain
interior triangles. Four pyramid levels, 1280x720, current building depth:

| Yaw | Cells per side | Candidate triangles after frustum | Hidden triangles | Remaining |
|---:|---:|---:|---:|---:|
| 0 | 32 | 2605056 | 51200 | 2553856 |
| 0 | 8 | 2128640 | 1117824 | 1010816 |
| 180 | 32 | 2230272 | 0 | 2230272 |
| 180 | 8 | 1788672 | 138112 | 1650560 |

All four complete reference comparisons have zero depth difference and zero
changed pixels. The 8-cell case rejects 1117824/2128640 = 52.51% of its frustum
triangles at yaw 0, but only 138112/1788672 = 7.72% at yaw 180. This is one camera
and one building asset, not a whole-scene or all-Places savings guarantee.

Subdivision creates 16 candidates per page: 60112 instead of 3757 overall.
Frustum and occlusion gains must therefore pay for the extra jobs, compaction,
patch addressing, depth hierarchy and pass/attachment traffic on the GPU.
The streaming CPU reference takes roughly 30-33 seconds at 720p and holds only
one terrain page's geometry at a time. It is a validation tool, not a runtime
algorithm or device timing prediction.

Four levels can query at most 4x4 blocks of 16x16 pixels. Merely retaining whole
tiles therefore loses much of the exclusion opportunity at 720p. A query-only
comparison at yaw 0 with ten levels rejects 360448 whole-tile triangles instead
of 51200; the 8-cell case improves from 1117824 to 1165056. The ten-level counts
do not have a separate complete terrain raster comparison yet.

Native selection must use the current frame's actual opaque depth, keep unknown
coverage visible, preserve fractions and outer skirts, and prove camera motion,
lighting and total GPU cost before admission. The existing subject pass already
renders buildings before ground, so current depth reuse is preferable to drawing
the same building mesh again solely for this culler. The implementation decision
and runtime acceptance remain in #12.

Further [terrain work-selection comparisons](terrain_work_selection.md) cover bounded
lower envelopes, exact early terrain, projected bounds and meshoptimizer normal cones.

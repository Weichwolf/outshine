# Native terrain depth replay

Owner [#1919](https://github.com/Weichwolf/outshine/issues/1919).

Replay the current `TerrainPatchBounds::Bounds` and 192-byte selection view,
four native min-depth levels and rectangle/LOD selection against an actual
completed frame depth image. Preserve the existing 0.5-pixel geometric limit,
one-pixel rectangle padding and absolute `1e-6` reversed-Z depth bias.

```sh
python3 -B -Werror -m unittest discover -s test/experiments/render \
  -p test_terrain_depth_occlusion.py
python3 test/experiments/render/terrain_depth_occlusion.py \
  --snapshot "$SNAPSHOT" --depth "$DEPTH" --benchmark "$BENCHMARK" \
  --output "$TMPDIR/outshine-terrain-depth.json"
```

Inputs are a diagnostic capture, not a new asset format: `manifest.json` schema1
identifies one group with unchanged `bounds.bin`/`view.bin` SHA256 and patch count.
Bounds contain three Float32 vec4 words per patch; the view keeps the native
column-major matrix and unsigned count words. Depth is top-left row-major
Float32 at the view's full source dimensions. The benchmark binds the completed
world, profile, commit and observed final LOD counts. Captures/outputs stay in
system temp; normal tests use independent small controls and no private files.

Supply `--before-depth` when existing object depth already rejects terrain.
Otherwise the replay assumes no initial depth rejection. The model refuses a
comparison when its four baseline counts differ from the native report.
Matching totals do not establish exact per-patch GPU membership or general
Float32/raster equivalence: replay arithmetic uses Float64. Unknown bounds and
eye-plane crossings remain fine and visible; oversized rectangles fail open.

## Decision and limits

Actual Jura capture on `f3de7e822`: 30,496 valid bounds, uniform fractions, no
eye-plane/unknown bounds, four native counts reproduced exactly. Current final
depth would remove 7,840 patches and 1,711,104 of 6,696,696 patch index references:
`1,711,104 / 6,696,696 = 25.55%`. Skirts remain separate. Costs, raw bindings,
whole-frame results and private capture procedure are in #1919.

This is a hypothetical work reduction. Final depth contains all actual depth
writers; it is not automatically an admissible terrain prepass. Its construction,
extra vertex/raster/bandwidth work and subsequent colour-depth agreement remain
native obligations. No new default, reduced tolerance, GPU speedup, device budget
or moving-world qualification follows. Changes require the complete native
comparison described in #1919 before adoption.

Design context: [GPU-driven rendering pipelines, SIGGRAPH2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf)
for visibility before shading and [Geometry Clipmaps, SIGGRAPH2004](../../../doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf)
for separate geometric and appearance detail. The concrete replay models this
repository's current shaders; it does not implement either paper wholesale.

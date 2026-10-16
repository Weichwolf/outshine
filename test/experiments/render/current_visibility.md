# Current-view building visibility

Owner: [#12](https://github.com/Weichwolf/outshine/issues/12).
This CPU experiment compares occluder selection, bounding volumes and cluster
granularity on an existing native `building-lod` cache asset. It does not modify
the renderer, fetch sources, regenerate assets or admit a new runtime path.

## Reproduce

Requires a built engine, its compilation database, NumPy and the existing cached
asset. Use the engine revision matching the asset codec; obsolete entries can
fail decoding. The exporter loads the requested asset and its prepared parent.
Outputs belong in system temp; never place them inside the source/asset cache.

```sh
python3 test/experiments/render/structure_cache_reference.py "$ASSET_DB" "$KEY" \
  --output /tmp/outshine-visibility-input
python3 test/experiments/render/verify_current_visibility.py
python3 test/experiments/render/current_visibility.py \
  --input /tmp/outshine-visibility-input --output /tmp/outshine-visibility-result \
  --width 1280 --height 720 --seeds 300 --bounds aabb --levels 4
```

The export receipt contains the compile command, engine revision and exporter
hash. `report.json` records the asset identity, work counts and separate CPU
preparation, seed rasterization, query and reference times. `depths.npz` contains
the seed, full and retained depth images. `--cluster-triangles 16` subdivides each
existing leaf's index run without changing triangles or crossing its boundary.

## Method and limits

The camera uses the asset's selected ECEF eye and focal length, looks toward the
mesh's mean horizontal position, pitches down by approximately 11 degrees, and
supports yaw offsets. This is a geometry experiment, not the canonical Place
heading or a measured GPU frame. Positions remain ECEF offsets from the parent.

The seed selects the 300 safe visible clusters with the largest clipped projected
bounding area per triangle. A reversed-depth min pyramid keeps uncovered pixels
zero, including partial odd-sized blocks. Queries use conservative sphere/box
projection, screen intersection and at most 4x4 samples; near-plane intersections
remain visible. A four-level chain matches the existing renderer's level count;
longer chains are alternatives, not necessarily more selective around holes.

The reference rasterizes all input triangles, independently of frustum selection.
It models single-sided opaque triangles at pixel centres without MSAA, material
alpha, deformation or near-plane clipping. Near-crossing triangles are omitted
from both images and cannot supply occlusion. Equal CPU depth images therefore
do not prove material/color equality, native rounding or runtime visibility.
The full-depth query diagnoses the same bounds/query with all reference geometry
available; it is not a realizable seed plan or an optimality proof.

Verification compares the vectorized rasterizer against a scalar barycentric
oracle; checks holes, camera disocclusion, equality, near-plane intersections,
offscreen bounds, odd pyramid edges, projected bounds and source-preserving runs.

## Tokyo comparison, 2026-10-16

Native integration revision `19295714d`; asset
`27e54f346f191910d1e5c9d48502c83e050b284693fe788b04a0d2543ac7f8c3`.
The exported mesh contains 516875 triangles and 4039 existing leaf clusters.
At 1280x720, four pyramid levels and 300 seeds:

| Yaw | Bounds / maximum triangles | Frustum triangles | Seed-culled triangles | Full-depth diagnostic |
|---:|---|---:|---:|---:|
| 0 | sphere / 128 | 208768 | 0 | 0 |
| 0 | AABB / 128 | 191360 | 0 | 256 |
| 0 | AABB / 16 | 136224 | 2320 | 28224 |
| 180 | sphere / 128 | 153600 | 128 | 256 |
| 180 | AABB / 128 | 132224 | 256 | 1024 |
| 180 | AABB / 16 | 90192 | 4672 | 23104 |

All six retained depth images match the complete reference with zero changed
pixels at a 1e-12 comparison threshold. Sixteen additional 320x180 comparisons
cover four yaw directions, two bound types and four/ten pyramid levels; all match.
These counts are for one asset, not the entire Tokyo scene or terrain.

The large apparent gain from tighter/smaller bounds is predominantly frustum
rejection. For yaw 0, AABBs save (208768-191360)/208768 = 8.34% before occlusion.
Smaller clusters multiply visibility jobs by roughly eight; 300 partial clusters
still form a poor occluder surface. Merely adding this seed pass is rejected.

Next native comparisons must include the dominant terrain work, coherent opaque
occluders, disocclusion and the total cost of extra jobs/passes. No runtime speedup,
budget pass or smaller LOD error bound follows from this experiment.

Reference: [Aaltonen/Haar, GPU-Driven Rendering Pipelines, SIGGRAPH 2015](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
pages 26-27: selected occluders, full-resolution depth, conservative downsampling
and GPU culling. The cited PS4 timings are not A18-Pro measurements.

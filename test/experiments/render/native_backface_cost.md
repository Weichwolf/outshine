# Backface work on native input

Owner: [#894](https://github.com/Weichwolf/outshine/issues/894).
Compare whole-group normal cones and per-triangle directional masks before
changing the native pipeline. Geometry, source indices and selected LOD remain
unchanged. These are Double geometric counts, not GPU/frame savings.

## Input and reproduction

Use the shared [selected native fixture](native_visibility_pipeline.md).
`batch-states.json` adds three recorded state runs covering all 6,134 draw rows:
actual raster backface culling, surface kind and declared rigid/deforming motion.
Its input hashes bind every file used for geometry/selection/model/state mapping.
The fresh observer on C++ `e2ae80985` and shaders `e34537d3b` has the same 11,938
selected jobs, models, 4,284,198 consumed index words and 2,174,806 referenced
vertex IDs as the existing fixture, with unchanged values and byte addresses.
Unread zeroed slots remain irrelevant. No additional geometry archive is needed.

```sh
task_dir=$(mktemp -d "${TMPDIR:-/tmp}/outshine-backface.XXXXXX")
PYTHONPATH=test/experiments/render python3 -B - "$task_dir/input" <<'PY'
import sys
from pathlib import Path
from native_visibility_data import extract
extract(Path("test/outshine/data/render/visibility/selected-native.tar.xz"), Path(sys.argv[1]))
PY
python3 -B test/experiments/render/native_backface_cost.py \
  --input "$task_dir/input" \
  --batch-states test/outshine/data/render/visibility/batch-states.json \
  --output "$task_dir/result.json"
python3 -B -m unittest discover -s test/experiments/render -p 'test_native_backface_cost.py'
```

Only raster-culled, declared rigid jobs qualify. A deforming designation is not
silently treated as static, even if a single captured pose looks unchanged.
Degenerate/uncertain cones fail open. Mask queries outside the truncated region
retain all triangles; source-sphere enclosure is checked. Perspective eye/model
inversion and geometric normals use the original Float32 inputs in Double.

## Methods

The cone axis is the normalized sum of unit geometric normals. Its aperture
encloses every normal. Moving the apex behind every triangle plane permits a
whole-group facing test without assuming a camera distance. This construction
does not claim the minimum enclosing cone.

Directional masks follow Haar/Aaltonen, SIGGRAPH 2015, PDF pages 22–24;
[local paper](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf),
diagram on page 23 opened. Test one or four cells per cube face. Each cell has
four unnormalized corner rays `r`. A plane has offset `b=n·(centre−vertex)`.
For cameras `centre+t*r` with `t≥cutoff`, reject only if `max(n·r)<0` and
`b+cutoff*max(n·r)<−margin`. Convex combinations preserve the inequality.
Cutoff distance is measured on the dominant cube axis, not Euclidean distance.
The reference evaluates the queried cell; it does not allocate a runtime mask
table. Float32 query/quantization, native winding/raster and GPU costs remain open.

## Recorded view and decision

Selected input: 1,428,066 triangles. Qualified rigid input: 597,858; individual
geometric backfaces within it: 188,164. The original 128-triangle partition has
only 122 triangles in groups whose every face satisfies the same strict geometric
backface predicate; the simple cone removes all of them. Repartitioning,
edge-on/degenerate handling, frustum and depth introduce different criteria.
This is a fixed-partition control, not global optimality.

| Triangles/group | Groups tested | Cone rejection | Whole-group strict-facing control |
|---:|---:|---:|---:|
| 128 | 5,452 | 122 | 122 |
| 64 | 10,122 | 135 | 141 |
| 32 | 19,433 | 313 | 1,033 |
| 16 | 38,094 | 1,150 | 3,791 |
| 8 | 75,392 | 5,373 | 14,205 |

Six directional cells reject at most 35,786 triangles here (cutoff four radii).
Twenty-four cells reach 65,721 (cutoff two radii): 65,721/1,428,066 = **4.60%**.
All comparisons have zero false rejections against the geometric margin reference
and zero sphere-enclosure failures. Synthetic controls cover front/back/degenerate
planes, truncated face regions, material/motion exclusion and incorrect inputs.

Decision: no native cone/mask port from these counts. Existing mixed-orientation
groups bound whole-group selectivity; finer groups already have unfavorable
native costs on Iris. No new mask/cache format. Prioritize shared far geometry
and a current-depth comparison that charges its actual depth construction.

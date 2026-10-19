# Two-phase current-frame cluster visibility

Owner #891, parent #844. Python reference; no native integration or admission.

`GeometryCullStage::EncodeCull` (8a1a0ee3f, lines121–132) invalidates the old depth
when the selection projection changes. `RenderCatalogue` supplies last-frame depth.
That guard is necessary for final rejection. This experiment tests the established
two-phase alternative instead of removing the guard or reusing a point Capture.

## Contract and execution

1. Current geometry bounds determine current-frustum candidates. Old depth only
   divides them into phase1 and a deferred list. Invalid/missing history keeps all.
2. Render phase1 **at the current pose**, then build its current depth hierarchy.
3. Retest every deferred group against that current depth. Draw the groups that
   are not demonstrably hidden. Finally build the next frame's depth hierarchy.

A depth hierarchy stores maximum **positive linear camera depth**, with missing
samples/padding represented by positive infinity. This is an independent CPU
control, not a port of the product's reversed-Z/min-depth or four-level shader.
Projected group AABBs are queried conservatively over a fixed 2×2 point lattice;
coarser hierarchy cells include supersets of the queried points. A hole, a near-
plane crossing, incomplete crop, equal depth or uncertain boundary keeps a group.
Float64 guards are engineering allowances, not formal all-input certification.

`native_view_clusters.py` reads the actual archived v10 lists after the existing
manifest/geometry/hash checks. Every native level-zero index triple binds once to
an original triangle; overlapping, unbound, nonzero-LOD or incomplete ranges fail.
All original vertex arrays, source bindings and triangle winding remain unchanged.
The original membership is retained; AABBs are derived from its actual vertices.
These are **not the native Float32 sphere bounds or native shader bindings**.

The same native Double camera tuples, width1280×height720, focal691.5535657096198px,
world-axis rotation and moving endpoint6m are used. Only the full cell's screen
crop is sampled at the original pixel scale; outside the isolated cell is empty.
A sorted original-face tie rule matches the CPU full path. Native coplanar ties,
colour/G-buffer writes, raster samples, alpha/material qualification, derivatives,
TAA, reversed-Z precision and both backend/resource lifetimes need separate proof.
In particular, current depth must use the actual current raster projection including
jitter; a guard-expanded selection projection cannot substitute for it.
[Master's global Metal diagnosis](https://github.com/Weichwolf/outshine/issues/891#issuecomment-6277527069)
confirms the additional TAA guard and supplies actual global argument/bounds data;
its last-depth Double controls are separate from this cell and remain unqualified
for Float32/device admission. No guard removal or new data export is requested.

## Complete input and measured outcome

Input: `test/outshine/data/render/building-full-view/cell.json.gz`, SHA256
`31e296fdda1e8ffebba3303bf28f35e4de99c29410e19bc9983a6e375696d3da`.
Original112clusters (90wall/22roof),14169triangles,576sources. The112group records,
original-face membership and derived bounds occupy64744B as actual CPU arrays.
The geometry/material arrays are reused, not replaced with approximated fields.

Both native60-frame tracks are checked:120poses,28visible crops/66992subsamples,
92poses offscreen for this cell. Final depth and original face IDs are bit-equal to
full current geometry, as are interpolated normal/colour/UV/metre fields. The
full-ray reader additionally matches894 exhaustive all-triangle Möller controls
across the main sequence. Same winners preserve original per-triangle input
identities; this is not a complete native PBR/raster equivalence proof.

| Main sequence work | Full current-frustum path | Two phases |
| --- | ---: | ---: |
| Submitted original triangles | 396732 | 396732 |
| Depth hierarchy reduction inputs | 91968 | 183936 |
| Extra historical group tests | 0 | 2912 |
| Historical hierarchy taps | 0 | 14201 |
| Deferred/restored/finally rejected groups | — | 0/0/0 |

**No triangle-work saving in this isolated cell.** Empty depth inside the large
cluster rectangles prevents rejection. These controls lack other scene geometry/terrain as
occluders and do not establish the outcome on the complete native world.
Do not port this as an already demonstrated performance solution.

A deliberately stale near occluder defers all112groups: stopping after phase1
would produce298wrong samples. Current empty phase1 depth restores all112groups
and14169triangles; final errors0. A second fault-injection removes original
cluster26 (128triangles): the14041-triangle current-world baseline also matches
exactly. This is a render-input change control, not a completed world-edit feature.
Contact images include the stale-only failure and repaired result and must be
opened locally. They use stored normals/colours with a Lambert diagnostic light.

Work counts include both depth passes, both newly generated pyramids and the old
history queries. Previous and newly built pyramid payloads are reported separately.
If native colour/G-buffer writes occur in those geometry passes, the triangle
submission count is their combined count. A separate final colour pass would add
**all accepted triangles again**; the report states this alternative explicitly.
Tie handling and extra GPU synchronization/passes are not assumed free.
The candidate wall interval includes both classifications and Python index rebuilds;
reference/control/field validation runs outside it. These are not native GPU times.
Full-diagnostic RSS covers the process, libraries, sources and saved arrays.
Optional tracemalloc measures only its tracked subset and slows the CPU reference;
allocation count and physical copied/traffic bytes remain unknown.

## Reproduction and limits

NumPy, Trimesh/Rtree, Pillow and the existing input checker are used. No third-
party source is copied. Outputs must be fresh and explicit; private images stay
in system temp and are not published in doc/README.

```sh
OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 .venv/bin/python \
  test/experiments/render/compare_two_phase.py \
  --input test/outshine/data/render/building-full-view/cell.json.gz \
  --output /tmp/outshine-research-two-phase-<commit>/tokyo
```

Add `--trace-allocations` only when its diagnostic cost is desired. Dataset and
source hashes are in the report. Focused tests cover holes, ties, invalid/empty
history, brute lattice checks, raw interpolation, original clusters and corruption.

SIGGRAPH first: Haar/Aaltonen, **GPU-Driven Rendering Pipelines**, SIGGRAPH2015
Advances in Real-Time Rendering course, slides51–52; slide52 inspected optically.
[Primary source](https://advances.realtimerendering.com/s2015/aaltonenhaar_siggraph2015_combined_final_footer_220dpi.pdf),
local `doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf`.
The reference borrows the two-phase scheduling idea, not the HTILE implementation.

#844 still needs complete moving-world rendering, hierarchical demand before
individual decode/residency, actual GPU work and both16.67ms/p99 device budgets.
Camera visibility does not remove shadow, reflection, collision or simulation
requirements. The opaque point control is not permission to discard unknown
materials or relax world coverage, sight, contents, resolution or frame rate.

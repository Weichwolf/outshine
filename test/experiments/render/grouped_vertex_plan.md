# Grouped cluster draws

#1955; native input: `resident_vertex_references.md`. This is a reference plan and
native scan comparison, not an integrated renderer speedup.

## Plan

`grouped_vertex_plan.py` groups clustered, singly instanced opaque/masked batches by
material, layout, pipeline, motion, passes and surface kind. Rigid and deforming groups
remain separate; deforming vertices still require the previous-position stream.
Other batches form order barriers and retain their original draw path. No geometry
is discarded. Roots sort stably by group; logical cluster addresses are reassigned.

Each logical cluster retains `(sourceIndexFirst, modelSlot)`. Virtual indices encode
`(logicalCluster << cornerBits) | corner`. `cornerBits` derives from the maximum
template index count; overflow rejects the representation. The reference reconstructs
every selected source index and placement, and compares counts with native arguments.
Group index-buffer slices reserve the sum of root capacities, independent of selection.
`indices.bin`/`raw_indices.bin` contain dense selected references for verification;
`arguments.bin` retains the sparse capacity offsets used by a runtime draw buffer.

## Scan

The existing `geometryRootScan.comp` already accepts multiple roots per batch, but
serializes their loops/barriers within a workgroup. Direct grouping makes it slower.

`grouped_scan/` uses segmented inclusive scans of 256-element blocks. Keys are monotone
group IDs. The first level reads live `kept`/root visibility through cluster metadata.
Each block publishes its final segment sum/key. Those tails are scanned recursively.
A carry applies only when the preceding block's final key equals the element's key.
The last pass writes exclusive cluster offsets and final draw counts. Rejected/zero
clusters get the sentinel; groups with logical clusters mark their index data dirty.
The experiment does not yet preserve the finer existing per-batch compaction cache.

For fixed block size W, work is `O(N log W)` = `O(N)`; temporary levels form a geometric
series. No added scene-size/recursion cap; uint32 addressing and native resource
admission still bound allocations. At 50,572 clusters there are two levels and three dispatches.
The probe checks every resulting offset and every argument word against a separate
CPU scan, including shared roots, hidden stale counts and zero counts. The same GPU
buffers run visible → zero selection → visible again before measurement.

## Reproduce

```sh
python3 -B test/experiments/render/grouped_vertex_plan_cases.py
python3 -B test/experiments/render/grouped_vertex_plan.py /tmp/resident-geometry \
  --output /tmp/grouped-plan
nice -n 10 cmake --build build --target outshine_experiment_group_scan --parallel
build/outshine_experiment_group_scan /tmp/resident-geometry /tmp/grouped-plan \
  build/shaders/grouped-scan
MTL_DEBUG_LAYER=1 build/outshine_experiment_group_scan /tmp/resident-geometry \
  /tmp/grouped-plan build/shaders/grouped-scan
python3 -B test/experiments/render/grouped_scan/cases.py \
  --probe build/outshine_experiment_group_scan --shaders build/shaders/grouped-scan \
  --output /tmp/grouped-scan-cases
```

Fresh output directories required. Normal build/gate does not run the experiment.
Native API and GLSL are shared; Iris Xe measurements remain missing.

## A18 Pro/Metal observations, 2026-11-05

26 rounds with rotating order across all three modes, first two discarded. Values are
native submission GPU means in ms, without upload/readback. Clocks remain unmeasured;
these fixed-selection kernel replays establish no whole-frame or p99 result.

| Snapshot | Source roots / active clustered rows | Planned active clustered groups | Existing scan | Grouped existing scan | Grouped parallel scan |
|---|---:|---:|---:|---:|---:|
| Tokyo | 8,355 / 2,202 | 30 | 0.281 | 4.112 | 0.103 |
| Feldkirch | 4,563 / 1,001 | 134 | 0.124 | 0.875 | 0.035 |
| Jura | 3,752 / 1,175 | 227 | 0.108 | 1.218 | 0.038 |

Tokyo keeps all 4,205,964 selected mesh indices. Other render stages/direct draws remain
outside those clustered counts. Metal validation and 1/255/256/257/65,535/65,536/65,537/
70,001-cluster boundary cases pass. Product images are unchanged and actually inspected.
Next: native grouping, vertex fetch/material/previous-position integration, compaction
cache behavior and full-frame scene comparisons. The larger local-vertex reference
alternative remains relevant for post-transform reuse despite its higher memory cost.

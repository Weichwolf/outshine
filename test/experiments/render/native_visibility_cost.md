# Visibility granularity on native input

Owner: [#894](https://github.com/Weichwolf/outshine/issues/894), movement and world
representation in [#844](https://github.com/Weichwolf/outshine/issues/844).

`native_visibility_cost.py` compares visibility groups while preserving every
triangle selected by the original native GPU. It never changes render LOD,
positions, materials or the set of original index references before selection.
A group is an original contiguous index subrange, not a simplified replacement.

## Input and limits

The input is an explicit private native readback directory: `inputs.json`, the
256-byte CullView in `uniform.bin`, original position/index buffers, jobs, kept
counts, placements, arguments, the final depth pyramid and `draws.json` with
successfully encoded pass/range records. The loader selects only encoded opaque
indirect rows and reconciles their actual argument counts with kept jobs.
Native buffers use little-endian Float32/UInt32 and the 144-byte placement stride.
The producer uses the current native compiler flags; original GPU inputs and
variant source/commands/hashes remain with its private receipt.

The final pyramid is the resource after the last depth stage, not a certified
copy of the historical bytes used by an earlier cull dispatch. Bounds and
projection use Double. Results are cost comparisons, not Float32/continuous
coverage proofs, candidate renderings, moving-world admission or GPU timings.

The four original pyramid levels are retained. Additional minimum-reduction
levels are considered only when the original four-by-four query cap fails;
existing successful fine queries are not replaced by looser coarse ones.
Clear depth remains zero, odd edges repeat, unsafe denominators fail open.
Additional GPU storage/dispatches, selection/compaction and first-phase raster
costs still require measurement. Python array bytes are actual prototype arrays,
not a proposed native layout or measured device traffic.

## Observed decision input

Native basis `2dfdaba32`, last Tokyo pose after the original 60-frame 360-degree
1280×720/60 benchmark: 1,428,066 encoded opaque indirect triangles in 11,938 kept
jobs. All 4,284,198 referenced corners are finite and inside their original
spheres. Observer images match the unmodified native product exactly. This is one
actual world pose; it does not validate arbitrary locations or movement.

| Maximum triangles/group | Groups | Remaining triangles, four levels | Depth scalar reads |
|---|---:|---:|---:|
| Original groups, exact boxes | 11,938 | 1,054,876 | 37,878 |
| 64 | 23,094 | 888,955 | 79,732 |
| 32 | 45,377 | 708,762 | 167,932 |
| 16 | 89,982 | 532,617 | 337,266 |
| 8 | 179,168 | 378,959 | 657,226 |

The eight-triangle model removes 1,049,107/1,428,066 = 73.46% of this primitive
work, while adding many selection queries. It is not a fourfold frame gain.
The smallest group is not yet the native winner. Compare 8/16/32 on actual GPU
execution with the two-phase path; preserve disocclusion, dynamic content,
material coverage and shadow work. Do not disable TAA or the stale-depth guard.

The same input with only box/frustum rejection retains 1,405,196 triangles for
the original groups and 1,378,864/1,360,147/1,346,598/1,336,177 for limits
64/32/16/8. Even the smallest groups remove only 91,889/1,428,066 = 6.43%
without depth, despite increasing the group count from 11,938 to 179,168.
These are Double final-pose counts, not Float32/raster or GPU-time evidence.
The larger reduction above depends on occlusion; finer frustum checks alone
do not justify a new native grouping path.

## Native cost comparison

An Apple A18 Pro prototype on native review basis `d74014221` replays the same
original selected Tokyo inputs from `2dfdaba32`. It measures box selection,
packing, the production prefix scan, a 32-lane-per-group index compactor and
1280×720 D32F position-only raster together. The final full-scene depth pyramid
is supplied without charging its construction. This excludes PBR, first-phase
depth, parent selection, group construction, history recovery and complete frames.

Each configuration has twelve GPU-timestamp samples, including the first.
Variant order alternates forward/reverse; the three measured modes rotate order.
Metal API validation is enabled. Every surviving index matches its original;
there are no false rejections against the Double reference in this pose.
Before culling, every regrouped baseline produces bit-identical depth.

The following uses exact eight-corner projection with two shared reciprocals
per corner. All rows retain the original 1,577 draw batches. Times are medians;
the unculled raster baseline is 2.214–2.220 ms across these configurations.

| Maximum triangles/group | GPU remaining triangles | Select/compact ms | Select/compact/raster ms |
|---|---:|---:|---:|
| Original groups, exact boxes | 1,055,004 | 0.547 | 2.252 |
| 64 | 889,083 | 0.505 | 1.945 |
| 32 | 709,050 | 0.542 | 1.728 |
| 16 | 533,001 | 0.689 | 1.595 |
| 8 | 379,436 | 1.071 | 1.805 |

Sixteen beats eight here: additional query/scan/compaction work outweighs the
smaller triangle count. Its measured saving is (2.214−1.595)/2.214 = 27.96%
of this isolated path, not a full-frame speedup or a universal group size.

A conservative clip-interval projection is cheaper in isolation but retains
more geometry: sixteen-triangle groups keep 644,464 triangles and take 1.764 ms
with selection, compaction and raster. It loses to the exact-corner path overall.

For position-only depth, grouping equal model matrices into bounded runs of at
most 256 groups reduces batch overhead. The eight-triangle configuration then
has 701 batches and takes 1.238 ms against its 2.176 ms unculled baseline;
index and baseline-depth checks remain exact. This does not authorize merging
PBR batches: the 1,577 original placement records have different attribute
offsets/colours despite only two distinct model matrices.

Decision: retain 16 as a candidate with existing batches, compare bounded
compatible batching separately, and reject the cheaper interval projection as
the current winner. No product shader, bindings, TAA guard or LOD changed.
The private native probe retains source, complete compile/link commands,
shader hashes, raw samples and terminal receipts under the system-temp
`outshine-gpu-visibility-894-*` task. Canonical device evidence and integration
coordination belong to [#894](https://github.com/Weichwolf/outshine/issues/894).

## Reproduce

Use an interpreter with NumPy. Keep inputs and outputs in system temp.

```sh
python3 -m unittest discover -s test/experiments/render -p test_native_visibility_cost.py
python3 test/experiments/render/native_visibility_cost.py --input "$INPUT" --output "$OUTPUT"
```

Primary method: Haar/Aaltonen, SIGGRAPH 2015 GPU-driven rendering course,
[two-phase occlusion, pages 51–52](../../../doc/references/geometry/siggraph/2015-gpu-driven-rendering-pipelines.pdf).
The diagram separates old-depth prediction, current-depth construction and
recovery of false negatives. This experiment studies grouping cost, not that
complete runtime integration.

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

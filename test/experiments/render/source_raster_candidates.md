# Original triangle support before native material evaluation

Owner: outshine_research. Parent: #1861, native consumer: #1853.

The first-hit CPU control omits four/five native winning triangle ordinals in
the two fixed ss2 poses. `source_raster_candidates.py` instead selects every
triangle whose outer screen support can reach any viewport pixel center.
It does not decide depth, facing, material, lighting, or a unique winner.
The existing native consumer retains original triangle order, winding and
material parts; its existing referenced-vertex packing preserves attribute bytes.

## Model and boundaries

Input is the whole original Float32 position/index inventory and the actual
CPU-uploaded static `litVertex.glsl` view/placement words. The modeled order is
placement, uploaded shift, view projection, division, then viewport transform.
Each four-term dot product receives a seven-operation FP32 error envelope;
shift, division intervals and viewport rounding are expanded separately.
Usual round-to-nearest FP32 relative-error bounds are assumed; native fast-math
and fixed-point setup are not qualified.
`raster_guard_px` is an additional, explicitly supplied axis-aligned vertex
displacement bound in **2560×1440 raw raster pixels**, not logical pixels.
Zero adds no hardware snapping allowance. These are arithmetic/raster model
assumptions, not a proved Metal/A18 execution or raster precision contract.

For nominal edge e and triangle-wide uncertainty radii rx,ry, require only
the outward half-plane test E >= -(|ex| ry + |ey| rx), together with the
expanded triangle bounding box. This contains the Minkowski sum of the nominal
triangle and its uncertainty rectangle. Both windings survive; numerically
uncertain orientation uses the bounding box. No erosion/occlusion decision.
Bounded chunks visit only pixel centers in those boxes. GPU coverage masks,
winning IDs and query names never enter this geometric selector.

Any possible clip-boundary crossing, nonfinite/subnormal upload operands or
unsupported arithmetic retain the whole original group. Malformed layouts are
rejected. Limits: 262144 vertices, 131072 triangles, 4194304 preflight box pairs,
65536 pairs per chunk. A pair-budget failure retains all original indices before
pair expansion. A guard covering the viewport also keeps the original group
before potentially overflowing dilation arithmetic. Projected output and triangle scratch still span the full source; the pair
chunk is not the total memory bound. Empty selection is valid geometrically;
this nonempty native fixture adapter explicitly rejects it before rendering.
No live revision/lifetime, deformation, MSAA, arbitrary clipping, raster-device,
cache-demand or World admission is provided. Other views require new selection.

## Frozen finite result

Initial whole-vertex measured source: `04227166524f427b0251a97b1137862a282ad200`.
The bounded follow-up below replaces this projection scratch default.
Fixed native packet SHA-256:
`57bd6f0f241ca76d38cd891d7b829fc308aec96c3bc63fb4f8ea2cdd5e826300`.
Full 19 operands /1558 sources /38476 triangles /78374 vertices retained as input.
Both private diagnostic PNGs were opened; they show reference owners and
projected original edges, not a new native/PBR render.

| Additional guard | Near triangles | Side triangles | Two-pose union | Missing native IDs per pose |
| --- | ---: | ---: | ---: | --- |
| 0 px | 550 | 572 | 1107 | 0 /0 |
| 1/256 px | 633 | 713 | 1325 | 0 /0 |
| 1/16 px | 2804 | 3164 | 5710 | 0 /0 |
| 1/2 px | 38476 | 38476 | 38476 | 0 /0 |

Presets are sensitivity controls, not a fit or a certified hardware profile.
Per-pose containment holds independently; no other-pose seeds were borrowed.
First-hit selects 203/202 and misses 4/5; its union is not a native substitute.
Whole-column union and the half-pixel control retain all 38476 triangles.

Linux i5-1135G7, NumPy2.5.4, one BLAS/OMP thread, normal priority; seven timing
rounds, separate Memray1.20.0 allocation trace, load average 3.00/3.83/5.42:

| Query | Wall median | Allocation events | Requested bytes | Scoped peak |
| --- | ---: | ---: | ---: | ---: |
| Near first-hit, prepared columns | 65.325 ms | 260652 | 33023708 B | 1486997 B |
| Near envelope, guard0 | 98.953 ms | 1705 | 122824465 B | 30166760 B |
| Side first-hit, prepared columns | 273.927 ms | 1059624 | 235338349 B | 1667624 B |
| Side envelope, guard0 | 91.610 ms | 1705 | 123426168 B | 30166760 B |

Envelope includes full vertex validation/projection each query: 1398/1969 tested
box pairs, one chunk. It holds no new persistent acceleration structure.
Column preparation separately costs45.266ms/13590479B peak and holds1695340B.
Fixed fixture loading/hashing separately costs1.322s/11088121B scoped peak;
this is not production cache-hit timing. All original geometry is already loaded.
The union's packed native input would be3165 vertices /13284 index bytes /
190524 total array bytes, versus4850656B whole source. These are numeric payload
counts, not measured copies, physical GPU/UMA residency, avoided DB reads or
frame/p99 savings. Memray requested bytes are allocations, not copied bytes.
The lower-quality first-hit control is not a same-quality performance baseline.

## Reproduction and qualification

Use the project `.venv`; existing NumPy, Memray and Matplotlib dependencies.
From a clean measured source, with `PYTHONPATH=lib/src:test/experiments/render`
and `OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1`, run
`python test/experiments/render/compare_source_raster_candidates.py --output /tmp/outshine-raster-<commit>`.
The fresh host-temp directory contains report.json, traces, ordinals and two PNGs.
Focused checks: `python -m unittest test_source_raster_candidates test_source_triangle_materials`.
23 cases passed, including exact rational perturbed-triangle witnesses,
thin/disconnected/overlapping/degenerate support, reversed winding, clipping,
capacity, extreme finite guard fallback and chunk invariance. Ruff/format/diff checks passed.

Existing native comparison gains `--selection raster-envelope --raster-guard-px 0`;
all other native arguments and PBR/derivative processing remain unchanged.
Native HDR/depth comparison remains open at this delivery. Runtime retains its
original group; all reports explicitly keep `qualified_gpu_raster=false`.

## Bounded projection follow-up

Measured source `2bbe7a4b7a6d6d2acd155e45920bc4f1a8829094` releases intermediate
projection arrays per8192 vertices; only the full projected centers/radii stay.
Whole-vertex control uses the same arithmetic function with limit262144.
All four modes have bit-identical center/radius words and selected original IDs
for both poses. Native source inputs are unchanged; existing opened diagnostics
and the original material consumer remain applicable.25 targeted cases passed,
including a bad later block retaining the whole source and three guard controls.

Same Linux host, normal priority, BLAS/OMP1; concurrent builds at nice10 and
another compute process. Load7.59/6.57/5.96. Seven grouped timing rounds and
separate Memray traces; medians in ms, scoped peaks in bytes:

| Vertex limit | Near query | Side query | Near query peak | Side query peak |
| --- | ---: | ---: | ---: | ---: |
| Whole262144 | 116.442 | 119.728 | 30167264 | 30167264 |
| 512 | 177.153 | 178.977 | 8258599 | 8450455 |
| **8192** | **116.231** | **118.055** | **8256751** | **8448607** |
| 32768 | 107.840 | 129.585 | 15163104 | 15163104 |

Projection-only peak30166968/30165792→5724936B; full-query peak falls72.6%/72.0%.
8192 reaches the same full-query memory plateau as512 with much less loop work.
Three interleaved ABBA windows per pose give whole→8192 wall medians
137.435→133.842ms /125.824→124.975ms; this is no general CPU/device speedup claim.
Whole→8192 allocations1708→9904 per query; requested bytes
122824853→147047133 /123426556→147648836B. Lower peak therefore trades more
allocation traffic. The wrapper explicitly copies2507968B projected output per
query; held center/radius output is also2507968B. These are scoped numeric counts,
not all physical copies or a universal memory ceiling for larger inputs.

Reproduce with the same environment using
`python test/experiments/render/source_raster_projection_cost.py --output /tmp/outshine-raster-projection-<commit>`.
The fresh directory contains report.json and16 scoped allocation traces, including
all raw grouped/ABBA timings and output hashes. Projection-only and full-query
measurements overlap and must not be added. No additional source loads, native
passes, GPU ABI, raster assumptions or World/IO/UMA/frame admission are introduced.

## Primary method references

- SIGGRAPH2000 extended projections, local
  `doc/references/geometry/siggraph/2000-conservative-visibility-extended-projections.pdf`,
  section3/Figure5 inspected optically: conservative visibility needs outer
  object support and separately justified occluders.
- [GPU Gems2 chapter42, Conservative Rasterization](https://developer.nvidia.com/gpugems/gpugems2/part-v-image-oriented-computing/chapter-42-conservative-rasterization):
  triangle/pixel Minkowski dilation and expanded-edge/bounding-box construction.
- [Vulkan vertex post-processing](https://docs.vulkan.org/spec/latest/chapters/vertexpostproc.html)
  specifies limited fractional viewport precision through subPixelPrecisionBits.
  It does not establish a Metal/A18 snapping bound for this experiment.
- [Apple Metal rendering guide](https://developer.apple.com/library/archive/documentation/Miscellaneous/Conceptual/MetalProgrammingGuide/Render-Ctx/Render-Ctx.html)
  defines pixel-center and viewport conventions; no A18 raster-grid bound inferred.

Own #1320 acceptance: this is a Python research selector and existing-native
test adapter, with borrowed source lifetime during each call and owned sorted
ordinal output. No generator/World/runtime policy or GPU ABI/BRDF is added.
Pair work is preflighted and bounded; complete projection scratch is measured.
The scalar first-hit and whole-source controls remain for the concrete native
opponent. General device precision, native pictures and World admission remain
separate, so this research delivery does not close #1861 or #1797.

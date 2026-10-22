# Translation overlap in native sun-shadow tiles (#868)

[Input and extraction provenance](../../outshine/data/render/regional-shadow-translation/README.md).
This diagnoses the actual changed-tile work from #15/#866. It implements a small
cost model, not a shadow renderer or reusable GPU algorithm.

## Comparison

```sh
python3 test/experiments/render/test_shadow_translation_cost.py
python3 test/experiments/render/shadow_translation_cost.py \
  --output "${TMPDIR:-/tmp}/outshine-shadow-translation-cost.json"
```

The model requires unchanged linear, depth and homogeneous coefficients and exact
integer translations. Fractional shifts, changed scale/basis/depth and disjoint
windows retain no texels in the estimate. It preserves the new projection and full
tile extent. Shader UV flips Y: texture shifts use `(N/2,-N/2)` times the clip-space
translation change. For each integral shift `(dx,dy)`, geometric overlap is
`max(0,N-|dx|)*max(0,N-|dy|)` texels. Independent brute pixel enumeration checks all
axis signs, diagonals and overlapping/disjoint translations, plus negative controls
for nonintegral motion, projection/depth changes and corrupted native claims.

Native input: 60 frames with a 1/60-s simulation step, 36 updates and 12,119 original
shadow packets. These workload totals are not measured bytes per wall-clock second.
The 2048×2048 regions shift by one pixel per changed axis; all four native world-depth
transforms stay fixed. Both observed packet masks and native matrix-change masks
match. These results are geometric eligibility, not a Float32 raster equivalence proof.

| Approach | Cleared/new texels | Copied overlap payload | Extra texture payload |
|---|---:|---:|---:|
| Current full tile redraw | 150,994,944 | 0 | 0 |
| Shift overlap through scratch | 83,963 | 1,207,287,848 bytes | 16,777,216 bytes |
| Toroidal storage plus exposed strips | 83,963 | 0 | 0 |

Scratch copy counts atlas→scratch and scratch→atlas payloads. Reads/writes, cache
compression, actual bus traffic, driver allocation alignment, resource transitions
and GPU costs are not measured by these numbers. The clear-texel count is not a
physical-memory-write measurement. Ring metadata and shader/caster work also cost
something; zero overlap-copy bytes does not mean zero overhead.

The report leaves incremental draw packets, GPU times and actual GPU allocation
bytes unavailable. New-strip caster selection must retain every intersecting
occluder, including offscreen casters. Copying or wrapping alone does not reduce
vertex work if every original caster is still submitted. No method is admitted as
the native winner by this model.

The model reports its own wall/CPU time, tracemalloc peak and compressed/decoded
input bytes, including instrumentation. These describe the diagnostic process,
not engine costs; allocation counts are unavailable.

## Remaining contract

A ring candidate could use existing unused `ShadowRegionUniform.Depth.zw` for integer
texel origins, preserving the 48-byte layout. This semantic change needs agreement
with master before native integration. Logical bounds/clamping and receiver-plane
PCF offsets must be computed before wrapping physical fetch addresses; crossing a
ring seam must not create a 2047-texel receiver-slope jump or sample another tile.
State must follow accepted submissions; rejected submissions and all content/light/
projection resets must preserve full-redraw correctness. Complete initial filling,
wrap, negative/large motion and dynamic-content invalidation need an independent
full-raster reference and native GPU tests. No stale projection, reduced region,
resolution reduction or delayed refresh is authorized. Current places and API
validation must then be checked on Vulkan and Metal, with actual device costs.

## Primary source

Losasso/Hoppe, *Geometry Clipmaps*, SIGGRAPH 2004, §3/§5 and Figure 2:
`doc/references/terrain/siggraph/2004-geometry-clipmaps.pdf` (text and figure inspected).
Its toroidal incremental-update principle motivates the ring candidate. The terrain
paper does not prove shadow-map/PCF correctness or the GPU performance of that transfer.

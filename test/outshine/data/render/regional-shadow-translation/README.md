# Native regional-shadow translation input (#868)

`frame-inputs.json.gz` contains 61 native world-matrix states: the preceding
submitted state followed by the 60 measured frames. It also contains independently
observed draw-packet tile masks and counts, joined by successful submission serial.
SHA-256: `6f9e652973d39cb1fd87a390e7fe2c8767a8cfa07f3640fe4ae44bcdc81c3591`.
The gzip timestamp is zero. Ordinary tests read this fixed input and never regenerate it.

Product: `37db0b9e38299ba55ed73d758c2b1b4167f5368e`, native Vulkan on
Intel i5-1135G7/Iris Xe, Clang 22/libc++, Linux x86-64 System V ABI.
The capture metadata retains the product binary SHA-256, source blob, declaration
SHA-256 and image SHA-256. A declaration hash does not identify every external asset.
The original prepared source/asset caches remain separate and unchanged.

```sh
build/outshine benchmark --scenario src/assets/places/Flensburg.scenario \
  --frames 60 --linear-velocity 6,0,0 --angular-velocity 0,-360,0 \
  --offline --preload-seconds 60 \
  --output "$scratch/benchmark.json" --image "$scratch/final.png"
```

The benchmark profile is 1280×720 at a target of 60 fps. A GDB entry breakpoint on
`SunShadowStage::BuildRegions` records the actual basis, centre, pre-view translation,
requested world-fit radius and declared radius. A finish breakpoint reads the
actual four completed `Static_` matrices. The native process exits normally with
61 completed captures and zero debugger errors. No inferior functions are called.

Offsets come from Clang's `-fdump-record-layouts` using the complete actual
`SunShadowStage.cpp` compilation-database command, replacing only object emission
with `-fsyntax-only`. This exact build has `sizeof(SunShadowStage)==2080`, alignment 16,
`RadiusM_` offset 168, and `Static_` offset 816 with 512 bytes for four 128-byte matrices.
At exact function entry: `rdi=this`, `rsi=&basis`, `rdx=&centre`, `rcx=&preView`, and
`xmm0` low double is the radius. These are build-specific extraction details;
other binaries/platforms must derive their own layout and calling convention.
Each matrix payload retains a SHA-256 of its native 512 bytes; JSON numbers round-trip
all Float64 values, including signed zero. Matrices are column-major.

The simultaneous explicit observer from #866 supplies packet masks. Its dropped
count is zero, exactly one queue is observed, and all 60 benchmark samples join to
timed, ended submissions. Matrix-change masks match observed packet masks for every
frame. All linear coefficients and world-depth mappings remain exactly unchanged;
all changed XY translations are exactly integral pixel shifts. The complete final
PNG matches the earlier control/observed flight image byte-for-byte.

GDB pauses and observation affect timings; use these inputs for geometric/work
analysis, not native GPU performance claims. All moving frames remain unrefined
under #844. Native Float32 rasterization, PCF, dynamic-content invalidation and
GPU resource execution are not qualified by this matrix capture.

Cost comparison: [shadow_translation_cost.md](../../../../experiments/render/shadow_translation_cost.md).

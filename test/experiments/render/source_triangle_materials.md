# Original triangle material samples

Owner: #1853 / #1797. Native consumer: existing
`MaterialFieldInput::Prepare(Geometry, Extent)` → `MaterialFieldPass` → original
vertex processing and shared `surfaceMaterial.glsl`. No new GPU bindings or BRDF.

`source_triangle_materials.py` selects original triangle indices within their
original parts. The selected input gathers referenced rows without welding;
triangle-corner order and attribute bytes, part slots and material ordinals stay
unchanged. Native vertex packing, normal transformation and flat/smooth facade
processing remain with the existing renderer. This is not a compact far asset
or runtime visibility admission.

```sh
cmake --build "$private_build" --parallel \
  --target test_experiments_render_NativeBuildingImpostors
export PYTHONPATH=lib/src:test/experiments/render
export OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 MTL_DEBUG_LAYER=1
python -B test/experiments/render/native_source_triangle_materials.py \
  --binary "$private_build/test_experiments_render_NativeBuildingImpostors" \
  --selection native-reference --output "$new_system_temp/positive"
python -B test/experiments/render/native_source_triangle_materials.py \
  --binary "$private_build/test_experiments_render_NativeBuildingImpostors" \
  --selection geometric --output "$new_system_temp/geometric"
python -B -m unittest test_source_triangle_materials
```

Freeze the program and its external shader directory throughout the comparison.
Normal execution does not modify shaders, fixtures or pins. Unused integral
readbacks are deleted; raw colour/depth, compact samples, commands and native
reports remain in the requested private output. The geometric command exits one
when the native comparison differs. That is a measured counterexample.

## Bound finite native input

`test/outshine/data/render/building-world-opponents/ss2/` contains the unchanged
whole-source geometry/camera references plus `native-visible-triangles.{json,npz}`.
The supplemental fixture binds both original manifests and every array checksum.
It contains native pixel coordinates, original triangle ordinals, linear HDR and
reverse-Z depth for the two perspective poses. Input is Apple A18 Pro/Metal,
base c2ec71929, 1558 sources / 19 operands / 38476 original triangles.

The diagnostic producer deindexed only its visibility-control copy. Each
triangle's three UV1 values were `(original_part_triangle_ordinal, part_ordinal)`.
The original vertex shader produced identical depth to the untouched original.
The control fragment encoded rounded UV1 as low/high ordinal bytes and part+1
in RGB16F, retaining opaque alpha. All encoded integers are exactly representable.
No fragment `PrimitiveId` capability or backend feature change was needed.
After decoding, the untouched original indices/attributes were used for PBR.
The private control shader was restored before that material comparison.

On this finite native input, 401 selected original triangles reproduce the whole
near/revealing/repeat HDR and depth exactly. `401/38476 ≈ 1.04%` describes only
the selected material render's triangle references: obtaining GPU visibility
itself processed the full source and required readback. It is not a free oracle,
end-to-end speedup or runtime default. The original index-only consumer retained
all 78374 vertex rows; the current selected input compacts referenced rows below.

The existing geometric nearest-hit union also contains 401 triangles but differs
by nine ordinals in each direction. Its native renders lose four/five covered
samples; shared covered samples retain exact material values. Equal cardinality
does not establish matching visibility. The broader capture-column candidate
union retains all 38476 triangles and provides no reduction for both poses.

The supplemental selection is a hardware-bound positive/reference input, not a
portable raster coverage certificate. Other drivers must report actual results.
CPU real-arithmetic rays, native Float32 vertex processing, clipping/fill rules
and derivative footprints remain separate. The original group remains the
fallback for unqualified runtime coverage, pose, lighting or World revisions.
Source-hit raw attributes alone still do not establish native raster admission.

The same supplemental archive now carries the actual upload-boundary words:
`<pose>/view_words` is the 56-word uniform block, `placements/words` the 144-byte
storage rows, both stored as little-endian UInt32 to retain every Float32 bit.
`upload_capture` binds the three query frames, byte layouts, native program,
producer sources and shader inventory. These are CPU bytes supplied to the native
upload, not GPU buffer readback or a guarantee of the driver's arithmetic order.
The instrumented Metal run retains the original HDR and depth at every sample,
including clear pixels, for both poses and the repeated first pose. Diagnostic
hooks stay outside product code; actual binaries, sources, shader sidecars and
readbacks are archived privately. The existing `lens_operands` remain explicitly
recomputed camera operands; the captured Float32 shift is a distinct input.

## Referenced vertices before upload

The comparison defaults to `--vertices referenced` for the selected source.
The whole-original control keeps its original vertex inventory; `--vertices
original` also preserves it for the selected source as an explicit control.
Compaction gathers only referenced vertex rows in their original per-part order
and remaps indices. It preserves all triangle-corner attribute bytes, material
slots and empty parts; it does not weld equal positions or quantize attributes.
Colours and tangents follow the same remap as positions, normals and both UV sets.

On the bound 401-triangle input, 1199 vertices replace 78374. Original selected
arrays occupy `78374*56 + 401*3*4 = 4393756` bytes; referenced arrays occupy
`1199*56 + 401*3*4 = 71956` bytes. These are input-array bytes and native input
vertex counts, not measured physical GPU/UMA peaks or a frame-time saving.
The source CAD geometry and original face selection remain unchanged.
Near/revealing/repeat native HDR/depth remain exact for the reference selection;
the geometric selection still loses the same four/five samples. Vertex packing
does not establish missing raster coverage or runtime World admission.

## Shared World sample composition

`compose_native_source_materials.py --native <native-comparison-directory>
--output <fresh-host-temp>` consumes either comparison's existing `report.json`
and `samples.npz`; it does not rerun the GPU. Whole-source HDR/depth and native
capture, lighting and Lens operands must match the bound World input exactly.
The selected original-material samples enter the existing World compositor with
terrain and the opening opponent, at the common 2560x1440 sample positions.
Depth ordering precedes the existing 2x2 reduction to 1280x720. Limits remain
0.25 linear-HDR RGB and 0.125 visible-source coverage; no runtime admission follows.

The native-reference selection reproduces both unions exactly at both poses and
the repeated first pose. The geometric selection fails all six finite comparisons:
one missing source sample contributes 1/4 coverage error after the 2x2 reduction,
exceeding 0.125. Whole-source fallback restores exact union colour and depth.
Filtering first and globally blending loses the opening's depth correlation even
for the exact positive selection. Original group fallback remains mandatory for
unqualified coverage and unknown World, pose or light revisions.

# Native source-parent component comparison

Owner [#1127](https://github.com/Weichwolf/outshine/issues/1127); existing recipes
and original controls belong to #1167/#1350/#1362. This probe consumes them. It
does not introduce a runtime asset format, new recipe or renderer default.

The original indexed geometry, opaque parents, fractional parents with full
mixtures and the same fractional parents with two fitted lobes share the source
palette, lighting and GGX implementation. The shader includes the product BRDF
and energy tables. Original materials retain their two analytic UV states.
These are the existing steady-repeat material controls and geometric normals,
not a captured application PBR/interpolated-normal/texture replay.

Each parent group renders its first hit into a separate atlas region. Original
remainder geometry has an opaque region. A compute pass sorts actual group depths
per sample and composes premultiplied radiance/transmittance. Raster, material
reads, all group shading, atlas traffic and composition are paid. No free fine
depth, source visibility, opaque coverage or hidden-background oracle is supplied.
The fixed component supports at most sixteen layers and nine palette entries.

The diagnostic expands indexed source positions into ordered triangle corners.
A flat vertex output carries the global triangle ordinal to the fragment stage;
it does not require Vulkan's optional Geometry capability for gl_PrimitiveID.
Triangle order, layer ranges, source positions and face records remain unchanged.
The report separates original position/index bytes from expanded raster position
bytes; buffer_bytes counts the actual uploaded buffers without an index buffer.
Corner expansion and upload are preparation outside the twenty-four shading
rounds; their CPU time is not included in those encode/GPU samples.
This diagnostic representation is not a proposed runtime geometry storage price.

The crop shares the research replay's integer/clipped ROI and preserves the
original 1280×720 presentation pixel spacing. It measures
a small isolated component, not a complete frame or a supersampled quality gate.
It includes neither terrain/environment/shadows/IBL nor parent production,
upload/selection, disocclusion recovery or world integration. All available
original remainders are paid; failed sources remain required and unpriced.
Diagnostic face records are not proposed runtime storage prices.

Use Python>=3.11, NumPy, Shapely, SciPy, Pillow and trimesh as required by the
existing reference modules. Build only the explicit CMake probe target:

```sh
nice -n 10 cmake --build build \
  --target test_experiments_render_NativeBuildingParents --parallel
export PYTHONPATH=lib/src:test/experiments/render:test/experiments/buildings
python -B -m unittest test_native_building_parent_inputs
python -B test/experiments/render/build_building_parent_shading.py \
  --input test/outshine/data/render/building-full-view/original-inputs.tar.gz \
  --seed-cell 33 --cell-count 4 --output <fresh-system-temp-producer>
python -B test/experiments/render/native_building_parents.py \
  --parents <producer>/parents --fits <producer>/fits/2 \
  --binary build/test_experiments_render_NativeBuildingParents \
  --shaders build/test-shaders/native_building_parents \
  --output <fresh-system-temp-cost>
```

Run separately with `--audit` and another fresh output. It enables native
validation and an extra primitive/UV/state/depth MRT. On Metal the runner also
sets `MTL_DEBUG_LAYER=1`. Audit costs are distinct from normal measurements.
Twenty-four rounds alternate original/candidate ordering; first samples count.
GPU submission timestamps and CPU encoding remain separate; no CPU fence time
is reported as GPU time. The target is excluded from routine tests/gates.

The independent Double point-sampled reference reports C/T errors. Primitive
readbacks additionally compare GGX for actual native first-hit faces. GPU raster
boundary choices may differ from Double rasterization; those differences remain
reported, not silently converted into a reference pass. A completed diagnostic
is not an image/LOD/world admission. Actual costs, opened images, boundary
counterexamples and device decisions belong in #1127, not this reproduction guide.

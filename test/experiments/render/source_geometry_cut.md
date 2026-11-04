# Intrinsic geometry cut

Reference for #1955, alongside `source_product_cut.py`. Products own immutable
source intervals; their logical address excludes eye, focal length and lookahead.
The geometry itself is camera independent. Current projection selects an existing
product; changing pose may select another product without rebaking the old one.
Unknown sources remain explicit. Geometry support comes from the face inventory,
independently of whether the material-field producer supports that source.

Three geometric products: original source geometry, shells with declared world
Hausdorff error, and bounding-box hulls. A hull and every represented source lie
inside the same declared box; its diagonal bounds their world-space Hausdorff
distance. Hulls are **render proxies**, separate from precise functional assets.
A shell group contains every child's shell; its world error is the maximum child
error, never an average. Grouping shells reduces products and selection work;
it does **not** reduce source geometry or prove that loading it is cheap.

For a fixed orthonormal camera basis and eye displacement of norm at most `r`,
transform the source box to camera-coordinate intervals and expand each by `r`.
Let `z0 > 0` be the minimum depth and `Mx/My` the maximum absolute lateral values.
For corresponding points separated by world distance at most `e < z0`, perspective
L1 error is at most `F e (sqrt(2) + (Mx + My)/z0) / (z0 - e)`.
For hull points both inside the same box, camera translation cancels in their
coordinate differences. With fixed-box camera spans `Dx/Dy/Dz`, the tighter bound
is `F (Dx + Dy + (Mx + My) Dz/z0) / z0`. Do not add eye displacement to these spans.
Orthographic bounds are respectively `sqrt(2) F e` and `F (Dx + Dy)`, where `F`
is pixels per metre. A perspective interval crossing `z=0` cannot qualify an
approximate product; native frustum clipping remains a separate requirement.
Changing orientation requires reevaluating selection, not changing static geometry.

These are **real-arithmetic reference bounds over declared inputs**. Native mesh
containment, shell error, float/GPU transforms, materials, coverage, shadows and
runtime handoff remain separate acceptance requirements. This does not raise the
old CPU capture's `EyeRadiusM=0` or lower the native LOD error threshold.

## Reproduce

Build `source_product_inputs.cpp` using the compiler/flags in
[source_product_cut.md](source_product_cut.md). It additionally writes `.geometry`:
a versioned little-endian header, immutable revision, and `(shell_error_m, faces)`
for every original ordinal; `-1` means unknown error. Export reads headers, no forms.

```sh
/tmp/outshine-source-product-inputs build/assets/assets.sqlite "$BASE_KEY" /tmp/source
PYTHONPATH=test/experiments/render python3 -m unittest -q test_source_geometry_cut
python3 test/experiments/render/compare_source_geometry_cut.py \
  --input-prefix /tmp/source --source-revision "$BASE_KEY" \
  --eye "$ECEF_X" "$ECEF_Y" "$ECEF_Z" --focal-px 1000 --error-px 1 \
  --lookahead-m 0 100 1000 --output /tmp/outshine-geometry-study
```

Default orientation points at this source parent's centre, not a real Place camera.
An explicit `--target` changes it; `--orthographic-px-per-m` selects a parallel view.
Reports identify input/code hashes and count sources as well as products. Reusing
logical groups is not a native cache hit, full-world performance or memory proof.
Native integration and current device evidence belong exclusively to #1955.


## Native candidate selection

`SourceProductSelection::SelectGeometry` implements this candidate cut. Native
source-group metadata stores face support and optional declared shell error per
original ordinal. Its versioned hit reads neither prepared headers nor forms.
Unknown errors/sources stay explicit; this does not certify generated products.

```sh
cmake --build build --target outshine_experiment_source_geometry --parallel
python3 test/experiments/render/compare_source_geometry_cut.py \
  --input-prefix /tmp/source --source-revision "$BASE_KEY" \
  --eye "$ECEF_X" "$ECEF_Y" "$ECEF_Z" --focal-px 1000 --error-px 1 \
  --lookahead-m 0 100 1000 --output /tmp/outshine-native-geometry-study \
  --native-control build/outshine_experiment_source_geometry \
  --asset-db build/assets/assets.sqlite
```

The optional native comparison uses the default root-centred perspective camera.
It checks source partitions, product kinds, declared errors and visited-node counts
against Python. `metadata_ms` measures catalog lookup/decode; `selection_with_setup_ms`
includes native aggregate-table construction and selection. First catalog creation
may read source headers; repeat in a fresh process to verify header/form/mesh bytes
are zero. World integration, native geometric qualification and GPU cost remain
separate acceptance requirements in #1955; this tool does not grant LOD authority.

## Whole-world control

`static_world_geometry.patch` is an experimental change for a detached checkout:
turn off only surface projection, isolate geometry keys/kinds, keep all scenario
parameters. This retains the legacy adaptive mesh's 64m selection domain; it is
not an intrinsic shell product and its cache key still includes the eye.
Build normally, then run the ordinary `places.py`/motion benchmark with the same
source and asset stores. Shader artifacts must accompany the executable. Keep
experimental images/reports in system temp; do not promote them to Places.
`building-lod-static-probe` identifies the temporary products for targeted cleanup.

A18 results and the decision belong in #1955. The control tests actual GPU cost;
triangle counts alone do not establish a native performance result.

## Native mesh-cache reuse

The field producer also consumes camera-independent `GeometryAsset` products.
Their fixed descriptor is `8+(6+16)*8=184` bytes: version, local bounds and world
transform. Mesh/material streams use `GeometryProductCodec` in a separate package,
so metadata queries do not decode meshes. Missing meshes remain misses; a retained
descriptor may still reconstruct view metadata. No descriptor certifies LOD error.

```sh
cmake --build build --target outshine_experiment_building_geometry_cache --parallel
/usr/bin/time -l build/outshine_experiment_building_geometry_cache \
  build/assets/assets.sqlite "$BASE_KEY" /tmp/outshine-mesh-products.sqlite \
  "$GEOMETRY_BYTE_LIMIT" "$SOURCE_ORDINAL" "$ANOTHER_SOURCE_ORDINAL"
```

Use ordinals from one immutable parent. The byte limit bounds encoded geometry;
it does not prove total producer/GPU residency. The first capture publishes a mesh,
then a different direction/resolution renders with no source owner. The geometry
address retains source revision/set, capture frame and materials, excluding view
catalogue/resolution. Existing field addresses keep their previous byte sequence.
Timings include metadata, mesh work and GPU capture; first-use device/shader startup
prevents attributing their whole difference to caching. Source IO counters and
process RSS are separate evidence. Normal World fields use this path on misses;
selection before the legacy world's initial geometry remains work in #1955.

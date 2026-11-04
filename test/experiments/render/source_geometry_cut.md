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

# Source-product cut

Metadata-only reference for #1955. One cached source parent is an input, not a
complete Place. The native exporter loads prepared headers and the stored
`PageIndexAsset`; an index miss may create that small metadata product. It opens
no fine form blocks. `*.supported` records support for the native field producer,
not permission to discard other source objects.

`source_product_cut.py` chooses complete source intervals as field candidates,
geometry leaves or explicit unresolved leaves. Cohort tags are opaque; current
building-cell tags are one input. The selector does not impose a 64-cell World
interface. A cut owns every original ordinal exactly once, in canonical order.

For a centred perspective camera, capture half-extent `h`, distance `d > 2h`
and focal length `F`, flattening `|u|,|v| <= h`, `|depth| <= 2h` has real-arithmetic
L1 projection error at most `4 F h² / (d (d - 2h))`. The tests include the tight
capture corners. This is **not** the native interval bound for arbitrary camera
poses, GPU rounding, visibility, silhouettes, materials or shadow replacement.
Native `ImpostorPoseBounds` and actual products must still qualify those demands.

`publish` models coverage handoff with explicit current-parent validity. Partial
children retain a valid parent; expired parents cannot conceal missing children.
Stale source revisions, different representations and unresolved readiness are
rejected. The current single-eye CPU bake is not thereby made reusable.

The reports count source-group reuse, not view/material cache hits. Logical PBR
payload is three RGBA8 images with complete power-of-two mip chains:
`12 (4 P² - 1) / 3 = 16 P² - 4` bytes per field. Alignment, archives, metadata,
staging and other device resources are excluded. There is no frustum/occlusion
selection here; dense cities can still request excessive hidden field payloads.

## Reproduce

Build the metadata exporter through its CMake target:

```sh
cmake --build build --target outshine_experiment_source_inputs --parallel
PYTHONPATH=test/experiments/render python3 -m unittest -q test_source_product_cut
python3 test/experiments/render/compare_source_product_cut.py \
  --exporter build/outshine_experiment_source_inputs \
  --database build/assets/assets.sqlite --base-key "$BASE_KEY" \
  --eye "$ECEF_X" "$ECEF_Y" "$ECEF_Z" --focal-px 1000 \
  --output /tmp/outshine-source-product-study
```

Use a fresh system-temp output directory. The report records source/code hashes,
native metadata IO, candidate costs and ECEF-X translations from 0 to 600 m.
Timings are scoped Python observations, not native frame/IO/peak-memory budgets.
Runtime integration, decisions, measured examples and open contracts belong to
[#1955](https://github.com/Weichwolf/outshine/issues/1955).

# Native box field: complete PBR comparison

Owner: [#1127](https://github.com/Weichwolf/outshine/issues/1127).
Companion: [query control](../box_field.md).

## Scope and implementation

- `pbr_dda.patch` applies to archived mask-control commit `08a51d354` only.
  It is an isolated experiment, not a product merge or a new runtime dependency.
- Same complete input, camera orbit, registered metallic-roughness material,
  lighting, floor, shadow atlas, temporal/output passes and binary for both modes.
  Unchanged box PNG/D32 controls match the earlier archived opponent exactly.
- One conservative24-vertex/36-index box replaces the mask planes. A regular
  XY DDA intersects occupied prisms from the same R32Float height image.
  Three-dimensional clipping precedes traversal; at most columns+rows+2 steps.
- Inverse clip transforms generate near/finite-or-infinite far ray endpoints.
  Nonperspective interpolation supports the perspective view and orthographic
  sun projector. Back-face proxy rasterization covers rays originating inside
  the proxy. Actual hit position, normal, depth, light coordinates and current/
  previous clips replace proxy attributes before native PBR and velocity output.
- Existing view and placement layouts are also bound as fragment uniforms;
  their existing CPU/GPU size/offset assertions remain applicable. No asset API
  or cached input format change. This experiment stores two extra CPU bindings.
- Receiver-plane slopes come from the hit plane transformed into light space.
  Cross-pixel derivatives mix separate buildings and produce incorrect shadows.
  For light-space plane normal n and shadow UV=(x/2,-y/2), slope=(-2nx,2ny)/nz.
- Caster intersections use the entry boundary matching the actual box atlas.
  `Cull::Front` alone does not imply an exit hit: the existing sun projection
  has different winding from the colour view. Exit queries produced systematic
  atlas depth errors. Atlas readback is checked independently of final colour.

## Reproduction

Prepare a separate worktree and an existing pinned shader compiler. Build and
database generation are sequential; do not share a build with another process.

```sh
prototype=$(mktemp -d "${TMPDIR:-/tmp}/outshine-box-pbr-XXXXXX")
git worktree add --detach "$prototype/checkout" 08a51d354
git -C "$prototype/checkout" apply --unidiff-zero --intent-to-add "$PWD/test/experiments/render/urban_fields/pbr_dda.patch"
make -C "$prototype/checkout" all JOBS=2 GLSLANG="$GLSLANG"
make -C "$prototype/checkout" db JOBS=2 GLSLANG="$GLSLANG"
python test/experiments/render/urban_fields/compare_pbr.py \
  --native "$prototype/checkout" --input complete.field --output "$prototype/normal"
```

Repeat with `--validation` and a fresh output directory for functional validation;
exclude instrumented timings from performance comparisons. Input text starts with
columns, rows, pitch metres, fill fraction, then row-major Float32 heights. It can
be exported from `bench_box_field.load(product.npz)` without geometry generation.
The complete current opponent has624 rows,616 columns and247731 nonzero heights.

The runner uses the archived native harness with its full CDB flags. Each view
executes three settling frames then60 measured frames over360 degrees. Cold and
source-free warm processes must preserve packages, PNG, D32 and shadow bytes.
Four camera cases separate distance and altitude; reports include actual device,
requested field allocations, GPU/host quantiles and image/depth/atlas differences.
PNG/D32/atlas readbacks occur after measurements. Keep results in system temp.

The three settling frames also retain separate GPU/host samples, including the
initial atlas build. They do not enter steady-state quantiles. Do not add host
submission time to asynchronous GPU time. `.shadow-frame` reconstructs the packed
native sun projection from its actual caster centre and existing raster packing;
this harness has an identity scene placement whose centre equals its anchor.
It is not a GPU-uniform readback or a general arbitrary-scene projection exporter.

`shadow_oracle.py` checks selected atlas disagreements against every prism and
the floor, independently of DDA/grid pruning. It models both actual Float32 box
placements and field bounds. Returned-point/prism distance and first-hit edge
distance distinguish boundary-sensitive cases; neither is a certified SSE bound.
Its intentionally quadratic all-pairs work is reference code, never a runtime path.
Small mixed/empty fields and parallel rays compare against independent triangles
in `test_shadow_oracle.py`. To inspect all outliers of the current field:

```sh
python test/experiments/render/urban_fields/shadow_oracle.py \
  --input complete.field --boxes "$prototype/normal/far-low-boxes" \
  --field "$prototype/normal/far-low-field" --rays 1024 \
  --output "$prototype/shadow-oracle.json"
```

## Decision boundaries

The qualified experiment selects DDA for the next native field implementation;
concrete measurements, source hashes and remaining differences belong to #1127.
Matching primary coverage does not prove identical shading or shadow depth.
Raster boundary rules, Float32 arithmetic and subpixel temporal filtering remain
separate concerns; no bit-identical rendering or derived screen-error bound claimed.
The fixed-light atlas is rendered during settling and reused during measurements.
The initial frame is timed; its caster pass is not isolated. Model base is0 with parallel upright
prisms; terrain, curvature, filtered parents, moving light, native World integration,
Iris PBR execution and complete-world p99 remain open. Places are unchanged.

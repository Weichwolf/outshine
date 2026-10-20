# Native visibility pipeline replay

Owner: [#894](https://github.com/Weichwolf/outshine/issues/894).
Method and earlier comparisons: [native visibility cost](native_visibility_cost.md).

The same C++/GLSL replay runs through the native Metal or Vulkan backend. It
measures selection, packing, production prefix scan, index compaction and
1280×720 D32F position-only raster. No product pipeline is replaced.

## Reproduce

Use Python with NumPy, a built native checkout with its current
`compile_commands.json` and `build/liboutshine.a`, glslangValidator, and SDL3
development packages. Root master still has the older renderer; `--checkout`
must point to the native integration build. Its calibration API must match
the replay; the initial Metal run uses native commit `e2ae8098599d`.

```sh
python3 -B test/experiments/render/native_visibility_pipeline.py \
  --checkout /path/to/built/native-checkout \
  --output "${TMPDIR:-/tmp}/outshine-visibility-device-run"
```

Output must be a new system-temp directory outside both checkouts. The starter
uses the complete native `GpuCompute.cpp` compilation entry and canonical native
link flags. It builds only this probe and its shaders; it does not rebuild the
engine. Metal API validation defaults on. `--device-name` can require an exact
reported device. Validation, compilation and runtime logs remain in the output.
`receipt.json` records terminal status, native revision/changes, source/shader/
binary hashes, input hashes, commands and timings. Failed runs remain failed.
The emitted compilation database also supports focused clang-tidy:

```sh
clang-tidy -p "$OUTPUT" test/experiments/render/native_visibility_pipeline.cpp
python3 -B -m unittest discover -s test/experiments/render -p 'test_native_visibility*.py'
```

## Fixed input and boundaries

`test/outshine/data/render/visibility/selected-native.tar.xz` is 15,012,884 bytes
and expands to 325,484,909 bytes. It preserves all 4,284,198 consumed index words
and positions addressed by 2,174,806 vertex IDs from 11,938 selected original
jobs of one captured view on `2dfdaba32`. Only unread position/index slots are
zeroed for standard XZ compression. Buffer lengths and byte addresses remain
unchanged. Original and canonical file hashes are in the adjacent manifest.
The archive contains OSM-derived procedural geometry with attribution there.

This is a fixed selected-input fixture. It cannot select previously unselected
jobs or represent another pose. Its sparse compression is not runtime memory
savings: the original position/index allocation sizes remain intact. Extraction
checks archive and member hashes, sizes, complete membership and regular files.

Each grouping retains all 1,428,066 original triangles before culling. Original
groups and limits 64/32/16/8 are compared with original draw batches (`source`)
and bounded runs of at most 256 equal-transform groups (`block`). The latter
is valid only for position-only depth: different PBR attributes/materials must
not be combined merely because model matrices match.

The compactor uses 128 threads with four independent 32-lane jobs per workgroup,
not one 32-thread workgroup per job. Variant order alternates and mode order
rotates over twelve rounds; first samples count. Timings are submission GPU
timestamps, not CPU fence-wait durations. Missing timestamps fail verification.

Every retained group must preserve its original complete index range and order.
All regrouped unculled baselines must produce identical depth. False rejections
against the Double reference fail. Conservative extra survivors are reported.
The culled standalone depth need not equal the unculled depth: supplied
full-scene depth includes other geometry not drawn by this position-only pass.

Final full-scene depth is supplied free. Group construction, parent selection,
first-phase depth/pyramid construction, disocclusion recovery, PBR, movement and
complete frames remain outside the measurement. The replay does not establish
the two-phase algorithm or the 720p60 world budget. Iris results remain separate.

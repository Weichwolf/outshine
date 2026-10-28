# Native building impostors

Owner/acceptance: #1570/#1127. Original controls: `native_building_cell_controls.md`.
This diagnostic uses fixed source geometry, capture directions, lighting and queries.
It does not admit a World LOD, source occluder, smaller error bound or frame budget.
Unsupported original sources retain the original manifest's unresolved status.

```sh
cmake --build "$build" --target test_experiments_render_NativeBuildingImpostors --parallel
"$build/test_experiments_render_NativeBuildingImpostors" "$input"
"$build/test_experiments_render_NativeBuildingImpostors" "$input" --cached-world
```

The ordinary mode reads `input.json` and the exported part arrays, captures `atlas.bin`,
and writes `result.json` plus fixed original/card controls. Cached-world mode needs
`input.json` with `atlas_sha256`, the SHA-256 of those exact encoded atlas bytes.
On a miss it verifies/decodes `atlas.bin` once and creates only requested view/mip
products in the existing AssetCache, under `$input/world-view-assets.sqlite`.
It does not reconstruct original meshes or invoke another GPU capture.

The metadata index contains fixed capture directions and source extent. Selection
uses eye direction and the projected bounding sphere before product IO. Stored keys
contain source/capture/policy identity, never a query name or camera position.
Each loaded product passes through ImpostorInstances and WorldCandidate publication,
then the normal native scene pipeline. Outputs: `world-result.json` and
`<query>-cached-world.{linear,depth,rgba,ss16}`. Repeated runs replace those diagnostics.

For a fresh-process hit check, move `atlas.bin` aside after the first successful run
and repeat cached-world mode. Every query must be a hit, with zero source bytes,
decodes and writes, and unchanged output hashes. Restore/protect the original input.
Database open, metadata, source read/decode and per-product load times are separate;
metadata time includes any source import. Never sum overlapping intervals.
The 24 GPU submission samples and 16 physical microcamera samples are component
measurements; they do not replace Places' rotation/p99 protocol. Open actual crops,
compare against original SS16 controls, and retain visible error/cost limitations.

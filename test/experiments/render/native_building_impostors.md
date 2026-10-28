# Native building impostors

Owner/acceptance: #1570/#1127. Original controls: `native_building_cell_controls.md`.
This diagnostic uses fixed source geometry, capture directions, lighting and queries.
It does not admit a World LOD, source occluder, smaller error bound or frame budget.
Unsupported original sources retain the original manifest's unresolved status.

```sh
cmake --build "$build" --target test_experiments_render_NativeBuildingImpostors --parallel
"$build/test_experiments_render_NativeBuildingImpostors" "$input"
"$build/test_experiments_render_NativeBuildingImpostors" "$input" --cached-world
"$build/test_experiments_render_NativeBuildingImpostors" "$source_planes" --source-mesh
```

The ordinary mode reads `input.json` and the exported part arrays, captures `atlas.bin`,
and writes `result.json` plus fixed original/card controls. Cached-world mode needs
`input.json` with `atlas_sha256`, the SHA-256 of those exact encoded atlas bytes.
On a miss it verifies/decodes `atlas.bin` once and creates only requested view/mip
products in the existing AssetCache, under `$input/world-view-assets.sqlite`.
`atlas_provenance` defaults to `native-original-impostor-1`; source-plane captures
declare `native-source-planes-impostor-1` explicitly. Provenance and content digest
both enter the metadata namespace; old products cannot be reinterpreted.
It does not reconstruct original meshes or invoke another GPU capture.
The shared `ImpostorViewAssets::Resolve` validates a cached product before calling
its selected-view factory. A failed factory publishes no partial product; a fresh
hit needs no factory. Admission/coalescing remain with the existing content jobs.

An alternative `source_request` object (the fixed `source-plane-request.json`)
and `source_input` path replace atlas import. On a metadata miss, the common
`BuildingSourceCapture` owner opens only source headers and plans the shared
projection. Product misses trace/capture only the selected view; each capture
call owns and retires its device on the executing worker; CPU inputs and the
most recent single-view result can serve later requested mips.
No complete atlas is created or required. Recipe/source/group/profile identity
names products; cells are canonicalized, while work limits, diagnostic flags,
query names and camera demand do not enter that identity. Changing actual source
or capture inputs cannot reinterpret stored products.

```sh
python test/experiments/render/replay_source_view_misses.py \
  --client "$build/test_experiments_render_NativeBuildingImpostors" \
  --source-input "$original/input.json" --output "$new_system_temp_directory"
```

The versioned eight `source-world-controls.json` queries preserve the original
cameras/lights. The cold control produces two views/32,768 rays with two job-local capture
contexts and four packages. A fresh process uses an absent source path, zero work
budget and reversed cell order; all 32 raw images and asset keys remain identical,
with zero source bytes/decodes/captures/writes. Removing a required child makes
the absent source a failure without changing the remaining assets. Nonempty
finite colour is checked; this is cache/production/WorldCandidate evidence, not
original-image quality or whole-World admission. Selected products now use the
existing AssetPipeline/Tasks with owned store lifetime and stale-key/cancellation
checks. The driver waits during offline setup; it introduces no frame-loop wait.
Its queue declaration covers retained request bookkeeping, not measured total
source/GPU memory. Generic World integration, source-work costs and the known
soft/energy-biased images remain open.

Source-mesh mode consumes the indexed geometry exported by
`native_building_source_planes.md`, choosing the same nearest catalogue direction
for each unchanged query. A part's optional `material` role defaults to its array
index, preserving the original three-part input. It compares actual source-patch
geometry with cards; no source mesher, silhouette fitting or World LOD admission.

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

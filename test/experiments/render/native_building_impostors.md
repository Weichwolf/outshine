# Native building impostors

Owner/acceptance: #1570/#1127. Original controls: `native_building_cell_controls.md`.
This diagnostic uses fixed source geometry, capture directions, lighting and queries.
It does not admit a World LOD, source occluder, smaller error bound or frame budget.
Unsupported original sources retain the original manifest's unresolved status.

The original-capture mode accepts `capture_pixels` (default128) and optional
`capture_half_extent_m`. The view count follows `catalogue`; source geometry and
the Baker's extent validation stay unchanged. Keep centre, extent, rays and clips
fixed when comparing capture densities (#1756).

Every base draw writes `.linear`, `.depth`, `.normal` and `.rgba`. The `.normal`
file is the unchanged shading-normal attachment: 1280×720 row-major Float32×4,
native byte order (little-endian on both target devices). XYZ is the scene-space
shading normal after model/facing/material-normal transforms; W preserves the
front/back sign written by the shader (+1/-1), including attachment clear/resolve.
No normalization or camera/ECEF conversion is applied. Pair it with the same
`.depth` field; the existing aggregate excludes depth zero. It belongs to the
base frame, not the
supersampled colour `.ssN`. `normal_sum` retains the depth-masked XYZ aggregate.
Replay compression stores these outputs losslessly as `.normal.gz`.

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

`Generators::BuildingSourceView` owns the native source selection and plane-to-mesh
conversion. It accepts a retained PreparedStructureTile (including lazy shape blocks),
selected cells and an explicit frame in the tile-relative source coordinates. Open
uses headers only, preserves supported and unresolved original ordinals and builds
the existing capture hierarchy. Capture has sample/source-test/stop limits, transforms
into the declared local frame and accepts wall/roof/metre materials from its caller.
Errors preserve kind and original source ordinal; sampling/conversion costs are separate.
It owns no JSON, file access, provider, cache or GPU object. The diagnostic adapter
only reads fixed archives and submits the resulting native geometry to its job-local
baker. Runtime registration from PreparedBuildingAssets and World revision/publication
remain open. Raw-product identity must bind source/capture/material inputs; a metadata
hit must not reopen the source forms. This introduces no World error or reuse bound.

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

For #1752, `--source-mesh` also accepts input JSON `stored_samples: true`,
`atlas_file`, `atlas_sha256`, and optional `atlas_provenance` (default
`native-original-impostor-1`). It draws diagnostic `Test::StoredSampleQuads` through
the same native geometry/PBR/depth path. This branch needs no original geometry
directory; the independent reference still loads its complete original meshes.
The diagnostic uses no production renderer switch or additional asset format.
An optional query `capture` uses the existing explicit camera description, including
orthographic rays and clipping, for a same-ray control against perspective changes.

`export_building_depth_samples.py --original "$original" --samples "$samples"
--native-commit "$product_commit" --output "$output"` exports matching source-mesh
runs. It retains base colour/depth/normal and the actual microcamera average,
rejects crops that remove content, and binds full readback/geometry/driver hashes.
Both directories require the actual successful `qualified-command.json` run receipt
with `exit`, `binary_sha256` and `driver_sha256`; exporter-time source state is not
substituted for the program that produced the images.
The small shared native counterexample is `test/outshine/data/render/building-depth-samples/`.

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

## Lighting before spatial filtering

`--radiance-world` accepts the same `source_input` / `source_request` inputs as
cached-world mode. It explicitly compares a different order: render the selected
full-resolution material field with the current light, filter premultiplied linear
radiance/coverage to the selected screen support, then publish the unlit optical
card through the existing WorldCandidate. It uses nearest reconstruction of the
selected level, not another material/normal average. Nothing switches the general
World path or reinterprets existing reduced card products. The selected unlit view
is stored separately as `impostor-material-view-1` under the same metadata index.
Its key binds capture/source identity and catalogue direction, independently of
light, query name and selected screen mip. The native atlas codec preserves coupled
normal/material samples, depth and empty coverage at the full capture resolution;
reducing those samples before lighting would lose their joint response.
Shading uses the typed isolated material-field pass on the existing render device;
it does not open a RuntimeScene, replace the main world/history or read pixels in
product code. The diagnostic retains one CPU atlas and one resident GPU source.
Light, eye, focal and snapshot changes reuse that source; selecting another source
field replaces it. The ordinary WorldCandidate below presents the finished card.
The source upload and combined relight/filter submission have separate GPU timings.
`field_filter_gpu_ms` is null for this mode because the filter shares its submission
with shading. Configure, CPU preparation, upload/encode calls, explicit offline
waiting and optional source readback are reported separately; overlapping intervals
must not be summed as frame time. The driver alone waits and reads diagnostic pixels.

Selection precedes payload IO; only a missing valid field invokes source capture.
The byte limit is checked before serialization allocation and payload decode; it is
not a total producer working-memory reservation. A fresh validated hit needs neither
source headers nor source geometry. One call shares a renderer for field shading
and presentation. `RadianceFilter` reduces the scene-linear GPU image directly;
`RadianceImage` retains its producing device, texture, sampler and submission.
Render material registrations retain that lease across candidate/plan replacement.
Native source materials and the GPU material layout stay unchanged; the prepared
unlit binding selects premultiplied-area reconstruction. Lit/opaque consumers are
rejected. The support quad creates no CPU images or unused moment fields.
GPU objects stay in the executing renderer/thread, never across non-affine source
jobs. The general World path still needs its asynchronous producer/LOD admission.

```sh
python test/experiments/render/replay_cached_radiance.py \
  --client "$build/test_experiments_render_NativeBuildingImpostors" \
  --source-input "$source/input.json" --output "$new_system_temp_directory"
```

Surface cards and raw material views use typed products of the same
`ImpostorViewJob` in the existing AssetPipeline. Both validate the requested key
against current metadata before work, retain their asset store/source owner and
return a stored CPU-product receipt. Cancellation is checked before generation
and before publication. A source capture owns its GPU resources inside its worker
call; no renderer or device crosses Tasks. The diagnostic waits only during offline
setup and then presents on its executing renderer. Shared consumers, last-consumer
cancellation, admission rejection, source-free hits and stale metadata are checked
for both product types. This is not yet the general World demand producer.

`BuildingImpostorSource` is the native PreparedBuildingAssets producer for complete
opaque source groups. Its canonical recipe key binds the base content key, selected
cells, intrinsic source frame, capture catalogue/raster and serialized materials.
Camera, light, caller name and work budgets are not source identity. Metadata hits
need no building store; field hits never invoke the producer. Only misses load the
prepared native index/forms. Sample/encoded-byte admission precedes descriptor IO.
A group with unresolved originals publishes no complete-source metadata and retains
its original rendering path. This is source-set completeness, not a filtered image,
LOD, visibility or parent-replacement proof. CPU source state may serve capture jobs;
each GPU baker lives only inside its executing call. World registration and current
revision/coverage publication remain open.

The atomic `NativeSourcesStayClosedOnFieldHits` case builds native game assets,
produces a selected field through AssetPipeline and loads its CPU lease; a new
producer without a source store replays the cached field. An optional explicit
prepared-index filename checks real unresolved inputs without loading form packages.
Normal test runs use only their self-contained fixture.

`ImpostorFieldLoad` bridges a completed asset receipt to a decoded CPU field.
Start/Poll perform no payload IO or decode; the existing Tasks worker reads and
validates the current metadata/key and coupled field. Pending returns no field,
errors/cancelled generations never become ready. A CPU alias retains the publication
ticket until its final consumer retires; dropping a pending loader cancels without
waiting and the detached worker owns its remaining state. This stages CPU data;
it does not choose World replacements, admit an LOD or create GPU resources.
The byte cap bounds encoded reads, not total decoded/source/GPU working memory.
The offline diagnostic waits for readiness before presentation; a runtime consumer
must poll, retain valid parents and validate its current World revision at publication.

The eight fixed controls generate only two raw views. A fresh process with an absent
source path, zero source-work budget, reversed cell order and renamed queries relights
the stored fields: all 40 raw outputs and view keys must match, with no source work
or field writes. Removing a required field must fail without changing remaining
assets. The driver checks finite positive radiance and losslessly compresses outputs
with SHA receipts. This proves the selected native cache/relighting consumer, not
asynchronous World production, cancellation, angular/LOD quality or frame budgets.

Reproduce with the existing prepared original-part arrays and fixed source archive:

```sh
python test/experiments/render/replay_building_radiance.py \
  --client "$build/test_experiments_render_NativeBuildingImpostors" \
  --source-input "$source/input.json" --original-input "$original/input.json" \
  --field-control --output "$new_system_temp_directory"
```

The driver binds the fixed queries/recipe and all supplied original arrays by SHA,
checks finite positive outputs, reports coverage/RGB errors and compresses raw
artifacts losslessly after reading them. `--query` selects an existing control;
`--filter-axis-samples` requests a denser finite comparison. It creates no oracle
or pin and makes no new admission decision. Prepare the source from the versioned
`original-inputs.tar.gz`; original part preparation remains in
`native_building_cell_controls.md`. No API data is acquired by this driver.

Optional `field_original` names the prepared original geometry's `input.json`.
That control shades the complete original geometry with the same orthographic
capture frame instead of shading its captured material field. Outputs are
`radiance-result.json`, `<query>-source-field.f32` (128² linear RGBA for the fixed
input), and `<query>-radiance-world.{linear,depth,rgba,ss16}`. Set a query's
`dump_source_field` to false to omit that diagnostic source readback; rendering and
filtering then consume no CPU radiance image. `source_readback_bytes`, CPU
`image_storage_bytes` and declared `resident_image_bytes` are separate scopes.
The latter excludes driver allocation granularity; actual Metal device allocation
is reported separately. The source-copy submission is `field_source_upload_gpu_ms`;
`field_relight_filter_gpu_ms` includes shading and filtering together. The filter
has no independent submission time in this mode. `field_prepare_ms` includes device
configuration, CPU preparation, upload/encode calls, offline waits, optional readback
and support assembly. These overlap the individually reported phases and GPU work.
No CPU filtering/reupload or additional unchanged field render is needed.
Integer-footprint filtering explicitly rounds to
half precision before storage; native checks compare against independent integrals.

The executing diagnostic consumer retains its last successful GPU source and
radiance field. Its radiance reuse key binds raw source/view identity, eye position, focal length, sun elevation
and the optional source snapshot. The source shading profile is fixed in this
consumer. Light, eye, focal and snapshot changes relight the resident source; a
changed source/view prepares and uploads a replacement. Query names and
presentation-only sampling change neither product. Original-geometry comparisons always rebuild because they have
no immutable source identity. Retention adds no global cache or worker scheduler.
`field_renders` / `field_reuses` count actual preparation versus retention;
`field_resolve_ms` includes key lookup and any preparation. On reuse, source load,
source readback and field preparation/setup/GPU work are zero; presentation still
runs and its time is separate. This is not a whole-frame performance claim.

```sh
python test/experiments/render/replay_radiance_reuse.py \
  --client "$build/test_experiments_render_NativeBuildingImpostors" \
  --warm "$previous_replay/warm" --output "$new_system_temp_directory"
```

The source-free replay exercises repeat, A/B/A lighting, eye, focal length,
snapshot and catalogue-view changes. Reused and freshly rebuilt repeated inputs
must yield identical final raw outputs. It verifies one render device, no source
capture/writes and zero repeated field work. Outputs are losslessly compressed.

A query's diagnostic `filter_axis_samples` selects 1..128 physical samples per axis;
default 4 retains the existing `.ss16` output. Other counts write `.ss<N²>`.
This is a component convergence control, independent of the Places frame protocol.
In very small original geometry, SS16 can be far from the denser finite averages.
Compare SS16/1024/4096 before attributing a discrepancy to the new representation;
none is a continuous-integral bound or permission to regenerate existing pins.
Movement/history, angular error, changed-light scheduling, bounded World residency
and whole-frame costs remain open under #1570/#1127/#1524.

`--radiance-world` accepts query `support_plane: "fitted"`; default `"capture"`
preserves the archived control. Native bounded Givens-QR fits all occupied depths
once per retained source. Source changes invalidate it; pose/light changes reuse
its three coefficients with the resident source. Geometry changes only physical
support; capture UV, one coupled prefix and two triangles remain. The output binds
active coefficients, source point count, normalized RMS and fit wall time. RMS is
not a surface bound or depth authority. Changing support alone reuses radiance;
the source/light key stays independent of support placement. Inputs, original
opponents and existing shader filtering remain unchanged.

`field_lighting` chooses `capture` (the existing default) or `viewer`. Viewer uses
exactly the query camera used by the original geometry draw and projected support,
including an explicit `capture` override. Source raster rays remain unchanged.
The retained radiance key binds the perspective eye or orthographic direction;
focal changes, perspective rotation at the same eye and orthographic translation
reuse the PBR result. Source GPU data and the source-only support fit remain held
across observer/light changes. `lighting_observer` records the actual producer
binding, including on reuse. This is an explicit finite diagnostic, not World
pose admission or a relight-every-frame policy.

```sh
python test/experiments/render/replay_radiance_observer.py \
  --client "$build/test_experiments_render_NativeBuildingImpostors" \
  --warm "$stored_field_run" --output "$new_system_temp_directory"
```

The replay checks source-free loading, changed and irrelevant PBR inputs, unchanged
capture depth/rays, the default control and invalid observer rejection. It keeps
losslessly verified compressed readbacks. Its timing is diagnostic; it establishes
no World, pose, shadow, p99 or total memory budget.

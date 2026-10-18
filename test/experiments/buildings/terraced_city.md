# Admitted terraced families in the complete city window

Issue [#618](https://github.com/Weichwolf/outshine/issues/618), following the
[physical cohort checks](terraced_admission.md). `terraced_city.py` compares the
previous complete source window with the new scoped admitted mass families.
It retains all988 source buildings on the same flat plane, camera and profile;
there is no DEM, source culling or place-specific architecture. Architecture
choices still follow source properties, geometric feasibility and stable IDs.

These images retain their original69-source physical admission. The later
[ground-route correction](terraced_ground.md) is a separate functional follow-up;
it does not relabel this earlier city rendering as a new image run.

## Paired composition

The producer reconstructs the existing `PodiumDetails`/`DormerFacades` pipeline
first. Previously valid functional owners stay in that pipeline. The earlier
physical report supplies proposed programmes and their scoped successful/failed
outcomes; source footprint/property identities and the full original memberships
must match. Its modelling-file hashes under the Buildings experiment and Buildings
reference library must still match before reusing that earlier evidence. These
are reproducible experiment receipts, not a native cache reader, arbitrary trust
flag or a claim of rerunning the complete physical audit for this picture.

`PreviousParts` holds the previous same-camera **draw parts** for the paired
comparison, including the special radial source. New admitted sources alone use
`TerracedDetails`; rejected/unselected sources return the exact previous parts.
The producer checks all previous part fingerprints again after composition.
This removes unrelated regeneration and rendering changes from this comparison;
the retained draw parts are neither complete game assets nor a general cache
policy. They remain caller-owned for this offline comparison only.

New source plans/coarse owners are prepared once. The existing geometric
`FacadeProducts.select` runs before full near construction. Its0.35m exterior
position bound and at-most0.5px threshold are unchanged. A separate explicitly
named full-near reference can request the same new owners' complete near products;
previous parts and the camera remain unchanged. This is an exterior-position
experiment, not general radiance/transmission/shadow or temporal LOD admission.

The wall role binds to the actual provider/procedural source wall material.
Three authored accent/frame/sill roles use private comparison IDs60000–60002;
they cannot overwrite source IDs12+ or prior masked-facade bindings32768+.
Existing glazing/transmission and other component roles remain unchanged.
Coordinates, normals and UVs pass through the binding without copies. Material
indices are rebound; unnecessarily promoted64-bit draw indices are narrowed only
after checking their actual vertex range and32-bit capacity. No common
material/shader API changes.

The model sources remain the previously documented SIGGRAPH2006 scope/component
modelling and pinned SDK/Ghidra primitive references, optically checked before
this composition work. This study adds source composition, not a new Esri operator
or independently established full architecture family.

## Complete actual result and optical assessment

Frozen full producer `2089b8717` completes all three scenes with988 sources and no
omissions:987 ordinary buildings and the retained mast.69 ordinary sources receive
new mass programmes,40 cascade/29 paired wings;12 previous functional owners stay
protected. The two rejected ground approaches retain their earlier source parts.
Normal demand selects59 full near products and10 direct compact exteriors. The
explicit full-near reference expands the remaining10 using the same prepared
owners. All988 old draw-part fingerprints remain unchanged.

The reconstructed before mesh is byte-identical to the pinned prior complete
scene (`d48fe4dccaa47639fc2a43f06b99cf1f9040e47e61445696254c68f30fba6b37`).
Camera, material definitions/bindings and10000m ground also match. The prior actual
`4d54994c2` image is reused with its original render provenance, verified image,
input mesh/report hashes and eight matching renderer-chain source files. It is
not represented as a new before rendering.

Before, normal after and full-near whole images were optically opened at
1920×1080. The new steps, wings, terraces and deep windows interrupt several large
blocks across the district. Existing small roofs, dormers, courts and previous
functional buildings remain. The palette still reads uniformly grey; new glass
and pale interiors weaken some facade rhythm, and highest roofs remain bare.
This is visible mass diversity, not CityEngine-level recipe/finish diversity.

`test/scripts/pixels.py` observes92430 changed before/after pixels (4.4575% of the
image), mean RGB-channel difference1.087604/255. Normal selection versus the
full-near reference changes2546 pixels (0.1228%),1606 by more than1/255, mean
0.003124/255 and maximum49/255, confined to the distant upper image area. No black
or near-black pixels appear. These are single daylight-view observations; a
maximum radiance difference of49/255 is not a universal LOD-error acceptance.
Position selection remains independently scoped; no night/temporal proof follows.

## Work, memory and index-width replay

| Whole scene | New near / compact | Draw triangles | Final rendering-array MiB |
|---|---:|---:|---:|
|previous|0 /0|550101|142.572|
|normal after|59 /10|2620164|691.533|
|full-near reference|69 /0|2875611|759.257|

The normal scene emits255447 fewer triangles (8.883% of the whole scene) than the
full-near reference,10.681% fewer among new source draws. It is still4.76 times
the previous scene's triangle count. The added quality is costly, not a delivered
runtime optimization. The same-camera comparison reuses919 previous source draws;
it does not reconstruct their geometry for each after pass. New source plans are
prepared once; the full-near reference reuses the59 already-expanded near owners.

Original complete production takes803.221s after source acquisition, including
reconstruction of the old reference pipeline, both new draw passes, hashing and
exports. Before draw assembly324.504s; first new pass63.839s; incremental full-near
reference11.970s. These are sequential, different-state offline phases, not a
same-work speedup comparison. Whole-worker peak RSS3235.477MiB includes old assets,
saved per-source parts, new owners, combined meshes and Python metadata. It is
not a runtime asset/CPU/device bound. Per-source construction/allocation evidence
retains the scope of the [preceding study](terraced_admission.md); complete producer
allocation/copy traffic and general game-asset loading are not measured here.

Inspection found signed64-bit indices promoted by the earlier near-draw assembler.
Frozen `4aa5468a3` binds new draws to the existing32-bit capacity without changing
vertices, normals, UVs, materials or actual index values. `terraced_city_indices.py`
replays only that change on the two complete exported scenes, avoiding unrelated
old-model reconstruction. Original complete inputs/metadata remain retained;
the replay explicitly records `2089b8717` and original assembly timings.
Normal indices62883936→31441968B, full-near69014664→34507332B:29.985/32.909MiB less
render-array storage. Loaded-buffer range checking/conversion takes22.39/25.43ms
on this host; this excludes file loading, exports and whole-model construction.
It creates a new32-bit index buffer only when needed; material rebinding remains
a separate copy. No device latency or total-memory saving is inferred from it.

Both final replays are freshly rendered with Blender5.2.2 LTS/Eevee,16 samples,
llvmpipe LLVM19.1.7 and the same sunlight/sky/camera/material configuration. Every
RGBA pixel matches the respective original64-bit image; PNG hashes differ because
Blender records new date/render-time metadata. Actual imported triangle counts,
input/report/image hashes and1920×1080 profiles match. Original normal/full renders
take54.870/56.038s; these software offline times are not native frame costs.

Frozen final controls:5 source/5 installed tests pass, each locating94 actual
reference-module paths and matching their bytes to source. The earlier installed
reference is reused with main `.venv` dependencies, without isolation claims.
The earlier broad physical controls retain #684's evidence; they are not claimed
as a fresh full-cohort/controller test. Subsequent unrelated integration preserves
all94 loaded reference-module bytes. No library or native implementation changed.

Original whole production, unchanged before, comparison inputs and pixel logs:
`/tmp/outshine-buildings-city-models-2089b8717/`. Final typed mesh replays,
render/hash receipts and current images:
`/tmp/outshine-buildings-city-models-4aa5468a3/`. Geometry/UV/material/draw-array
costs and actual device/lighting/shadow/cache admission remain distinct.

The next concrete corrections are the two rejected ground approaches; richer
highest-roof/facade programmes and selective near components follow. Grey box
coverage, passing checks or this image alone do not close the architecture goal.

## Reproduction

Use `.venv` and explicit temporary paths:

```sh
export TMPDIR=/tmp
export PYTHONPYCACHEPREFIX=/tmp/outshine-buildings-pycache
export XDG_CACHE_HOME=/tmp/outshine-buildings-cache
export MPLCONFIGDIR=/tmp/outshine-buildings-mpl
export PYTHONPATH=lib/src:lib/tests:test/experiments/buildings:test/experiments/assets
.venv/bin/python -m unittest -v verify_terraced_city
.venv/bin/python test/experiments/buildings/terraced_city.py --audit /tmp/outshine-buildings-admission/cohort.json --output /tmp/outshine-buildings-city/models --full-near-reference
LIBGL_ALWAYS_SOFTWARE=1 xvfb-run -a .venv/bin/python test/experiments/buildings/roof_crown_render.py --geometry /tmp/outshine-buildings-city/models/TerracedAfter-geometry.json --output /tmp/outshine-buildings-city/images --samples 16
```

For an existing complete output from the earlier producer, the index-only path is
explicit; it retains original provenance rather than pretending to rerun models:

```sh
.venv/bin/python test/experiments/buildings/terraced_city_indices.py --geometry /tmp/outshine-buildings-city/models/TerracedAfter-geometry.json --output /tmp/outshine-buildings-city/typed
```

Produce the explicit physical input with the preceding
[admission experiment](terraced_admission.md). The producer requires a clean frozen
implementation and the pinned existing provider snapshot; no automatic refresh.
All private inputs/outputs, logs, renderings and display files stay in `/tmp`.
No specialist gallery, public images or showcase server is created.

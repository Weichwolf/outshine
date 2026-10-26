# Declared grass above the stored ground material

Owner #451, parent #6, goal #189. This is a cause-isolation experiment, not a
selected canopy BSDF or a product shader change. GroundMaterialPalette packs
Ground/Litter but not Grass/Dry/density/height. The removed Sward path previously
consumed the stand. meadow retains its explicit full RGB precomposition;
alpine_grassland instead stores limestone plus dry litter without that response.

The current source supplies alpine_grassland density1165/m², height0.12m,
width0.011m and dry share0.45. Its graminoid leaf colours are linear RGB
(0.1506,0.1892,0.0803) green and (0.3526,0.2377,0.0988) dry. Their declared mixture
is (0.2415,0.211025,0.088625). The first colour control uses cover0.829, copied
from the catalogue's historical nadir-cover declaration. It is an explicit
experimental assumption, not a newly validated directional cover estimate.

## Reproduction

Use a complete frozen matching client/source/shader package. Do not combine
current source includes with an old Device archive or solar LUT writer. The
provided native5ea6f823b package used here predates #400 and #439; it is not a
current-master acceptance build. All modes use that package's own shader includes.
GLSLANG names glslangValidator. Python uses only the standard library.

```sh
python3 test/experiments/render/ground_material_diagnostic.py \
  --source-checkout /tmp/frozen-native --binary /tmp/frozen-native/build/relinked-client \
  --preload-seconds 120 --output /tmp/outshine-ground-classes --mode classes Koerbersee
python3 test/experiments/render/ground_material_diagnostic.py \
  --source-checkout /tmp/frozen-native --binary /tmp/frozen-native/build/relinked-client \
  --preload-seconds 120 --output /tmp/outshine-ground-rock --mode rock-share Koerbersee
python3 test/experiments/render/ground_material_diagnostic.py \
  --source-checkout /tmp/frozen-native --binary /tmp/frozen-native/build/relinked-client \
  --preload-seconds 120 --output /tmp/outshine-ground-canopy --mode canopy-rgb --cover 0.829 Koerbersee
```

Default1920×1080, unchanged scenario and ten-second preload. Longer preparation
must be explicitly requested with --preload-seconds; it does not satisfy warm
acceptance. --compile-only records executed:false and can reproduce the shader
package without rerunning the client. Runtime outcomes and compiler/source/catalog/
binary hashes are recorded in the requested system-temp directory. The source
snapshot and product shaders remain unchanged. Classes use a named colour legend;
they remain lit, so RGB is not a calibrated numeric class readback. Rock-share
shows the existing class/slope rock weight before lighting, not source geology.

Canopy-rgb changes only the requested family's palette RGB and attenuates its
litter RGB difference by1-cover, preserving the separate base/litter mean at the
palette level. Existing detail modulation then acts as before. Roughness, normals,
geometry, shadows, other families and physical leaf transmission are not changed.
This does not prove the correct material response or near/far handoff.

## Native findings

All three controls completed through the public client on the matched native
package:60frames,360degree turn,23500reported triangles and0bare tiles. The saved
source classes show a substantial central hillside as alpine_grassland (green in
the marker); the rock-share diagnostic is dark there. Its ordinary grey response
therefore comes from the declared limestone/litter mixture, not slope-forced
rock. Mapped rock/scree still occupy other large pale areas and require their own
material-distribution work; grass is not an explanation for every grey mountain.

| Mode | Final PNG | CPU p99 ms | Over-budget frames |
|---|---|---:|---:|
| Source classes | 7c3850ef | 12.48 | 0 |
| Existing rock share | 1d89100a | 29.88 | 2 |
| RGB-only canopy control | 9bf4846d | 12.46 | 0 |

The retained baseline is697fd186. The RGB-only control changes108092 of2073600
pixels,102546 above1/255, worst44/255, mean RGB0.360727398/255. The central slope
becomes warmer/darker; this isolates a missing stand contribution but is not an
accepted canopy model or broad webcam-quality claim. The five-page comparison
contains RGB pair, class marker/legend, rock share, common crop and dated
Koerbersee2026-09-07 12:40CEST reference. Its contents were optically reviewed.
Weather/generated geometry/light are not matched to the photo. No product image
is published under doc/.

A first private C++ palette-producer control did not execute on the warm asset
hit; it emitted no diagnostic marker and retained the baseline PNG byte-for-byte.
InstallRegion restores the serialized ClassPalette. That attempt is explicitly
rejected as an unapplied control, not interpreted as absence of visible grass.
A future producer correction must version the palette-containing native product;
cache hits must continue loading the completed palette without regenerating it.

The delivered script reproduces all491SPIR-V artifacts byte-for-byte for each of
the three actual captures (1473 artifact comparisons); only the21Ground fragments
are rebuilt per mode. No shader-layout, C++ or public API changes are delivered.
The long image-preparation runs and diagnostic timings are not load/frame
acceptance: the rock-share diagnostic itself has two over-budget frames. Before
model selection, independent cover/angle/gap and leaf R/T references, actual
material response, near/far continuity and full affected budgets remain open.

Newer native packages also consume `groundPatchLit` fragments through the same
ground source. The runner discovers both existing fragment families, retains
their output-layout suffixes and compiles patches with `GROUND_PATCHES=1`, as
declared by the matching product build. Changing only `groundLit` would leave
the patch pixels unchanged. Old packages with only that family remain supported.
The bounded compatibility correction and actual21/42-variant controls belong to
[#653](https://github.com/Weichwolf/outshine/issues/653). Compile-only outputs are
not executed-image evidence; matching layouts do not close world/device gates.

## Restart completion on the newer matched closure

The unchanged delivered runner also completed on native3af55ff02, client SHA256
`a76eba6f59b5f3c56e6d75719d26b93dbabd5b20b593e30379900204a672f26d`.
This frozen closure includes #400/#439, but still predates the later module/CLI
migration and current native integration. It is no current-master acceptance.
The fresh unmodified baseline is af17648f and canopy control c3547362, both
1920×1080,60frames,23500triangles and0bare tiles. CPU p99 is17.73/14.56ms,
with1/0 over-budget frames; actual GPU mean/max6.528/12.635 and6.896/12.880ms,
60/60samples each. One nonisolated pair establishes no performance ranking.
Standing plus streaming24.7/23.0s remains above the unchanged ten-second target;
peak reported heap523MB each. No budget acceptance follows from this diagnostic.

The pair changes107983pixels,102550 above1/255, worst50/255 and mean RGB
0.361266397/255; bounding box remains[0,227,1919,743]. The repository pixel tool
independently checks the pair. Both pages of the new whole-frame/crop PDF were
optically opened: the declared grass slope darkens/warms while mapped rock is
preserved. Raw commands/logs, shader hashes, shots, pixel reports and PDF remain
under `/tmp/outshine-material-resume-20261017/`.

The actual timeout control uses a clearly labelled fake client that prints one
startup line then sleeps130seconds. With0.01s preload, the unchanged runner
terminates it after120.01seconds, returns124, records executed:true and the
explicit timeout reason, and preserves the exact startup line in capture.log.
The child is absent afterwards. This tests process/log handling, not rendering.

Vegetation has since delivered #462's bounded geometric contact reference:
fixed metric tangent footprint, upper direction toward observer/light, t≥0,
first-from-sky contact, explicit validity and source IDs. It demonstrates that
coverage changes with direction and overlap; it is not an optical R/T model.
That geometry is the next material-composition input, not a replacement of this
control's explicitly assumed0.829cover with another density×height formula.

Originals, explicit unapplied-control status, matching binaries/archives, commands,
source hashes, raw PNGs, PDF and reproduction checks are retained at
`/tmp/outshine-material-grass-aggregate-f9917ca9c/`. Substrate/source classes and
original catalogue parameters remain preserved.

Source principle first reviewed: SIGGRAPH2003 Decoret et al., Billboard Clouds for
Extreme Model Simplification, local PDF page5 (alpha/mip composition, Figures9–10),
optically inspected. Its composited-response/coverage principle is relevant; no
billboard-cloud algorithm is implemented. The old Sward constants at563af0330
are historical provenance, not current numerical validation.

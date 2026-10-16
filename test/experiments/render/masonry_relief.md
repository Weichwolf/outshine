# Filtered masonry height derivatives

Owner: #387, parent #306, material composition #6, goal #189. PR #399 records the initial private
comparison. The following production candidate shares its selected bevel primitive
with the standalone probes; it changes no binding, cached asset or geometry.

Before the production hookup, metric brick colour/roughness had zero heightChange.
This study gives the existing250×80mm running bond,10mm mortar gaps, a declared
2mm depth and2mm linear bevel. These dimensions are recipe assumptions, not
observed source attributes. Each axis is its rectangular brick band convolved
with a2mm box. Height is the product of the two profiles for each staggered row.
The resulting height is piecewise bilinear; its partial derivatives are affine
within each piece. No photographic texture, noise or displacement geometry.

Window/course coverage weights the height gradient itself. Differentiating a
product of mask and absolute height would create false window-edge bumps and
break invariance under a constant height offset. A flat brick across a partially
covered window edge must therefore retain exactly zero bump. Pure pane and
zero-depth controls also remain zero. Existing surfaceGradient/bumpNormal resolves
the final normal, including its degenerate-frame fallback.

## Production hookup

facadeMasonryRelief.glsl owns the chosen analytic bevel primitive; facadeMasonryField
accumulates its gradient under the existing16 wall/window/band weights and returns
screen-space heightChange. applyFacade consumes that existing output. All direct
experimental callers are migrated; the standalone probe imports the same primitive
while its polygon integral remains independent. Source RGB/MR arithmetic is retained.
Paired captures disable only the gradient contribution in a temporary source copy;
the enabled copy is the actual production shader. No runtime switch is introduced.

The public Engine wall consumer also uploads an authored normal texel/tangent
frame when mode3 is selected and captures sceneShadingNormal via the public API.
Its old modes1/2 remain. The actual production-enabled pair is byte-identical to
the prior verified prototype in all32 raster frames; the temporary enabled
source also compiles byte-identically to the unmodified production source.

Production-candidate checks on RTX3060 retain the976-input/five-method gradient
and period/authored-vector controls,32 raster pairs and480 independent pixel
normals. The public wall pair passes26 checks each. The real normal-texture pair
passes28 checks each:921600 normals, maximum composition error0.029556degrees,
RMS0.003081degrees;796017 flat normals are byte-equal. The fixed0.2degree tolerance
accounts for reconstructing the gradient from binary16 attachments; this compares
composition, while the polygon integral separately checks the height derivatives.
The native raster/compute probes use Vulkan Device; the paired public Engine uses
the explicit matching a56f3e3be headers/archive and retained SDL renderer. It does
not use a current Device vtable with an old archive.

The integrated helper's115200-probe cost sums under concurrent host/GPU use were
14.012512→40.498208ms and reverse31.514816→14.010368ms, each32 dispatches. The colour
control itself differs from the earlier isolated batch, so no code-regression or
whole-frame claim follows from comparing those absolute times. All six existing
colour/roughness/region outputs remain byte-equal, as do timed/plain captures.
No added buffers/bindings; shader code/register pressure still has a cost.
491 shader artifacts and nine compute contracts pass;262 artifacts (252 lit,
eight standalone controls and two fused cost probes) cross-compile to MSL2.1.
Focused clang-tidy on the changed wall consumer exits0 with no changed-unit
findings; the unchanged Check.h retains its14 existing diagnostics. Original
Metal atlas execution, Metal/A18 and equal-quality frame/device-memory checks stay open.

## Comparison and reference

Compare the point probe, one separable analytic pixel box, and2×2/4×4/8×8 oriented
subcells. Every method uses the same bevel and original geometry/mask definition.
The independent reference clips actual screen-pixel polygons against individual
linear-profile pieces, openings and bands. It integrates affine gradients using
polygon area/centroid; it does not sample the same subcells or candidate function.
The fixed acceptance limits are maximum2degrees and RMS0.5degrees after resolving
the mean gradient. This is not a BRDF or normal-variance integration reference.

968 inputs include480 actual native raster pixels, long/sheared/thin pixels down
to1:1024, window-edge motion, pure panes, flat window edges, degenerate footprints,
whole periods and large material coordinates. Large-coordinate native references
use the actually quantized float32 inputs; early phase reduction cannot restore
precision already lost in the asset coordinates.49 independent axis-box controls
precede the cohort; eight zero-depth rows extend the native execution.

The point and single-box methods fail.2×2 passes the initial720-case subset, then
fails the expanded movement cohort; its rejection is retained.4×4 is the cheapest
tested candidate inside the fixed limits: CPU maximum1.703648degrees,
RMS0.311470degrees; native maximum1.703654degrees/RMS0.311476degrees.
Native scalar parity stays below2e-4 and resolved-normal parity below0.05degrees;
the selected4×4 result reaches0.001530degrees. All five native methods and the
zero-depth controls run on RTX3060 through the existing public GPU consumer.

## Native work and actual raster

The baseline at327f5286d evaluates profile value and slope together. On the same
115200 probes×32 dispatches, selected4×4 without counters costs20.744288/20.726080ms
sums; with counters23.195776/23.112640ms. The optimized slope integrates periodic
bevel-interval overlap directly, evaluates profile values only for a nonzero
opposite slope and skips empty gradients. The identical uncounted cohort costs
6.107616/6.107200ms (about70.6% less measured dispatch time). Work-counter outputs
are returned without GPU formatting; timed/plain results and counted/uncounted
first seven outputs match bit-for-bit. This is not a whole-frame measurement.

The near-integer-period complement branch has180 additional native cases; maximum
normal error against the independent integral is0.000006904degrees.976 cases of
the actual bumpNormal with an authored shading vector retain identical height
derivatives and stay within0.000006306degrees of independently composed normals.
This does not exercise a normal-texture loader.

The private fused prototype patches copies of the actual facadeMasonry,
facadePattern and litFragment. It reuses each of the existing16 wall/window/band
weights and brick coordinates, then returns metre gradients through the existing
heightChange output. It adds no noise, texture or window/band evaluations, cached
asset fields, buffer layout bytes or bindings. Existing RGB/MR expressions remain.

The actual Vulkan raster consumer executes32 before/after pairs plus24 derivative
frames:480 independently integrated geometry pixels give maximum0.166681degrees,
RMS0.021490degrees. RGB/MR are byte-equal in all32 pairs; all eight excluded paths
and27330 pure-pane normals are unchanged. The optional compile_pair extension
also reproduces the existing default vertex/fragment SPIR-V byte-for-byte.

The same fused private shader renders the same native cached wall through the
public Engine MR path at1280×720. Both runs pass23 checks. The optically opened
comparison PDF shows modest recessed bevel lighting at the declared2mm scale;
99586 display pixels change, mean RGB difference1.669061/255. This public-Engine
crop still uses the retained SDL renderer; the component raster/compute probes
use native Vulkan Device. None is a current complete-world or Place frame proof.

Fused coating+masonry+relief cost uses the existing115200-probe colour cohort:
32-dispatch sums5.145344→13.570336ms, reverse13.585728→5.147776ms. The added work
is about0.2633ms per115200 probes on this RTX3060 cohort. Existing RGB, roughness,
opening and brick-field outputs are byte-equal;512 far controls have zero bump.
The consumer requests five buffers/33180160bytes and copies16590080bytes per batch;
these are requested resources, not driver/device peak memory. Shader code grows:
252 lit-fragment SPIR-V artifacts total41409956→44541812bytes (+3131856bytes).
Register pressure and whole-device memory remain unmeasured; no zero-cost claim.

All491 shader artifacts and nine compute layouts pass the existing contract audit.
252 modified lit fragments plus eleven private compute variants cross-compile to
MSL; this is source portability, not Metal execution. Raw inputs, source hashes,
paired outputs and local PDFs are retained in explicit system-temp directories.

## Reproduction

Use NumPy, Matplotlib, Shapely, installed outshine_ref, glslang and the existing
NativeGroundLitterProbe.cpp consumer linked to its matching native archive/headers.
The retained original consumer uses a56f3e3be; its hash is recorded in the native
report. Keep its archive and headers together; later Device API changes are not
ABI-compatible. Scene/raster consumers use the same retained matching closure.
No C++ unit changes here, so there is no additional clang-tidy unit. Actual raster inputs
come from the existing masonry_facade_raster.py and masonry_facade_pixel_reference.py
commands documented in [masonry_facade.md](masonry_facade.md).

```sh
PYTHONPATH=lib/src python3 test/experiments/render/masonry_relief.py \
  --native-input /tmp/outshine-masonry-reference \
  --output /tmp/outshine-masonry-relief-reference
PYTHONPATH=lib/src python3 test/experiments/render/masonry_relief_native.py \
  --input /tmp/outshine-masonry-relief-reference \
  --binary /tmp/native-material --profile \
  --output /tmp/outshine-masonry-relief-native
```

The additional commands accept the same retained native consumers and explicit
input/output paths (see --help); no product-source files are patched:

```sh
PYTHONPATH=lib/src python3 test/experiments/render/masonry_relief_raster.py \
  --binary /tmp/native-facade --input /tmp/outshine-masonry-raster \
  --coordinates /tmp/outshine-masonry-reference \
  --output /tmp/outshine-masonry-relief-raster
PYTHONPATH=lib/src python3 test/experiments/render/masonry_relief_scene.py \
  --binary /tmp/native-masonry-scene --fixture /tmp/wall.bin \
  --baseline-checkout /tmp/outshine-verified-shaders \
  --output /tmp/outshine-masonry-relief-scene
PYTHONPATH=lib/src python3 test/experiments/render/masonry_relief_cost.py \
  --binary /tmp/native-material --output /tmp/outshine-masonry-relief-fused-cost
```

Production-only checks use the extended native wall consumer with its matching
headers/archive and the prepared scene shader package:

```sh
PYTHONPATH=lib/src python3 test/experiments/render/masonry_relief_mapping.py \
  --binary /tmp/native-mapped-masonry-scene --fixture /tmp/wall.bin \
  --scene /tmp/outshine-masonry-relief-scene \
  --output /tmp/outshine-masonry-relief-mapping
PYTHONPATH=lib/src python3 test/experiments/render/masonry_relief_places.py \
  --binary /tmp/matching-outshine-client --scene /tmp/outshine-masonry-relief-scene \
  --output /tmp/outshine-masonry-relief-places Flensburg
```

The Place script uses fresh offline processes,1920×1080/60, unchanged scenario and
60 frames/360degrees, and checks preload<10s plus p99<=1000/60ms with no overruns.
Source acquisition/asset construction must finish separately before the warm run.
Unprepared-cache failures are retained separately, never passed as frame results.
Numeric timing thresholds do not assert image parity or complete-world quality.

Source basis: SIGGRAPH1980 Synthetic Texturing Using Digital Filters, local PDF
pages2/4 and Figures1/2 optically reviewed; filter texture and surface coverage
over the same pixel. Supplement: Mikkelsen JCGT2020 Surface Gradient-Based Bump
Mapping Framework, local PDF pages3/9, Equations2–3/17 optically reviewed. This
uses the existing derivative-frame contract, without introducing tangent assets.

## Corrected native world diagnostic

After #407 / Draft #416 restores the projected metre stream, the actual native
client now exercises brick walls. Both sides use exactly the same native binary
187a5b380+6156a8010 with only the verified two coordinate objects updated; the
owned42-link-input/header closure is recorded with #416. All491 shader artifacts
are identical except the252 paired lit fragments from this production candidate.
The disabled side reproduces the previous corrected-producer4b3734d8 PNG exactly.

At1920×1080/60 with unchanged declared fill0.6,585262 triangles/zero bare tiles,
60 frames/360degrees, enabling relief produces0b1301a6.6238 display pixels change,
141 by more than1/255, worst4/255, mean RGB difference0.001449/255. The optically
opened full/crop PDF shows a small distant joint-lighting response, no broad
Place improvement. Camera, clock, native geometry, cached products and RGB/MR
recipe remain the same. The strongest-difference224pixel crop is recorded at
(1280,544)-(1504,768); its selection scans96pixel windows in32pixel steps.

The first fresh10second offline attempts both fail before any frame, at10.087s
and11.711s under measured host load7.1–7.4. These are retained. A separate120second
image-preparation timeout produces complete paired captures: actual preload
8.002/13.006s, p9913.2218/11.3666ms, zero frame overruns. This is neither a speedup
claim nor warm-load acceptance; the after load still exceeds10s. Native
shadow/foliage parity also remains open in #328, so the good frame samples do not
complete equal-quality frame acceptance. Timing-consumer outputs now say
`timing_thresholds_met` and explicitly leave image acceptance unasserted.

The earlier retained SDL-renderer comparison, before the producer correction, is red: p99 before12.0354ms,
after25.7969ms; reversed after18.8958ms, before22.7139ms. All overruns are at
frame1, p50 stays0.98–1.03ms;60 frames/360degrees,585262 triangles and the
71c88aff PNG are unchanged. Warm offline preload3.64–6.00s; separate online
source acquisition/asset construction291.20s. Initial offline cache misses are
retained, not interpreted as frame timing. This does not isolate a shader
regression or satisfy the frame gate. The same image and dated Foto-Webcam.eu
2025-07-15 12:00CEST reference are optically reviewed in the local comparison PDF;
no Place image gain is claimed. The private material marker and #407 subsequently
proved that this earlier comparison had no metric-wall pixels; it cannot validate
the relief in the world. The corrected native comparison above does exercise it.

The unchanged original ComposedMaterialsReachTheCard.cpp consumer also runs against
this exact native renderer/header closure: before/after each31056 checks, zero
failures/skips/unprepared/partial, terminal0. The test exercises atlas generation,
normal capture and card consumption without changing its assertions. Source SHA256
f130e70dc0c5034a16de300e538d2642f1d56df5b5efcf50dec2aeb845d88908;
native consumer binary e0e56fecfa123639ed4a40d232109f94f6ec15a16b803a8cc6cc1f0b9a6f5f44.
Exact compile/link inputs and terminal runs are retained in atlas-build.json and
atlas.json beside the corrected-world captures. This proves native Vulkan atlas
consumption, not Metal execution or equal-quality whole-world frame acceptance.

Before merging the production hookup, the affected equal-quality frame/load
countercheck remains required. Metal/A18 counterchecks belong to Master;
full native-world/device-memory acceptance remains open. Real normal-texture
composition is now checked as described above. #386's known SDL submit
stall is independently recorded under #328; this experiment does not repair it.
Only Master publishes integrated Places. Private PDFs and raw inputs stay in
explicit system-temp directories; no images are pushed under doc/.

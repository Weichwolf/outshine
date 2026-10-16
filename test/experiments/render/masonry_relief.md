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

The public Engine wall consumer now also uploads an authored normal texel/tangent
frame when mode3 is selected, and captures sceneShadingNormal via the public API.
Its old modes1/2 are retained. Production tests and frame results follow in the PR;
Metal/A18 and complete native-world integration remain separate requirements.

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

Source basis: SIGGRAPH1980 Synthetic Texturing Using Digital Filters, local PDF
pages2/4 and Figures1/2 optically reviewed; filter texture and surface coverage
over the same pixel. Supplement: Mikkelsen JCGT2020 Surface Gradient-Based Bump
Mapping Framework, local PDF pages3/9, Equations2–3/17 optically reviewed. This
uses the existing derivative-frame contract, without introducing tangent assets.

Production hookup, the original material atlas, normal-texture composition, actual
frame/device-memory cost and Metal/A18 remain required before production adoption. #386's known SDL submit
stall is independently recorded under #328; this experiment does not repair it.
Only Master publishes integrated Places. Private PDFs and raw inputs stay in
explicit system-temp directories; no images are pushed under doc/.

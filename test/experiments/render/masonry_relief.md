# Filtered masonry height derivatives

Owner: #387, parent #306, material composition #6, goal #189. This is a private
experiment. No production shader, binding, cached asset or geometry is changed.

The integrated metric brick colour/roughness still has zero wall heightChange.
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

The first native implementation evaluates the box primitive's value and slope
together.115200 matched probes×32 dispatches: selected4×4 without work counters
20.744288/20.726080ms sums; with counters23.195776/23.112640ms. The low-quality
single-box control costs3.310336/3.310688ms. Thus the counter overhead is explicit,
and the selected component is initially about0.648ms per dispatch, not a whole
frame. Timed/plain outputs match exactly; disabling counters preserves the first
seven float outputs bit-for-bit. Counters themselves are returned, never formatted
inside the GPU work. These costs motivate avoiding unused profile-value work
before considering any production hookup.

## Reproduction

Use NumPy, Matplotlib, Shapely, installed outshine_ref, glslang and the existing
NativeGroundLitterProbe.cpp consumer linked to its matching native archive/headers.
The retained original consumer uses a56f3e3be; its hash is recorded in the native
report. It is not a current complete-world renderer claim. Actual raster inputs
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

Source basis: SIGGRAPH1980 Synthetic Texturing Using Digital Filters, local PDF
pages2/4 and Figures1/2 optically reviewed; filter texture and surface coverage
over the same pixel. Supplement: Mikkelsen JCGT2020 Surface Gradient-Based Bump
Mapping Framework, local PDF pages3/9, Equations2–3/17 optically reviewed. This
uses the existing derivative-frame contract, without introducing tangent assets.

Native material-MRT/lit images, normal-map composition, actual frame cost and
Metal/A18 remain required before production adoption. #386's known SDL submit
stall is independently recorded under #328; this experiment does not repair it.
Only Master publishes integrated Places. Private PDFs and raw inputs stay in
explicit system-temp directories; no images are pushed under doc/.

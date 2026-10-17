# Spatial native canopy response — #451

The fixed mean from #632 now retains a64×64 spatial field over the same1m²
footprint. Each cell averages4×4 original ground-anchored contact samples.
Unlit geometry coefficients, current homogeneous leaf R/T/substrate and current
RGB beam energies remain separate. The existing native compute driver executes
the new shader; no product shader, binding, world cache or renderer is changed.

## Model and filtering

Reuse [the material polynomial](canopy_response.md), #468/#477/#481's geometry,
opaque visibility, reciprocal sheet response and first-neighbour operator.
For each beam, gather its three direct basis terms at the saved first contact.
Gather incoming F*G on the contact's outgoing side for leaf R/ground g and the
opposite side for leaf T. Nine quadratic coefficients retain both interactions.
Average the twelve coefficients per cell, then form equal-area mips. With
homogeneous current material/light, filtering and evaluation commute.

The original65536 face/height/normal contacts and complete finite stand remain
authoritative. Gaps select ground nodes, never a fictitious leaf. No per-cell
logical IDs replace the original sources. Footprint is the authored tangent
frame grass-study-ground-1, east view(sqrt(3)/2,0,0.5), six fixed upper directions.
The whole nodal model uses288256 nodes/284162 finite triangles, a black external
boundary and one neighbour exchange; continuous transport and exact direct
shading at arbitrary contact positions are not established.

Primary technique source remains SIGGRAPH2002 Sloan/Kautz/Snyder PRT, local
PDF page2/Figure2 optically reviewed for #632. Spatial averaging and the material
polynomial are our own algebra; no spherical-harmonic or angular interpolation
method is implemented. See the linked source/method in canopy_response.md.

## Native contract and evidence

`canopy_field.py` creates seven Float32 geometric levels64/32/16/8/4/2/1.
`canopy_field_native.py` decodes them and sends current material/light separately
to the unchanged NativeGroundLitterProbe.cpp. Its128B compatibility prefix keeps
the existing harness input validation; it is not a new native palette format.
The private GLSL has the existing probe bindings/strides/offsets, independently
reflected against check_probe. MSL2.3 is generated, not Metal-compiled/executed.

The driver was freshly compiled/linked against the immutable native3af55ff02
source/header/archive closure, with LLVM21/libc++21 and warnings-as-errors.
This is an actual native Vulkan component proof, not current-master world or
Metal/A18 acceptance. C++ product and harness sources are unchanged; new
experiment Python/GLSL are formatted, linted and checked. No new C++ tidy scope.

Twenty states: front/back, no local transmission, white ground with absorbing
black leaves, black input and15 further bounded RGB material/light controls.
Full nodal outgoing and neighbour fields are evaluated before spatial filtering.
F64 field max error5.56e−16 versus preset1e−12; rounded coefficient error3.84e−8.
Mip coefficient mean error4.45e−9; unchanged mean limit1e−6. A separate comparison
with the exact old #632 source preserves all576512 footprint weights bytewise.

RTX3060, Linux x86_64/NVIDIA615.71.09, Vulkan validation1.4.309 enabled:
109220 valid plus2 invalid queries cover all states/levels. Native component
error1.30e−7; final direct+neighbour RGB error1.49e−7 versus preset1e−6. Invalid
records, output padding and all black states are exactly zero. Nineteen repeated
sets retain the same numerical result over2075180 valid queries plus2 invalid.
These are repeated material queries, not2075180 distinct geometry observations.

Both pages of the actual GPU-output PDF are optically opened: reference/native
front/back/no-T/ground-only and all native mips. Local grass/soil response and
backlight remain, while magnified64² pixels visibly limit near detail. Existing
pixels.py finds0/4096 differing pixels in both front/back sRGB reference pairs.
No scene-raster, natural grass, motion or Place-quality acceptance follows.

## Separated costs

| Quantity | Measured or counted result |
|---|---:|
| Geometric field payload,64² |1179648B|
| Seven-level geometric payload |1572768B|
| Preparation / whole20-state proof |12.34s /23.62s|
| Producer process peak RSS |480595968B|
| Full direct geometry fields / link arrays |83017728B /9224192B|
| Small native declared buffers / copy commands |17130528B /8565264B|
| Small32-dispatch GPU sum / full GPU batch |2.468704ms /3.258304ms|
| Large native declared buffers / copy commands |268773408B /134386704B|
| Large32-dispatch GPU sum / full GPU batch |42.214528ms /55.414112ms|

Native full batches include upload,32 repeated dispatches and readback. Dispatch
means are0.077147/1.319204ms, not frame p99. Buffer counts include staging and
readback; they are declared bytes, not measured physical VRAM peaks. CPU memcpy
into staging additionally copies parameters+probes once. Field preparation also
allocates gathered65536×3 Float64 arrays(1572864B each), a65536×9 quadratic
array(4718592B), combined65536×12(6291456B) per beam, and the stacked field.
Internal NumPy/SciPy/Embree/driver allocation counts and traffic remain unmeasured.
The host is i5-10400 with one numerical thread/nice10; interactive desktop and
other nice builds were active. Native measurements retain normal priority.

This is a prototype field, not1.57MB for every world square metre. Prototype
sharing, density/material variations, angular selection and native asset budgets
remain undecided. Native queries load completed coefficients and do not trace,
build a BVH or regenerate source geometry; current test references are separate.

## Reproduce and retained originals

Use existing NumPy/SciPy/embreex/matplotlib/Pillow dependencies, the matching
native component driver and GLSLANG=glslangValidator. Vulkan validation layer
must be installed/discoverable; the first missing-layer run correctly failed.
No validation suppression was used. Full compiler/link commands, hashes and
layer paths are retained in the proof directory.

```sh
PYTHONPATH=lib/src:test/experiments/vegetation OPENBLAS_NUM_THREADS=1 OMP_NUM_THREADS=1 \
  python test/experiments/render/canopy_field.py --inputs /tmp/outshine-material-contacts --output /tmp/outshine-material-field
PYTHONPATH=lib/src:test/experiments/vegetation \
  python test/experiments/render/canopy_field_native.py --prepared /tmp/outshine-material-field --binary /tmp/native-material --output /tmp/outshine-material-field-gpu --gpu-timing
PYTHONPATH=lib/src:test/experiments/vegetation \
  python test/experiments/render/canopy_field_view.py --inputs /tmp/outshine-material-field-gpu --output /tmp/outshine-material-field-view
```

Add --repeat-queries 19 for the large native workload. Original contact inputs:
`/tmp/outshine-material-canopy-response-inputs-5aeb4f802/`. Current prepared data:
`/tmp/outshine-material-canopy-field-gap-8f1cf23e3/`; native results:
`/tmp/outshine-material-canopy-field-gpu-final-8f1cf23e3/` and sibling gpu-cost.
Compiler/format/reflection/regression logs:
`/tmp/outshine-material-canopy-field-proof-8f1cf23e3/`. Actual images/PDF:
`/tmp/outshine-material-canopy-field-view-final-8f1cf23e3/`.

Angle/footprint changes, heterogeneous live/dry blades, roughness/specular detail,
wind, near/far handoff and full native world/cache/Place/device acceptance remain
open. #451 and the complete material goal #189 are not closed by this probe.

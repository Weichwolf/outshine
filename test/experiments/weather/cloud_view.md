# Actual native opaque cloud-front consumer (#344)

This assembly connects the owned public WeatherSnapshot/MakeLowCloud product
(#341), the shared MediumView224 camera frame (#319/#359), and the existing
joint cloud/air prefix (#300). It uses the regular production archive and the
native Render API on Vulkan or Metal; there is no second backend implementation.
It is an executable integration reference for the productive frame pipeline
(#328), not a replacement renderer or a completed Place/weather implementation.

## Data and ownership

The unchanged recorded Open-Meteo input is decoded through the public provider
and Source::DecodeWeather at its exact UTC. Its known-zero low cover selects the
clear-air path; nonzero high cover is not promoted to a low layer. Other cases
are explicitly declared low cover0.65, base2572m ASL, thickness600m and declared
layer advection (east6.92820323027551m/s, north4m/s). The10m observation is not
silently extrapolated to cloud height. Source/procedural origin, exact UTC and
payload hash accompany each capture. Unknown low cover remains distinct in the
owned product; this experiment does not invent an unknown-source fallback.

MediumView224 remains fragment3/2. The private experiment-only WeatherFrame is
six vec4/96 bytes at3/3: CloudLayer shell/field/drift, density camera anchor,
cloud sun irradiance and sky radiance. CPU size/alignment/offset assertions and
actual pipeline reflection check both blocks. This is not a new shared runtime
binding. World light/material parameters remain with their existing owners.

The density frame is fixed around the spherical Earth origin. The camera's
explicit density position plus MediumViewRay.OriginOffsetM supplies each ortho
ray origin; the full anchor cannot be replaced with camera/model height. Radial
WorldUp and physical eye height come from that same position. World axes follow
the existing ENU-to-render conversion; no duplicate NED wind convention is added.

## Actual raster and transfer

The first native render pass writes prelit RGBA and Depth32Float for three
camera-facing plane patches at500/1600/4500m along view. It uses actual projected
vertex depth, the real viewport including its depth range, and separate pixel
scissors. Depth is never CPU-painted or supplied by the reference calculation.
The second pass samples that Scene/Depth and writes composed RGBA, RGB radiance
plus stored depth, RGB transmittance plus ray distance, and logical work counts.
Sky depth0 already represents eye radiance and passes through unchanged.

The cloud/air partition is built from the complete physical atmosphere/cloud
ray before querying the opaque receiver. Query depth does not repartition the
fixed quadrature. Composition is exactly T*scene + illuminance*L_per_lux once,
with scene alpha preserved. Known-zero low cover follows the exact existing
mediumViewAir path and is byte-compared with mediumClearOpaque. Uniform inputs
are mutated after command recording to verify copied input ownership. All
resources outlive submission completion through the existing native helpers.

Clear solar/Psi LUTs are deliberately constant and the air control is homogeneous
and ozone-free. Cloud sunlight/sky fields use the existing explicitly approximate
source recipe. #320/#353 own the further actual direct-source shadow integration
and reuse; diffuse/Psi convergence remains open. This control does not establish
physical view/source convergence or choose a new runtime sample profile. The
existing16-view/6-light/warp1 defaults remain unchanged.

## Reproduce

Use the project's C++23 toolchain and a matching regular production archive.
All private outputs must be explicit system-temp paths. CXX, GLSLANG and
SPIRV_CROSS select the installed project tools. Python needs NumPy/Pillow;
plotting additionally needs Matplotlib. No dependency is vendored.

```sh
make all
python3 test/experiments/weather/cloud_view_native.py \
  --archive-root "$archive_checkout" --output "$weather_output/native"
python3 test/experiments/weather/cloud_view_reference.py \
  --captures "$weather_output/native" --output "$weather_output/reference.json"
python3 test/experiments/weather/cloud_view_controls.py \
  --native-receipt "$weather_output/native/receipt.json" \
  --output "$weather_output/controls"
python3 -m unittest discover -s test/experiments/weather \
  -p test_cloud_view_reference.py
```

The producer and current production source/headers must agree, and the archive
must contain exactly one production MediumView/LowCloud unit. The builder uses
central compiler/standard/warning/optimisation declarations and native link flags.
It records commands, terminal exits and hashes; captures retain the actual native
scene/composition/fields, scalar physical inputs, independent FP64 rays and source
metadata. Repeat rendering must be byte-identical, not merely perceptually close.

Thirteen cases cover clear, declared perspective/ortho, jitter, offset viewport
and0.2…0.85 depth range, inside/above height, an orbital foreground receiver,
yaw rotation, translation,
wind-only advancement and combined movement. Wind-only advancement keeps the
camera rays and scene/depth byte-identical while changing the field response.
The orbit case queries a foreground receiver before atmospheric entry; it does
not establish an orbital raster receiver behind the cloud layer. Complete
orbital transfer is separately exercised by the existing prefix controls.
The independent FP64 slab reference integrates the same fixed partition by
analytic Beer transfer; RGB/L/T use an unchanged1e−4 absolute implementation
control. It does not share the shader implementation or its reconstructed rays.
Three closed-form controls check vacuum, homogeneous analytic air, and a
receiver before either medium.

Ray distance compares against independent FP64 inversion of the actually stored
Float32 depth within2mm. Stored depth against ideal patch depth uses a bound of
two clip/window ULPs. That quantisation may itself exceed2mm in world space;
this is explicitly not a2mm ideal-surface accuracy claim. #359 repairs the prior
19.7cm cancellation counterexample without increasing the ray tolerance.

Four actual wrong GPU variants must miss the same1e−4 reference limit: omit
ortho origin, drop horizontal density anchor, repartition at the receiver, and
compose the front twice. Their ordinary native geometry/alpha/finite/validation
checks must still pass. A fifth clear-opaque control must remain byte-identical.
The controller also translates the four baseline and five control SPIR-V modules
to MSL2.3. Metal translation does not establish an A18 runtime result.

## Images and cost scope

The same native executable accepts optional width height and one case name
following its shader paths/output prefix. Use1280 720 declared-perspective and
1280 720 captured-clear for a matched larger pair; each renders twice. Plot the
actual images with cloud_view_plot.py --clear --cloud --output. The two-page PDF
contains unchanged native image data with an explicit linear-to-sRGB display
conversion, transmittance and actual logical work fields. Inspect both PDF pages.
No scientific plot is substituted for a Place capture or published to the gallery.

Reference results report actual per-frame sums of view-density evaluations,
cloud-source evaluations, union cells and air-source evaluations. A negative
clear-source sentinel remains unavailable, not zero cost. The diagnostic counts
are logical sample calls, not instructions, GPU execution times or memory traffic.
Declared texture texels occupy84*width*height+64 bytes; five readback buffers/copies
use5*align256(16*width)*height bytes per captured frame. Two capture vectors coexist
for the repeat comparison, each80*width*height bytes. These explicit payload bounds
exclude allocation granularity, command/backend/driver storage and OS reserve;
they are not a measured GPU/total-process peak or a production allocation plan.

Full scene/world publication, sky and Thin/history integration, full radiative
convergence, all Places, GPU timings, CPU/load/process peaks and A18/RTX frame
budgets remain open. The productive renderer must reuse/size resources according
to image benefit and measured cost instead of copying this diagnostic MRT setup.

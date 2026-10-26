# Joint metric facade coating

[#278](https://github.com/Weichwolf/outshine/issues/278) owns this experiment.
The verified kernel now lives in `src/render/shaders/facadeCoating.glsl` and
is consumed by the production facade path. Native structure pieces already
transform relative ECEF vertices into a fixed local scene frame through
`TilePieces::RowFor` and `TangentFrame`. `litVertex` supplies this unshifted
East/Up/North position at location10; `litFragment` now consumes it. No new GPU
resource, placement layout, cache or material parameter is required.
The large-coordinate experiment below remains a rejected generic/legacy input;
it does not describe the native Place-piece frame. Full lit Places, Metal/A18
execution and frame/memory budgets remain integration checks with master.

## Field and composition

The existing cubic SurfaceNoise3 field uses1.5m cells, centred at zero. Nine
metric subcell positions share the existing opening/pane/band subcell coverage.
Each field sample is weighted by `1-opening-0.35*band`; multiplying two separately
averaged factors would lose their covariance. The three region coverages are
resolved once, before the existing material composition. Roughness is unchanged.
Axis-aligned coverage retains its existing exact box formulas. Numerical clamps
preserve pane⊆opening and band⊆opaque wall.

Coating adds `amplitude*weightedField` to the original linear material colour,
where amplitude per channel is the lesser of0.16 times the nonnegative source
colour and twice its distance to the nearest0/1 boundary. Given valid source
reflectance in[0,1], the signed field's bounded wall contribution therefore
preserves that interval without a biased final colour clamp. Pure black/white
cannot move outside the physical range; existing frame/pane constants remain.
This is a colour variation, not a simulated stain, wetness, roughness or bump.

Visibility fades smoothly from one to two cells of maximum physical derivative
length. Beyond3m per pixel it returns exactly zero variation and the existing
coverage path; fully glazed profiles likewise use no noise. Near work is at
most72 noise-corner hashes per fragment, without claiming compiler sharing.
The declared mean is retained in the far field, not forced across each finite
facade patch. The prototype accepts an explicit common physical meter frame;
it does not infer one from camera-relative or asset-local coordinates.

## Native comparison and limits

`facade_coating_native.py` prepares3624 fixed cases: House/Commercial profiles,
rotated/sheared footprints and six91-step rotation/translation/growth tracks.
Independent64×64/128×128 midpoint integration evaluates pointwise wall/band
coverage and the complete3D field. Separate NumPy interval integrals check the
actual nine-subcell implementation. Inputs are rounded to FP32 before parity
comparison. The references run in one nice10 child; actual GPU captures run at
normal priority, with no mock/archive fallback.

Measured implementation disagreement is at most3.50e-5 in the four filtered
parameters, under the fixed2e-4 parity threshold. The coating-only contribution's
largest pixel-area error corresponds to0.001457 linear RGB at the default wall
colour;64→128 refinement changes that contribution by at most0.000289. The six
tracks' erroneous weighted-field step RMS is0.000165–0.000360 before the colour
amplitude. These finite results are not a global filtering or TAA guarantee;
they do not bound the existing region-coverage approximation's whole-image error.

Four additional colour cohorts check bounded reflectance, black/white identity,
unchanged far mean and full glazing. Local diagnostic: `facade-coating-native.png` (generated in system temp; not published).

The separate image chart contains actual
native material colours at160×96 pixels, enlarged2×, in a small explicit meter
frame. It demonstrates the prototype, not the current Earth renderer or Places.

The ordinary caller links the shared `NativeGroundLitterProbe.cpp` against the
regular published Vulkan archive with matching headers. The test's private copy
of the pinned ground palette supplies valid protocol input; wall RGB is read
from BuildingMaterials.h and roughness0.9 is an explicit test value. Per-probe
physical position/derivatives append12 floats after offset320 in that private
copy. This is experiment parameter transport, not a proposed runtime palette,
new binding or asset format. The source input is never rewritten.

Matched115200-probe batches of32 dispatches measured1.004743ms without coating
and2.099626ms with it on RTX3060. These GPU debug-label intervals include dependency
barriers and exclude copies. The profiling-only a9 backend enables the supported
hostQueryReset feature for Nsight; ordinary correctness uses the newer published
raster/compute archive. Outputs agree byte-for-byte across those captures.
The synthetic transport requests25,807,360 logical buffer bytes and12,903,680
copy bytes; these are not production allocations, process peaks or a frame/
memory-budget proof. No A18 or integrated renderer cost claim.

## Precision counterexample

`facade_coating_precision.py` uses a synthetic81-point line on each axis near the
Wien ECEF position, expressed relative to the existing fixed WGS84-A anchor.
The intended points advance2.5cm. Actual GPU evaluation of the existing FP32
coordinate/division/noise path leaves88.33% of neighbours identical; maximum
error versus the same physical FP64 points is0.10172 in the field, or0.01204
linear RGB at the default wall colour. This is a concrete rejected input frame,
not a shader-parity failure or a Place rendering. The input includes its full
physical points and native outputs for inspection.

The fixed WGS84 RuntimeScene anchor concerns a different path from the native
streamed structures. `HandsPiecesOver` sets the piece frame once from the declared
scenario origin; `PrepareBuildingSurfaces` uses the same tangent frame. Asset and
LOD anchors change the relative ECEF coordinates and placement translation, not
the common metric scene coordinates. Arbitrary scene-origin rebasing and
Earth-sized generic inputs remain outside this finite precision proof.

## Actual production raster

`facade_coating_raster.py` and `NativeFacadeProbe.cpp` execute the actual
`litVertex`/`litFragment` pair through the ordinary native archive, not a replacement
fragment. The material pass writes three160×96 float attachments. A real
`TangentFrame` at Wien supplies its ECEF origin and axes; fixtures follow the
existing `TilePieces::RowFor` formula. Two asset anchors separated by hundreds of
meters and an independent camera shift describe the same physical wall. This
checks the unchanged row formula and existing vertex transport, not the tile
streaming scheduler or a complete LOD transition.

Twenty-six raster captures cover House, Commercial, walls without openings,
gables, complete glazing, pane fields, negative trim/glass/metal-roof codes and
the far fade. All complete opaque pixels are present. Different asset anchors
and camera shifts disagree by at most3.04e-6 in albedo/metalness/roughness under
the fixed2e-4 comparison. The controls and far field remain exactly unchanged;
windowed roughness differs by at most5.97e-8 from the baseline. Near opaque RGB
changes by at most0.05442. The local comparison PDF was optically opened: broad
flat wall patches gain restrained coherent variation while windows/bands remain
readable. This is an albedo component gain, not lit Place acceptance.

The original3624-case independent-area/parity/motion proof and matched compute
cost comparison remain applicable because the production kernel is byte-identical
to the verified experimental source. Production fragment SPIR-V/MSL2.3 translation
passes252 old/new variant pairs: resources, layouts, output contracts and MSL
resource slots agree. Only the existing matching vertex location10 is added as a
fragment input. MSL translation does not prove Metal execution.

The focused C++ consumer is strict C++23/no-exceptions, with fixed-size fixture,
vertex offsets and complete-output checks. Its archive and headers must match.
For each capture it requests two upload buffers(288B), vertex/placement buffers
(288B), three245760B colour images plus737280B of readback storage, and608 uniform input bytes;
these logical products are test overhead, not new runtime material allocations.
The production change adds at most72 noise-corner hashes per near wall fragment,
fades noise to zero beyond3m/pixel and skips fully glazed/negative-code surfaces.
Actual driver allocations/peak memory and integrated device times remain open.

## Reproduction

Requires the reference Python dependencies (NumPy/Shapely), Pillow, and
glslangValidator (`GLSLANG` can select it). SPIRV-Cross translates the probes to
MSL2.3 separately. Use the regular production archive's exact native headers and
`sh test/run.sh --native-link-flags` when linking the common consumer.

```sh
PYTHONPATH=lib/src python3 test/experiments/render/facade_coating_native.py --binary /path/to/native_material --output /tmp/outshine-material-coating-native
PYTHONPATH=lib/src python3 test/experiments/render/facade_coating_image.py --binary /path/to/native_material --output /tmp/outshine-material-coating-image
PYTHONPATH=lib/src python3 test/experiments/render/facade_coating_precision.py --binary /path/to/native_material --output /tmp/outshine-material-coating-precision
```

Build the raster consumer against the matching current product archive/headers
and the platform's native link flags (requires SDL3 and the backend dependencies):

```sh
c++ -std=c++23 -Wall -Wextra -Werror -fno-exceptions -ffp-contract=off -Iinclude -Isrc/base -Isrc/base/geo -Isrc/render/device/native -Itest/harness/shared -Itest/harness/render test/harness/render/NativeFacadeProbe.cpp build/liboutshine.a $(sh test/run.sh --native-link-flags) -o /tmp/outshine-native-facade
```

The production raster comparison accepts a baseline fragment alongside its
baseline include directory, generated BRDF tables and SPIRV-Cross reflection:

```sh
python3 test/experiments/render/facade_coating_raster.py --binary /path/to/native_facade --baseline /path/to/baseline-shaders/litFragment.glsl --output /tmp/outshine-material-coating-raster
```

All outputs require an explicit system-temp path. `--reference-only` is CPU-only
and never establishes GPU success. Normal entry clears previous success before
preparation; the shared capture launcher deletes stale native outputs. No output
pin, cache or product asset is regenerated. SIGGRAPH material composition and
orientation references, including the optical PDF review, remain in #196/#284.

# Joint metric facade coating prototype

[#278](https://github.com/Weichwolf/outshine/issues/278) owns this experiment.
It does not activate coating in the product. The existing large FP32 world
coordinates fail the fine-field precision comparison below; a stable, precise
source/asset material frame must be agreed before integration. The verified
window/band coverage filter from #285 remains independently integrated.

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

Performance confirmed the current RuntimeScene anchor is fixed through ordinary
streaming/LOD. That prevents camera swimming but does not recover fine precision.
General SubjectMesh anchors can also change. The source/asset phase and basis,
including placement/LOD/rebasing behavior, remain the explicit missing contract
in #278; no shared GPU layout or phase encoding has been changed here.

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

All outputs require an explicit system-temp path. `--reference-only` is CPU-only
and never establishes GPU success. Normal entry clears previous success before
preparation; the shared capture launcher deletes stale native outputs. No output
pin, cache or product asset is regenerated. SIGGRAPH material composition and
orientation references, including the optical PDF review, remain in #196/#284.

# Positive normal response on canonical prototypes and a whole spruce

Owner: vegetation, [#643](https://github.com/Weichwolf/outshine/issues/643).
Continues the [positive finite-measure comparison](normal_lobes.md).
The purpose is to carry that component into actual existing prototypes and the
cached whole plant, without changing needles, geometry, coverage or albedo.
The raw positive prototype fields are valid; the first Blender atlas binding is
**rejected for minified use** by the actual counter below. This remains an
**isolated unshadowed unit-diffuse diagnostic**, not a new PBR,
R/T, wind, shadow or native material contract.

## Same source raster, additional bounded output

`conifer_window_catalog.template_image` optionally captures its existing visible
interpolated normals after rastering. Default image arithmetic stays unchanged.
Covered fine-pixel slots map to their native32² footprints with weight1/64;
packets≤8192. Capture re-normalizes Float32 interpolated normals in Float64,
without changing the existing image's Float32 normals. Occluded back layers do
not enter the measure. Mismatched native sizes refuse before projection/raster.
`NormalMeasure` can request only the needed partition fields before allocation.

The local SIGGRAPH2007 normal-filtering source and exact finite-measure proof
remain owned by the linked report and
[library contract](../../../lib/normal_distribution.md). Existing raster/BVH,
projection, normal partition, moment/filter and material helpers are reused.
No source acquisition, whole-plant organ expansion or second raytracer is added.

The same frozen8canonical size classes contain2/4/8/16/32/64/128/256needles,
64views each, native32²/8×8 subpixel samples. Three capacities(256/32/2) at
view indices0/19/47 first compare104light queries and all6MIPs to1pixel.
All nine newly captured original images are **exactly equal** to their previously
cached64-view bank entries. Thus coverage, colour, old normal and depth channels
are preserved, including small partial-window prototypes.

## Actual prototype comparison

Frozen source study2dce6f1db; maximum absolute response error across104queries
and all native32² pixels, including gaps. Unit diffuse, common1/pi omitted.
`unit4` rescales each bin to its mass; it preserves isotropic mean and can
reintroduce directional overestimation. Four and24partitions are not nested.

| Capacity / view | Old mean | Lower4 | Unit4 | Lower24 |
|---|---:|---:|---:|---:|
|256 / 0|.319923|.036829|.036829|.058084|
|256 / 19|.327979|.085047|.084269|.049842|
|256 / 47|.300277|.043979|.043665|.038884|
|32 / 0|.391677|.060323|.060323|.026985|
|32 / 19|.309244|.075324|.073960|.034378|
|32 / 47|.331832|.081192|.081106|.021067|
|2 / 0|.369170|.053053|.053053|.026863|
|2 / 19|.347596|.046953|.046953|.030376|
|2 / 47|.306843|.029464|.029451|.015479|

The256/0case explicitly shows that24bins need not beat the different4partition.
Bounds pass for the same finite raster at all6MIPs; source aliasing remains
unbounded. Representative256/19,32/19 and2/47plots were actually opened at
2560×1440. Side response is recovered and front overbrightness improves.
They remain native32² diagnostics, not high-resolution organ geometry.

67source and67installed controls pass, including original source/area/joint
controls, actual twig/window controls, fixed near-crown rows and leaf normals.
All58actually imported installed reference-module paths/bytes match the unchanged
#667wheel. All828NPZ fields across the nine installed prototype replays match
source exactly. The reused wheel SHA is recorded in normal_lobes.md; no new
library build or unrelated package admission is claimed.

## Complete diagnostic bank and costs

`prototype_normal_bank.py` prepares all512views from the same existing canonical
sources. Every original cached image stays exact. Only4bins and one reference
direction are requested per capture; unused6/24fields and104-query reference
work are avoided. Capture processes3,283,476visible samples in total.
Local build27.211s on e957abbef, one numerical thread, overlapping other work.
This is CPU offline preparation, not GPU/frame/device timing.

Store direct3component Float32 vectors in each projection frame:25,165,824B
for all8banks/512views. The first diagnostic bank also saved6,291,456B of
old-mean comparison vectors which the whole consumer did not use.99e9c0cb3
removes their calculation, retention and export; the new complete bank matches
all512consumed vector fields exactly. Historical bank input is retained only
for the actual whole-frame proof. Maximum direct-vector
rounding response bound5.009e-8 across the bank. This applies to direct encoded
vectors, **not** a raw(W,M) codec or nonlinear mass-preserving normalization.
No world residency budget is accepted from these sizes.

Only selected family archives decode for the nine-case study. The complete bank
build intentionally reads all8existing families, one at a time; each NPZ stores
all64views per array, so it cannot decode just one requested view. Prototype
geometry is cold preparation only. Whole-plant consumers read the finished
vectors and original banks, without canonical needle construction or placement. The diagnostic portrait also
hashes original preparation inputs for provenance; this is not a standalone
production cache-hit IO/latency proof.
Allocator counts/hidden GEOS/raster/Blender/driver memory are not fully measured.
Host: Linux/WSL2 x86_64,4CPU affinity/15.44GiB, Python3.13.5/NumPy2.5.4.

## Actual shader and whole-plant scope

`normal_component_material.py` binds signed Float32 fields with their original
UV projection frames. UV tangent, cross(normal,tangent) and geometric normal
transform the current light into chart coordinates. Evaluate positive dot sums
**after** texture filtering, divide once by camera coverage for the conditional
emission, and apply coverage×existing tail fraction once in transparency.
The baseline normalizes the filtered original mean normal, retaining its
existing mean-normal loss. Signed components are never encoded as colour photos.

The emission-based diagnostic isolates this response from Blender's BSDF,
light attenuation, shadows, ambient and specular models. A complete material
must combine joint camera/light visibility before filtering; multiplying this
response by an independently averaged shadow coverage is not justified.
The unnormalized4-bin model's isotropic energy loss remains explicit.

Actual Blender controls on10314ee6f check unit flats, proper90°rotation and a
same-mean/two-lobe mixture in both modes. Six64²frames match independent linear
expectations0.8/0.6/0 or0.3 within.000196 (half-float EXR quantization included).
Two additional opposite-light frames on80c2f3d38 pass (mean0/four0.3,
max error.000049), specifically checking negative stored vector components. EEVEE/Blender5.2.2, llvmpipe software, not a hardware GPU proof.

Whole-plant comparison uses the unchanged #626cached source/identity, physical
inventory and same camera. Read source/source hashes before binding. Existing
wood prototypes and complete camera-window selection are shared; shadow-only
surfaces are outside this deliberately unshadowed component diagnostic.
No LOD/source/needle-count/density reduction supplies the comparison.
Initial313b74ebf attempt failed before rendering because the empty Blender scene
had no World;80c2f3d38 initializes it explicitly. Failed starts are not evidence.
Both actual2560×1440/32sample whole frames on80c2f3d38 exit0 and were opened.
They retain49,186axes,79,085camera windows/158,170window triangles and14,124,783
represented source needles; none of those needles is expanded for the portrait.
Four instanced wood templates retain2,360,960wood triangles. Both modes share
the same complete source hashes and raw-frame alpha is exactly equal.

The scalar response change reaches the tree, but the crown remains conspicuously
thin, regular and tiered. The new field redistributes brightness, rather than
uniformly dimming every needle: full-frame linear RGB RMSE .006048, max .140869,
mean signed delta +.000407. PNG comparison via test/scripts/pixels.py changes
162,370pixels(4.4046%), channel MAE .176514/255. No whole-tree physical reference
was expanded, so this is **not a proved whole-tree error reduction or botanical
quality gain**. Single-interval references establish only the component's benefit.

Mean/four populate1.555/2.003s, render+export345.530/347.284s, peak process RSS
2696.61/2760.41MiB. Concurrent one-llvmpipe-worker software runs, not isolated
GPU/device timing. These are diagnostic renderer costs, not an approved realtime
representation. No speedup is inferred. Cold prototype preparation is separate.

The initial whole report's generated_image_float_bytes=0 is invalid: its predicate
excluded Blender's UV_TEST image type. A fresh actual material-binding control
finds2(mean)/5(four) Float32 320²RGBA images per family, giving26,214,400/
65,536,000B controlled CPU image payload for8families. Telemetry now selects
owned images by their diagnostic prefix. These counts exclude driver/GPU/MIP/
allocator overhead; originals and an explicit erratum remain in the private proof.
Only the counter changed after the successful render, with an actual binding
countercheck; no unverified rerender or whole-GPU allocation claim.

### Minified atlas counter: binding rejected

A known frame/filter restriction from the library contract must also hold in the
renderer. `normal_component_mip_probe.py` on0df85fc47 places one unit-normal field
in chart0 and valid rotated encodings of that same source normal in neighbouring
charts. The current light and world normal coincide, so response must remain1.
It renders the same chart at64pixels and1pixel, with alpha measured separately.

| Sampler / footprint | Conditional response | Expected |
|---|---:|---:|
| Linear /64px|1.000000|1|
| Linear /1px|.475187|1|
| Closest /64px|1.000000|1|
| Closest /1px|1.000000|1|

Four actual EEVEE frames exit0; their candidate-pass flags retain the Linear/1px
failure. The alpha sum is1.000218 for both tiny cases, so missing coverage does
not explain the lost response. Four-pixel atlas gutters cannot prevent deep
hardware mips from mixing chart frames. The successful magnified scalar/rotation
controls did not test that, and the whole frames cannot establish correct distant
light response. **Do not adopt this atlas binding as a runtime/LOD material.**
Closest avoids this specific counter on llvmpipe, but removes required footprint
filtering and has no moving/coverage-quality evidence; it is not accepted as the
solution. Next isolate each view's mip chain (for example per-view texture-array
or UDIM storage) and repeat this counter plus the whole comparison. No new
shared binding or native format is prescribed. The original per-view numerical
fields/MIPs and their proofs remain valid; this is a renderer-storage failure.

All outputs, logs, `.venv` and complete banks stay under/tmp, currently
`/tmp/outshine-vegetation-prototype-normals-proof-a5021bcb6/`.
No showcase server, image publication or native binding is introduced.

## Reproduction

Use one numerical thread and explicit fresh/tmp outputs:

```sh
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/prototype_normal_study.py \
  --models /tmp/existing-canonical-models --catalog /tmp/existing-capacity-catalog \
  --output /tmp/outshine-vegetation-prototype-normal-new
PYTHONPATH=lib/src:test/experiments/vegetation .venv/bin/python \
  test/experiments/vegetation/normal_component_portrait.py \
  --whole /tmp/existing-complete-plant --catalog /tmp/existing-capacity-catalog \
  --fields /tmp/existing-positive-vector-bank --mode four \
  --output /tmp/outshine-vegetation-component-portrait-new
```

#643 stays active. Fix per-view mip isolation before accepting the binding;
then coherent shadows/R/T/specular/wind,
further camera/light poses and shared world costs remain required. Native
integration and final species/stand quality are separate, unfinished gates.

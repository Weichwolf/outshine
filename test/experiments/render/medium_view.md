# Shared medium camera frame (#266)

`MakeMediumViewParameters` builds one224-byte snapshot from the actual Lens,
camera basis, SkyStanding, Medium80 and native Viewport. Build it once per
camera/viewport/environment update; passing a prepared snapshot to many draws
does not require repeating the factory. No allocation, IO or device call occurs
in the factory. The new shared helpers are consumed by the actual native raster
experiment below; they do not add an SDL_GPU call or switch the old rendergraph.

Material and Performance explicitly confirmed fragment set3/binding2 under
#266 (6196966936, 6197078105). Material3/0 and Lighting3/1 remain unchanged.
The opaque consumer retains Scene/Depth/MultiScatter/Veil at fragment set2/0..3.
The native layout supports the free3/2 slot without dummy uniform blocks.

## Snapshot and reconstruction

The first192 bytes retain the proposed aerial field layout: Right/Up/Fwd,
WorldUp/SunDir, TanHalf/Illuminance/EyeRadiusKm, DepthReconstruction and Medium80.
PixelToNdc at offset192 stores XY scale and ZW offset. It converts actual fragment
pixel coordinates to unjittered clip coordinates. Its scale uses the actual
Viewport extent/offset; its jitter uses the actual projection matrix. Lens
picture dimensions and viewport dimensions need not be equal.

OrthoProjection at208 contains explicit half width/height in metres and a Z flag
(1 orthographic,0 perspective); W is reserved zero. Lens.OrthoWidthM/OrthoM are
full dimensions. Perspective direction varies by pixel with a common camera
origin. Orthographic direction is constant, with a distinct physical origin per
pixel. `mediumViewRay` returns this world-space OriginOffsetM and Direction plus
the spherical AirRay. It accounts for the displaced origin in radius and solar/
view cosines, rather than retaining the centre camera's radial normal.

Native fragment pixels have a top origin. Existing Vulkan negative-height
viewport and Metal viewport agree on clip Y up. Viewport and scissor are separate:
the raster experiment supplies an explicit matching pixel-centre scissor. A
viewport alone is not assumed to mask an oversized fullscreen triangle.

For projection coefficients A=p14,B=p15,C=p10,D=−p11,
view distance is `(A−z*B)/(C+z*D)`. With actual window depth `w=m+s*z`, fold its
range into the existing four coefficients as `(A*s+m*B, B, C*s−m*D, D)`.
No extra depth-range uniform or depth texture is required by Thin. No10m ground
lift is added to physical camera height. Invalid viewport, projection, camera
basis and nonfinite medium inputs return a named expected error.

`mediumViewAir` fixes the physical atmosphere/planet/top extent first and queries
only afterward, using the already merged prefix. `mediumClearOpaque` applies
`T*scene + illuminance*L_per_lux`, preserves alpha and leaves eye-radiance sky
pixels with zero depth unchanged. It is explicitly clear air. Full cloud-front
transfer needs #300 and its separately declared density/camera anchor: add
OriginOffsetM to that anchor in the same coordinate frame. MediumView does not
replace the world-anchored cloud field, add another composer or claim cloud
illumination/shadows/Psi are solved.

## Executable controls

Build the regular archive on a frozen clean checkout, then run with the same
C++23 compiler and shader tool used by the project:

```sh
make all db
python3 test/experiments/render/medium_view_native.py --output "$medium_output"
```

The output must be outside the checkout, in system temp. `CXX` and `GLSLANG`
select the existing tools. The builder uses the published native link flags and
requires exactly one production MediumView object in the regular archive. It
records source/consumer/archive hashes and terminal commands; it does not
compile a private backend copy. The same consumer/shaders can run on Metal/A18.
Runtime acceptance there is required separately; MSL translation is not that run.

- 1,022 CPU projection/depth controls cover finite/infinite perspective and ortho,
  nonzero jitter, viewport offset, reversed/window depth and invalid inputs.
- 36 actual43×31 three-target geometric raster frames compare radius, origin,
  direction, solar/view cosines and distance to independently projected world
  points. Paired depths preserve physical origins/directions/cosines bitwise.
- 75 actual opaque frames compare clear Rayleigh/Mie/Psi transport against an
  independent homogeneous analytic solution. Asymmetric scene/depth/solar/Psi
  inputs, both world-up axes, rotated views, day/night, top entry, planet clip,
  zero illuminance, vacuum and zero camera distance are included. RGB uses the
  existing1e-5 relative/1e-7 absolute control limits; alpha/sky/outside-scissor
  preservation is exact.
- Exact homogeneous light integrates the visible parts of each ray separately at
  the planet's shadow-cylinder/solar-plane crossings. A night camera alone does
  not imply every distant point is shadowed. The analytic reference uses double
  precision world projection/sphere intersections, not the shader's fitted source
  coefficients or its reconstruction implementation.
- Two native repetitions must be byte-identical. A private wrong orthographic
  origin and a no-medium opaque mutation must fail their intended assertions
  with the same native resource interfaces and no validation diagnostics.

Raw captures and references are retained; plot them with:

```sh
python3 test/experiments/render/plot_medium_view_native.py \
  --samples "$medium_output/actual" --output "$medium_output/comparison.pdf"
```

## Remaining integration

Master/Performance own the native Stage/target/viewport transport. Material owns
Thin/Coverage calls and the simultaneous premultiplied blend-state change. These
helpers supply their shared camera/source geometry, without changing the old
SDL renderer before replacement. Complete all-media front transfer, cloud
source shadows/Psi, neutral RGB/depth/velocity identity, history, full Places,
CPU/GPU/device budgets and Metal/A18 acceptance remain #266/#193/#206 work.
The small homogeneous controls do not close those features or justify a full
resolution per-fragment prefix cost. No specialist images are published in doc/.

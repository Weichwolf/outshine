# Finite GGX evaluation

Issue: [#829](https://github.com/Weichwolf/outshine/issues/829).
This arithmetic correction is independent of the material-image precision in #824.

The original Float32 expression `nh*nh*(a2-1)+1` cancels to zero at the
specular peak for roughness 0.01. The equivalent positive denominator
`(1-c)*(1+c)+c*c*a2`, with `c=min(abs(nh),1)`, preserves the small moment.
Dividing successively avoids squaring this denominator into underflow.
Cosines rounded just above one are clamped at the Fresnel endpoint as well.

The RGB result is evaluated directly: `F/d * (V*incidentCosine/pi) * (a2/d)`.
Forming a scalar `D*V` first can overflow although the Fresnel-weighted RGB
result is representable. In the direct-light consumer, `incidentCosine=nl`:
a valid mirror pose with roughness about 1e-8 and nl=nv=vh about 1e-6 has a
BRDF exceeding Float32, but its cosine-weighted response remains representable.
Clipping the BRDF before applying nl loses this response. Diffuse, anisotropic
specular and sheen receive the same cosine once; clearcoat uses the weighted
GGX evaluation too. Layer weights, energy compensation and lighting inputs are
otherwise unchanged.

`precise` protects the ordered intermediates. The existing zero-moment
endpoint stays zero. No roughness floor, extra texture, binding, allocation,
upload or per-backend shader copy is introduced. The regular vector arithmetic
replaces the earlier experimental exponent-decomposition alternative.
Results outside Float32's representable range saturate at its largest finite
value; this is not a proof for arbitrary subsequent light intensities or
attenuation factors. Input subnormal losses and source r-to-r⁴ underflow
remain separate numerical limits, not successful accuracy comparisons.

## Numeric reproduction

`ggx_stability.py` uses the existing four-buffer native material probe and
the production shader include. Its independent Float64 controls use an
angular GGX NDF and Smith lambda. The 3672 records cover zero, ordinary and
small roughness, minimum normal and subnormal moments, adjacent Float32
values, rounded cosines, grazing mirror/aligned poses, and zero/coloured/tiny
Fresnel inputs, both with and without cosine weighting.

Each input has twelve Float32 fields; each output has eight Float32 fields.
The normal-input error bound is 32 Float32 epsilons relative, with one minimum
normal Float32 of absolute allowance for output flushing. Nonfinite
representable outputs fail separately. Input-denormal accuracy losses and
unrepresentable reference components remain listed in the report. A passing
normal-input result does not admit those losses as parity.

```sh
python3 -W error -m unittest discover -s test/experiments/render -p test_ggx_stability.py
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation python3 test/experiments/render/ggx_stability.py \
  --binary "$native_material_probe" --consumer-receipt "$build_receipt" \
  --baseline "$reference_shader_directory" --output "$temporary_output_directory"
```

The consumer receipt must match the executable SHA256. Compiler commands,
source and SPIR-V hashes, input fields, raw readbacks and classified errors are
retained in the system temporary output directory. Baseline compilation uses
the original consumer's final cosine multiplication; candidate compilation
also checks the new weighted helper. This compute test does not prove the
actual raster call sites or a device/frame budget.

## Raster and integration controls

`test/harness/render/NativeGgxRasterProbe.cpp` drives RuntimeScene,
SceneRenderer and the real material raster shader. It renders a fully covered
64×64 orthographic black dielectric under one front directional light, with
environment and shadow association disabled. Light intensity is 100r⁴, which
keeps the normal-incidence peak representable for all four roughness controls:
0.01, 0.02, 0.12 and 0.5. Base, clearcoat and iridescence exercise the changed
consumer routes. Readback size, depth coverage, finite radiance and a nonzero
specular response are checked; complete linear Float32 pixels are saved.

Compile this diagnostic with the complete compiler/link arguments of the
matching native render harness. Run the same executable with reference and
candidate shader-product directories as its respective working directories;
its sole argument is an existing temporary directory for raw rasters.
Record the C++ source/archive commit separately from the shader commit when
using a frozen executable. A shader-only comparison does not rebuild or admit
unrelated native integration changes.

Shader artifact/reflection checks, focused diagnostic clang-tidy, actual
raster inspection, unchanged-energy controls and equal-profile world timings
belong to the delivery evidence. Metal execution and the 1280×720/60 p99
device target require their own results.

## Primary references

- SIGGRAPH 2014, *Moving Frostbite to Physically Based Rendering*, printed
  page 12 / PDF page 13, Listing 2; local reference:
  `doc/references/lighting/siggraph/2014-frostbite-pbr-course-notes.pdf`.
- [GLSL 4.60 specification](https://registry.khronos.org/OpenGL/specs/gl/GLSLangSpec.4.60.html),
  precise variables, floating-point precision, and permitted denormal flushing.

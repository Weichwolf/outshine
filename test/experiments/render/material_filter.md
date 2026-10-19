# Material filtering before field reuse (#12 / #6)

The isolated storage defect is tracked by [#824](https://github.com/Weichwolf/outshine/issues/824).

This is an isolated **production direct GGX** comparison on Iris Xe, with opaque
isotropic dielectric inputs and unchanged geometry. It does not introduce a
cache, lower geometry resolution, bake final lighting, or certify a full atlas,
lit Place, environment, shadows, extensions or frame budget.

The storage defect is specific to **AreaMixture**: BuildImpostorSurface chooses
AddOpticalCardImages when any atlas surface is DiffuseThinSheet. Opaque-only
atlases use AddOpaqueCardImages with perceptual-r UNORM8 and are unaffected by
this r^4 encoding defect. A fully opaque footprint inside a mixed atlas still
uses AreaMixture. The separate [native atlas/raster confirmation](https://github.com/Weichwolf/outshine/issues/824#issuecomment-6264456136)
also preserves that positive opaque-only control; it is not an additional raster
claim for this numeric probe.

## Reference and controls

The private probe calls unchanged `metalRoughBrdf.glsl` and
`microfacetEnergy.glsl` at **0eb2e739d**, including the production energy table.
Its small IO wrapper does not reimplement the BRDF or change the existing four
buffer bindings. The existing NativeGroundLitterProbe is unmodified: source at
c058467a1 is identical to 0eb2e739d. Only this existing tool is linked against its
own frozen, clean c058 product archive using complete original CDB/native-link
flags. No product sources or archives are rebuilt or changed; no new native
implementation is added.
The tool is SHA-pinned in the receipt. Intel-only ICD and validation select the
reported **Intel Iris Xe Graphics (TGL GT2)**; GPU timestamps are unmeasured.

Fifty-one material/area-weight/view configurations each evaluate 161 light
directions and six field choices: **49,266 native responses**. The fine reference
is the area-weighted sum of the two independently shaded inputs. Alternatives
are parameter means, ideal existing atlas moments, the current 8-bit r^4 channel,
and an 8-bit perceptual-r control. Geometry/depth/IDs are never mixed. Normals are
shading fields on the same geometric support; all coverage weights are explicit.

Controls separate albedo-only, roughness 0.2/0.8, normals ±20°, constant r=0.2,
and actual procedural-pane parameters from `facadeOpenings.glsl` (r=0.12,
albedo 0.025/0.035/0.045). View angles are 0/35/65°; light angles −80…80°.
These are controlled materials, not new rendered OSM surfaces or a worldwide set.
Atlas moments mean r^4 and preserve normal resultant length before the existing
Toksvig roughening. Byte variants isolate **only roughness encoding**: albedo,
normal and coverage quantisation, texture interpolation and native mip raster
are not silently included in their admission.

Thirty-six endpoint arrays are bit-identical to their matching fine inputs.
Albedo-only parameter mixing differs by at most 5.96e-8 linear RGB. An independent
Float64 GGX/energy-table reference has maximum absolute difference 0.08232 and
maximum `abs(error)/(1+abs(reference))` 0.002292, including sharp r=0.12 lobes.
This is numerical context, not a relaxed visual tolerance or GPU precision proof;
all filtering errors below compare **native GPU response with native GPU response**.

## Quantisation defect, distinct from spatial filtering

Current `ImpostorOptics::FieldsOf` stores r^4; `Encoded` rounds it to UNORM8.
`litFragment` reconstructs r by the fourth root in the AreaMixture path. A constant
r=0.2 gives `round(255×r^4)=round(0.408)=0`, hence decoded r=0. Its positive
direct specular lobe disappears because the existing BRDF returns zero for a²=0.
The real-arithmetic zero interval extends up to approximately **r=0.21043**.
This happens without downsampling, mixed materials, normal variance or motion.

At view35°, unit white directional irradiance, the native r=0.2 peak specular
RGB component is **2.44023 → 0**. For the procedural-pane r=0.12 control it is
**18.8684 → 0**. These are unclipped linear responses, not display-pixel errors.
The same-byte perceptual-r control preserves r=0.2 and its native response
exactly; the r=0.12 peak becomes 17.9161. It is a useful precision alternative,
not a complete replacement contract: hardware interpolation of r no longer
interpolates the r^4 moment. Native filtering/format choice must be resolved
with the consumer rather than changing a decoder alone.

## Heterogeneous fields remain a separate approximation

At 50% area/view35°, maximum absolute linear RGB error is:

| Field | Parameter mean | Ideal atlas moments |
| --- | ---: | ---: |
| Albedo only | 1.49e-8 | 1.49e-8 |
| Roughness 0.2/0.8 | 1.16259 | 1.20610 |
| Normals ±20° | 0.48525 | 0.45228 |

Correcting storage does not make a single averaged lobe reproduce a mixture of
lobes. The existing moment method improves this normal control's RMSE, but does
not retain its two highlights. Thus lowering field resolution requires a
separate angular/material-error decision or local retention of finer fields.
No screen-wide lower-resolution material path is admitted by this probe.

## Reproduction and cost scope

Use `.venv`, NumPy, Matplotlib and glslangValidator; an existing native consumer
and production `brdfTables.glsl` are explicit inputs. Without `--binary`, only
preparation is reported; the plotting script refuses that state.

```sh
.venv/bin/python -W error -m unittest discover -s test/experiments/render -p test_material_filter.py
.venv/bin/python -W error test/experiments/render/probe_material_filter.py \
  --tables "$TABLES" --binary "$NATIVE_CONSUMER" --output "$FRESH_SYSTEM_TEMP"
.venv/bin/python test/experiments/render/plot_material_filter.py \
  --input "$FRESH_SYSTEM_TEMP" --output "$SYSTEM_TEMP/angular.png"
```

The native probe reports five requested buffers totalling **14,189,120 B** and
**7,094,560 copied bytes**, matching the Python ledger. These buffers exercise
all alternatives, not a proposed runtime cache layout. Driver/OS peaks and
Python allocation traffic are incomplete; reported Python RSS excludes the
native child. No timing extrapolation to frames. The angular PNG was opened
locally; no image is published to doc/README.

SIGGRAPH-first source: local [Karis 2013, PDF pages 2–3](../../../doc/references/materials/siggraph/2013-unreal-engine-4-shading.pdf),
visually inspected, parameter/result blending and GGX r² parameterisation.
Outshine uses its own existing correlated visibility/energy functions; this
experiment does not substitute UE4's published visibility remapping.

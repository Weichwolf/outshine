Type: defect
State: active
Architecture: ready
Parent: 2171
Depends: 2301
Priority: P1
Area: import, render, materials
Tags: khronos, tangent, normal-map

# Normal maps and generated tangent geometry agree

## Evidence

The public-client linear corpus evaluates all 25 geometry-versus-normal-map
relations in NormalTangentTest and NormalTangentMirrorTest. Every p95 relative
value exceeds the manifest's 0.157975 bound: lit pairs read about 0.20–0.30;
several dark pairs read 1. The earlier manifest derives this boundary between
correct MikkTSpace and a deliberately flipped handedness bit. The new runner
uses the earlier evaluator's nearest-rank quantile and per-channel relative
denominator. Full PNG agreement (95.7750% / 89.7554%) is diagnostic only.
The asset declares `NEAREST_MIPMAP_LINEAR`. Disabling mipmaps while changing
the minification filter to linear makes all 25 relations pass, but confounds
two variables; it is not a fix. Flipping tangent.w makes the ten lit pairs
substantially worse, so handedness is not the primary defect. A direction-mip
reduction independently violated the first-moment integral; its correction
has an analytic test, but the 25 image relations remain red. A controlled
four-sampler experiment resolves the confound: both mip filters (9986, 9987)
fail 15/15 NormalTangentTest relations; both non-mip filters (9728, 9729)
pass 15/15. At the five dark pairs, the geometry emits zero while the mapped
partner reaches 5.76–7.52 linear radiance. Removing Toksvig roughening makes
those dark pairs pass. This points to unmatched normal-footprint integration:
the map gains finite specular width from its mip, but smooth geometry remains
an ideal delta under punctual lighting. Confirm with float normal/confidence,
roughness and radiance AOVs before implementing a shared specular-AA rule.
NEGATIVE: applying `1-(|dFdx(n)|²+|dFdy(n)|²)/24` as a screen-space
normal confidence to both paths reduced a few lit pair errors but made the
dark pair reach 327/1253 linear peak radiance (geometry/map) and lowered PNG
agreement. The trial was reverted. A derivative-sized GGX lobe under a delta
light is too narrow for one raster sample; derive pixel-integrated specular
energy or a bounded distribution before changing the production shader.

## Contract and implementation

`src/import/` owns glTF tangent import/generation and handedness; native
geometry keeps tangent XYZ and sign. `src/render/shaders/normalFromMap.glsl`
owns TBN reconstruction and normalScale, with the sampled normal texture read
as linear data. Inspect the two public captures' linear radiance, normal and
identity attachments at paired cells before changing the shader; distinguish
wrong basis, wrong texel/UV, wrong normalScale and wrong direct-light response.
Keep the glTF sampler unchanged. Derive any geometric normal-footprint variance
from screen-space derivatives and apply the same roughness model to both paths;
check that a zero-roughness surface and a mipped normal map conserve energy
under movement. Do not add a sampler override or an asset-specific threshold.
The same native material path must serve imported and generated geometry.
No per-asset correction or softened bound.

## Falsifiable acceptance

- The 25 unchanged manifest relations pass through the public client. Flipping
  tangent.w, a map's green channel, UV set or map colour space fails an
  independent synthetic pair and the relevant vendor relation.
- Mirrored and nonuniformly scaled placements preserve the world normal and
  keep visible diffuse/specular highlights coherent; compare float AOVs,
  PNGs and a moving camera.
- `make format`, focused import/material tests and `LINT_JOBS=2 make lint` pass.

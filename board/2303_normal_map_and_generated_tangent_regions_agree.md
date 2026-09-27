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
has an analytic test, but the 25 image relations remain red. Next isolate
spatial filtering from mip choice with the declared sampler intact, then
compare float normal/confidence and radiance AOVs across paired cells.

## Contract and implementation

`src/import/` owns glTF tangent import/generation and handedness; native
geometry keeps tangent XYZ and sign. `src/render/shaders/normalFromMap.glsl`
owns TBN reconstruction and normalScale, with the sampled normal texture read
as linear data. Inspect the two public captures' linear radiance, normal and
identity attachments at paired cells before changing the shader; distinguish
wrong basis, wrong texel/UV, wrong normalScale and wrong direct-light response.
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

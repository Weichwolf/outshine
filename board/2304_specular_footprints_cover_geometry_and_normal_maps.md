Type: defect
State: open
Architecture: investigation
Parent: 2171
Depends:
Priority: P1
Area: render, materials
Tags: specular, antialiasing, normal-map

# Specular footprints cover geometry and normal maps

## Problem and evidence

`litFragment.glsl` broadens a normal-mapped GGX lobe using mip confidence,
but the same material on smooth geometry has no pixel-footprint integration.
In Khronos NormalTangentTest's five dark pairs, unmapped geometry emits zero
while the mipped mapped partner reaches 5.76–7.52 scene-linear radiance.
Disabling mipmaps makes all 15 pairs pass; changing spatial nearest/linear
filter alone does not. Doubling resolution halves lit pair error but leaves
the dark pairs at relative error 1. Temporal resolve smooths their PNG yet
does not equalize response. A derivative confidence
`1-(|dFdx(n)|²+|dFdy(n)|²)/24` produced peaks of 327/1253 linear radiance;
that trial was reverted. The corpus uses a directional delta light and
zero-roughness cells; its old threshold was derived before correct mip use.

## Architecture decision needed

Specify one finite, energy-bounded pixel/light-footprint response for native
Metallic-Roughness surfaces, including roughness zero, smooth geometric
normals, and normal maps with declared glTF samplers. Separate light angular
extent from surface normal variance; do not treat a derivative-sized GGX
lobe evaluated once at pixel centre as an integrated pixel. Decide whether
an analytic convolution, bounded multi-sample resolve, or a hybrid meets the
720p60 budget, and document its high-frequency and moving-camera limits.
Filament `ef1a133d` offers optional clamped derivative variance in
`shaders/src/surface_shading_lit.fs`; it is a reference, not a proven fix for
this zero-roughness delta-light counterexample. Preserve material energy and
colour and apply one rule to importer and generator geometry.

## Falsifiable acceptance

- A small analytic plane/sphere pair has finite, nonnegative, bounded radiance
  as roughness tends to zero; integrated energy does not rise with resolution.
- Geometry and its baked normal map agree under a finite light footprint at
  720p and 1440p and during movement; negative flipped-tangent and wrong-mip
  controls remain discriminating.
- Khronos paired regions and PNGs are re-evaluated without asset-specific
  shader branches. If the delta-light fixture is physically ill-posed,
  replace that acceptance only with an independent finite-source oracle and
  keep the old measurements visible as diagnostics.
- Measure p50/p95/p99 frame cost and memory at 720p on a Place plus the
  analytic scene; run format, focused tests and full lint.

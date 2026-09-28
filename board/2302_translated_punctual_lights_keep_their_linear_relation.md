Type: defect
State: open
Architecture: ready
Parent: 2128
Depends:
Priority: P2
Area: render, lighting
Tags: khronos, precision, point-light

# Translated punctual lights keep their linear relation

## Evidence

The public-client PointLightIntensityTest captures all eight imported lights.
Its RGB+white and three monochrome+white relations look identical in RGBA8,
but each exceeds the declared 32-f32-ULP bound on 814–2280 scene-linear
components. The worst difference is 19,577 ULP at low radiance near the
finite-range edge; RGB+white p95 is 19 ULP and p99 is 48 ULP. The strict
bound exposes a small spatial precision error, not the 20.2521% Blender PNG
appearance difference: the asset explicitly disclaims exact appearance.

`litVertex.glsl` forms interpolated surface position from instance matrix and
frame shift. `SubjectDraw::PackedLights` separately rounds ECEF light position
plus frame translation to float; `subjectLighting.glsl` subtracts the two
rounded positions. This is a likely cancellation source, not yet a proven one.
An experiment moved surface, light and camera into the shared asset-anchor
frame. RGB+white failures only fell from 2280 to 2142 components; gray rose
from 1143 to 1188. Its PNG changed by 0.0005 percentage points of oracle
agreement. The experiment was reverted. Per-instance placement and the BRDF
remain possible causes. At the worst sample, 10,878 ULP are only 1.98e-8
absolute radiance at about 3.0e-5; do not confuse this with a visible defect.

The 32-ULP bound is much stricter than the asset's stated "very nearly so".
Retain the red diagnostic until an independent precision budget proves or
replaces it. Priority is P2 behind contact, terrain and visible material gaps;
do not add a per-instance light table solely to pass this bound.

## Contract and implementation

Keep imported candela, colour multiplication, inverse-square falloff and the
declared finite-range taper. Do not tune range, exposure, colour or the test
bound. In `src/render/stages/SubjectDraw.cpp` and `src/render/shaders/litVertex.glsl`
and `subjectLighting.glsl`, first expose the per-fragment light displacement
and attenuation on a small translated analytic fixture. If displacement already
matches within the bound, attribute the BRDF stage instead and update this WI
before changing packing. Otherwise express each emitter and receiver in one
stable draw-local coordinate frame; compute the shared origin in double on CPU,
then upload local floats. Keep directional lights and camera-relative shadow
coordinates on their existing contracts. Instance batching may split by local
anchor only under a measured draw/upload budget; no per-pixel double dependency.

## Falsifiable acceptance

- All five PointLight relations pass their unchanged 32-ULP limit through
  `make corpus-render CASES=PointLightIntensityTest`. A one-channel colour,
  light range or light position mutation fails the appropriate relation.
- Independently translated and rotated six-panel fixtures retain the relation;
  no case-name or exact-camera branch. The reference PNG remains diagnostic.
- Measure GPU p95/p99 and draw/constant-buffer bytes with 1, 8 and 100 local
  lights; no silent light truncation. Open the PNGs and affected night Place.
- `make format`, focused light/subject tests and `LINT_JOBS=2 make lint` pass.

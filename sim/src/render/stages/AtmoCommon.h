/* The `Atmo` uniform struct + scattering helpers every atmosphere shader needs. A textual WGSL
 * splice, not compiled on its own — each consumer concatenates it ahead of its own body. */
#ifndef ATMOCOMMON_H
#define ATMOCOMMON_H

#include <cstdint>

namespace outshine::Render {

/* THE size of the Atmo uniform, in one place: every bind group pins its binding size, and a struct
 * that grows without them is six silent minBindingSize failures. */
inline constexpr uint64_t kAtmoUniformBytes = 12 * 4 * sizeof(float);

static const char *kAtmoCommon = R"(
const PI = 3.14159265358979;
const groundRadiusMM = 6.360;
const atmosphereRadiusMM = 6.460;
const rayleighScatteringBase = vec3f(5.802, 13.558, 33.1);
const mieScatteringBase = 3.996;
const mieAbsorptionBase = 4.4;
const ozoneAbsorptionBase = vec3f(0.650, 1.881, 0.085);
struct Atmo {
  /* camPosMm.xyz = real ECEF / 1e6; camPosMm.w = the REAL WGS84 ground radius under it, in Mm */
  camPosMm : vec4f, sunDir : vec4f, up : vec4f, sunTan : vec4f, side : vec4f,
  camRight : vec4f, camUp : vec4f, camFwd : vec4f, params : vec4f,
  moonDir : vec4f,   /* xyz = ECEF dir to moon, w = illuminated phase fraction */
  skyExtra : vec4f,  /* x = daylight factor (0 night..1 day), y = EVS gate (1=photo), z = spare, w = moon radius */
  view : vec4f,      /* x = the boresight's NDC shift (Renderer::ViewShiftNdc), y = lowest deck base
                        ASL, zw = this frame's sub-pixel jitter in NDC (render/TemporalJitter.h) */
};
/* THE camera ray, identical in every atmosphere shader: the full-frame tangents plus the same
 * off-centre shift MvpCamRel applies, so sky, sun, moon and cloud sit exactly where the terrain does.
 * The jitter is subtracted for the same reason it is added to the projection — the ray belonging to
 * a pixel is the one the jittered frustum sends through it. */
fn camRay(A : Atmo, ndc : vec2f) -> vec3f {
  return normalize(A.camFwd.xyz + (ndc.x - A.view.z) * A.params.x * A.params.y * A.camRight.xyz
                                + (ndc.y - A.view.x - A.view.w) * A.params.x * A.camUp.xyz);
}
/* THE camera position IN THE SCATTERING MODEL. The model stands on a 6360 km sphere; the camera
 * arrives in real ECEF, where the radius runs 6357-6378 km. Feeding the raw ECEF radius to a march
 * that measures altitude as (r - 6360) puts a pedestrian eleven kilometres up — measured: 2.15 EV
 * off the zenith, because exp(-11/8) of the air column is left above him. Rebase, do not truncate. */
fn atmoPos(A : Atmo) -> vec3f {
  let r = length(A.camPosMm.xyz);
  return A.camPosMm.xyz * ((groundRadiusMM + max(r - A.camPosMm.w, 0.0)) / max(r, 1.0e-6));
}
struct ScatterVals { rayleigh : vec3f, mie : f32, extinction : vec3f };
fn getScatteringValues(pos : vec3f) -> ScatterVals {
  let altitudeKM = (length(pos) - groundRadiusMM) * 1000.0;
  let rayleighDensity = exp(-altitudeKM / 8.0);
  let mieDensity = exp(-altitudeKM / 1.2);
  var s : ScatterVals;
  s.rayleigh = rayleighScatteringBase * rayleighDensity;
  s.mie = mieScatteringBase * mieDensity;
  let mieAbs = mieAbsorptionBase * mieDensity;
  let ozone = ozoneAbsorptionBase * max(0.0, 1.0 - abs(altitudeKM - 25.0) / 15.0);
  s.extinction = s.rayleigh + vec3f(s.mie + mieAbs) + ozone;
  return s;
}
fn rayIntersectSphere(ro : vec3f, rd : vec3f, rad : f32) -> f32 {
  let b = dot(ro, rd);
  let c = dot(ro, ro) - rad * rad;
  if (c > 0.0 && b > 0.0) { return -1.0; }
  let disc = b * b - c;
  if (disc < 0.0) { return -1.0; }
  if (disc > b * b) { return -b + sqrt(disc); }
  return -b - sqrt(disc);
}
fn getMiePhase(cosTheta : f32) -> f32 {
  let g = 0.8;
  let num = (1.0 - g * g) * (1.0 + cosTheta * cosTheta);
  let denom = (2.0 + g * g) * pow(1.0 + g * g - 2.0 * g * cosTheta, 1.5);
  return (3.0 / (8.0 * PI)) * num / denom;
}
fn getRayleighPhase(cosTheta : f32) -> f32 { return (3.0 / (16.0 * PI)) * (1.0 + cosTheta * cosTheta); }
fn tLUTuv(pos : vec3f, sunDir : vec3f) -> vec2f {
  let height = length(pos);
  let up = pos / height;
  return vec2f(clamp(0.5 + 0.5 * dot(sunDir, up), 0.0, 1.0),
               clamp((height - groundRadiusMM) / (atmosphereRadiusMM - groundRadiusMM), 0.0, 1.0));
}
)";

} // namespace outshine::Render
#endif

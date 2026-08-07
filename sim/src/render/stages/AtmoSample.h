/* Sky-view LUT sampling, shared by the sky dome and the terrain's aerial perspective. Returns
 * SCENE-REFERRED radiance on the one scale SceneScale.h defines — the same units IrradianceStage
 * integrates for the ground, which is what keeps sky and ground from drifting apart.
 * Requires AtmoCommon.h's `Atmo` struct and SceneScale.h's kSceneExposure in scope already. */
#ifndef ATMOSAMPLE_H
#define ATMOSAMPLE_H

namespace outshine::Render {

static const char *kAtmoSample = R"(
fn skyViewSample(svLUT : texture_2d<f32>, lsamp : sampler, A : Atmo, dir : vec3f) -> vec3f {
  let alt = asin(clamp(dot(dir, A.up.xyz), -1.0, 1.0));
  var az = atan2(dot(dir, A.side.xyz), dot(dir, A.sunTan.xyz));
  if (az < 0.0) { az = az + 2.0 * PI; }
  let coord = sign(alt) * sqrt(abs(alt) / (PI * 0.5));
  let uv = vec2f(az / (2.0 * PI), (coord + 1.0) * 0.5);
  return textureSampleLevel(svLUT, lsamp, uv, 0.0).rgb * kSceneExposure;
}
)";

} // namespace outshine::Render
#endif

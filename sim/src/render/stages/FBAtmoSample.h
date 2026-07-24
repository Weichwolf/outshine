/* FlightBox — sky-view LUT sampling + exposure, shared verbatim by the sky dome (FBSkyStage) and the
 * terrain's aerial perspective (FBTilesStage). Requires FBAtmoCommon.h's `Atmo` struct to already be
 * in scope — concatenate kAtmoCommon before this. */
#ifndef FBATMOSAMPLE_H
#define FBATMOSAMPLE_H

namespace FlightBox {

static const char *kAtmoSample = R"(
const kSkyExposure = 8.0;
fn skyViewSample(svLUT : texture_2d<f32>, lsamp : sampler, A : Atmo, dir : vec3f) -> vec3f {
  let alt = asin(clamp(dot(dir, A.up.xyz), -1.0, 1.0));
  var az = atan2(dot(dir, A.side.xyz), dot(dir, A.sunTan.xyz));
  if (az < 0.0) { az = az + 2.0 * PI; }
  let coord = sign(alt) * sqrt(abs(alt) / (PI * 0.5));
  let uv = vec2f(az / (2.0 * PI), (coord + 1.0) * 0.5);
  return textureSampleLevel(svLUT, lsamp, uv, 0.0).rgb * kSkyExposure;
}
)";

} // namespace FlightBox
#endif

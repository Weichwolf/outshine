/* The receiving half of the cascaded shadow maps: the cascade uniform every lit surface binds and
 * the 3x3 PCF lookup it does. ShadowStage owns the casting half. The atlas is ONE depth texture with
 * the cascades side by side, so a receiver needs one binding and one sampler however many cascades
 * there are, and the cast is one render pass rather than four. */
#ifndef SHADOWSAMPLE_H
#define SHADOWSAMPLE_H

#include <cstdint>
#include <string>

namespace outshine::Render {

/* [SET] 4 x 1024 in a 4096x1024 D32 atlas = 16 MB. Four is what covers a walker's near metres and a
 * town's few hundred at better than 3 cm/texel in the first cascade; a strip atlas keeps the lookup
 * one-dimensional in the cascade index. */
inline constexpr int kShadowCascades = 4;
inline constexpr int kShadowTexels = 1024;
inline constexpr float kShadowNearM = 24.0f;    /* outer radius of cascade 0 */
inline constexpr float kShadowFarM = 600.0f;    /* outer radius of the last cascade */

/* one mat4 per cascade + far radii + params */
inline constexpr int kShadowUniFloats = kShadowCascades * 16 + 8;

/* The shadow pass uses a PLAIN [0,1] ortho depth (0 at the light's near plane), not the scene's
 * reversed-Z: a comparison sampler carries exactly one compare function, and Less is the one that
 * reads naturally for "nearer to the light than the caster". */
inline std::string ShadowSampleWGSL(void) {
  return std::string(R"(
struct Csm {
  m0 : mat4x4f, m1 : mat4x4f, m2 : mat4x4f, m3 : mat4x4f,
  far : vec4f,   /* outer radius of each cascade, metres from the camera */
  /* x = 1/cascades (a cascade's u width in the atlas), y = depth bias in ortho clip units,
   * z = cascade-0 world metres per shadow texel, w = 1 when the atlas holds anything */
  par : vec4f,
};

fn csmClip(C : Csm, i : i32, rel : vec3f) -> vec4f {
  if (i == 0) { return C.m0 * vec4f(rel, 1.0); }
  if (i == 1) { return C.m1 * vec4f(rel, 1.0); }
  if (i == 2) { return C.m2 * vec4f(rel, 1.0); }
  return C.m3 * vec4f(rel, 1.0);
}

/* rel = camera-relative ECEF position of the fragment, nrmC = its normal, sunC = towards the sun.
 * Returns the unshadowed fraction of the direct beam. */
fn csmSunVis(shMap : texture_depth_2d, shSamp : sampler_comparison, C : Csm,
             rel : vec3f, nrmC : vec3f, sunC : vec3f) -> f32 {
  if (C.par.w < 0.5) { return 1.0; }
  let dist = length(rel);
  if (dist >= C.far.w) { return 1.0; }
  var ci = 3;
  if (dist < C.far.x) { ci = 0; }
  else if (dist < C.far.y) { ci = 1; }
  else if (dist < C.far.z) { ci = 2; }
  /* Normal offset rather than depth bias alone: it moves the lookup off the surface by about one
   * cascade texel, which is what removes acne on grazing lit slopes without the peter-panning a
   * constant bias large enough to do the same would cost. */
  let texelM = C.par.z * pow()" + std::to_string((double)(kShadowFarM / kShadowNearM)) +
         R"(, f32(ci) / )" + std::to_string((double)(kShadowCascades - 1)) + R"();
  let offRel = rel + nrmC * (texelM * (1.5 + 2.0 * (1.0 - max(dot(nrmC, sunC), 0.0))));
  let cl = csmClip(C, ci, offRel);
  let ndc = cl.xyz / cl.w;
  if (abs(ndc.x) > 1.0 || abs(ndc.y) > 1.0 || ndc.z < 0.0 || ndc.z > 1.0) { return 1.0; }
  let uw = C.par.x;
  let u0 = (ndc.x * 0.5 + 0.5) * uw + f32(ci) * uw;
  let v0 = 0.5 - ndc.y * 0.5;
  let refZ = ndc.z - C.par.y;
  let du = uw / )" + std::to_string((double)kShadowTexels) + R"(;
  let dv = 1.0 / )" + std::to_string((double)kShadowTexels) + R"(;
  var acc = 0.0;
  for (var jj = -1; jj <= 1; jj = jj + 1) {
    for (var ii = -1; ii <= 1; ii = ii + 1) {
      acc = acc + textureSampleCompareLevel(shMap, shSamp,
              vec2f(u0 + f32(ii) * du, v0 + f32(jj) * dv), refZ);
    }
  }
  return acc * (1.0 / 9.0);
}
)");
}

} // namespace outshine::Render
#endif

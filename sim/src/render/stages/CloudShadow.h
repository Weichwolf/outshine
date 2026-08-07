/* WHAT A LIT SURFACE ASKS THE CLOUDS: how much of the beam a deck of the declared cover lets through
 * at this altitude. ONE answer, the AREA MEAN — there is no cast cloud shadow.
 *
 * There was one, and it went out by decision: the local variant sampled the coverage field where the
 * beam actually pierces the deck, six kilometres downsun at an 11 deg sun, and drew moving patterns
 * from a cloud field whose own silhouette the sim-critic measured as a thresholded SDF. A shadow can
 * be no better than the body casting it, so the body comes first and the shadow last.
 *
 * Requires CloudDensityConstsWGSL() + kCloudDensityWGSL (CloudDeck, cloudCoverage) in scope. A
 * textual splice like AtmoCommon.h — never compiled alone. */
#ifndef CLOUDSHADOW_H
#define CLOUDSHADOW_H

#include <cstdint>

namespace outshine::Render {

/* 5 vec4 header + 3 decks of 16 tight floats. Every bind group pins its binding size, so the number
 * lives once and Renderer::WriteCloudSky is the only writer. */
inline constexpr int kCloudSkyFloats = 5 * 4 + 3 * 16;
inline constexpr uint64_t kCloudSkyBytes = (uint64_t)kCloudSkyFloats * sizeof(float);

static const char *kCloudShadowWGSL = R"(
struct CloudSkyU {
  axE : vec4f,   /* xyz = ECEF east axis at the WORLD anchor, w = camera east offset from it (m) */
  axN : vec4f,   /* xyz = ECEF north axis, w = camera north offset (m) */
  p0  : vec4f,   /* x = ground radius (Mm), y = march quality, z = haze sigma0 (1/m), w = camera alt (m) */
  p1  : vec4f,   /* xy = unit sun direction in the (east,north) plane, z = sun.up, w = cot(sun elevation) */
  tau : vec4f,   /* xyz = per deck, optical depth of the sun's slant path through the whole column */
  deck : array<CloudDeck, 3>,
};

fn cloudDeckThru(d : CloudDeck, tau : f32, cover : f32, altM : f32) -> f32 {
  if (cover <= 0.0) { return 1.0; }
  let frac = clamp((d.topM - altM) / max(d.topM - d.baseM, 1.0), 0.0, 1.0);
  return (1.0 - cover) + cover * exp(-tau * frac);
}

fn cloudMeanThru(S : CloudSkyU, rel : vec3f, upv : vec3f) -> f32 {
  let altM = S.p0.w + dot(rel, upv);
  return cloudDeckThru(S.deck[0], S.tau.x, S.deck[0].cover, altM)
       * cloudDeckThru(S.deck[1], S.tau.y, S.deck[1].cover, altM)
       * cloudDeckThru(S.deck[2], S.tau.z, S.deck[2].cover, altM);
}
)";

} // namespace outshine::Render
#endif

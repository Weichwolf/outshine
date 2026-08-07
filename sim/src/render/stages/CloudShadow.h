/* WHAT A LIT SURFACE ASKS THE CLOUDS. Two answers out of ONE field — the same CloudDensity.h the
 * march draws, from the same uniform buffer, so a shadow cannot land anywhere but under its cloud:
 *
 *   cloudSunThru   the LOCAL direct beam. DeckSunTransmittance with the deck's area-mean `cover`
 *                  replaced by the coverage actually standing between this point and the sun.
 *   cloudMeanThru  the AREA MEAN of the same expression, which is what the deck's diffuse
 *                  re-emission is: standing in a sunbeam you still see the lit underside of the
 *                  whole deck, so that term may not follow the local ray.
 *
 * The mean of the first over the plane IS the second, because CloudCalibrate makes `cover` an area
 * fraction (core/CloudDensity.h) — one quantity at two scales, not two models.
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

/* WHERE the beam pierces the deck, and it is the whole difference between a shadow and a tint: a
 * 1 200 m base under an 11 deg sun throws its shadow six kilometres downsun, so sampling the field
 * straight overhead would draw a pattern that belongs to a cloud nobody can see. */
fn cloudLocalCover(d : CloudDeck, S : CloudSkyU, eastM : f32, northM : f32, altM : f32) -> f32 {
  if (d.cover <= 0.0 || altM >= d.topM) { return 0.0; }
  let zC = 0.5 * (max(d.baseM, altM) + d.topM);
  let horiz = (zC - altM) * S.p1.w;
  return cloudCoverage(d, eastM + S.p1.x * horiz, northM + S.p1.y * horiz).coverage;
}

/* `rel` is camera-relative ECEF metres, `upv` the unit geodetic up at the fragment. */
fn cloudSunThru(S : CloudSkyU, rel : vec3f, upv : vec3f) -> f32 {
  let eastM = S.axE.w + dot(rel, S.axE.xyz);
  let northM = S.axN.w + dot(rel, S.axN.xyz);
  let altM = S.p0.w + dot(rel, upv);
  return cloudDeckThru(S.deck[0], S.tau.x, cloudLocalCover(S.deck[0], S, eastM, northM, altM), altM)
       * cloudDeckThru(S.deck[1], S.tau.y, cloudLocalCover(S.deck[1], S, eastM, northM, altM), altM)
       * cloudDeckThru(S.deck[2], S.tau.z, cloudLocalCover(S.deck[2], S, eastM, northM, altM), altM);
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

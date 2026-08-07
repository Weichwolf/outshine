/* THE exposure, in one place. Every shader that produces scene radiance works in the units the
 * atmosphere LUTs are in (top-of-atmosphere solar irradiance = 1) and applies THIS constant on the
 * way out — sky, ground, buildings, blades and haze alike. Two independently fitted scales is
 * what put zenith sky and sunlit ground 2.5-3.6 EV apart; there is now only one number to move.
 * Spliced ahead of AtmoSample.h and SurfaceLight.h, both of which require it. */
#ifndef SCENESCALE_H
#define SCENESCALE_H

namespace outshine::Render {

static const char *kSceneScaleWGSL = R"(
/* DERIVED from the measured frame, not tuned by eye: at sun elevation 39.1 deg the model's own
 * irradiances give a horizontal E_total of 0.600 (measured: walk/irradiance, sunDirectNormalY
 * 0.844 x sin 39.15 deg + skyDiffuseHorizY 0.0671), so ground of reflectance 0.15 has scene radiance
 * 0.15 * 0.600 / PI = 0.0287. Placing that at ACES input 0.32 — the value whose sRGB output is 0.70,
 * mid-frame for a sunlit surface — needs 0.32 / 0.0287 = 11.2, taken as 11.0. */
const kSceneExposure : f32 = 11.0;
const kInvPi : f32 = 0.31830988618;
)";

} // namespace outshine::Render
#endif

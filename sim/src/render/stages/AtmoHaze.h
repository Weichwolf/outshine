/* THE air between the eye and everything else — one definition, every consumer. The cloud deck and the
 * terrain both dissolve into it, and if it existed twice the two would dissolve differently; the deck
 * would hang in a haze the mountain under it does not see. So the constants, the extinction law and the
 * colour the far field converges to live HERE and are spliced into both shaders, exactly as
 * core/CloudDensity.h is the one cloud field for the picture and for a sensor.
 *
 * Weather-driven, not tabulated: sigma0 comes from the visibility the weather sample REPORTS
 * (Koschmieder). Only the part of it that today's weather CANNOT change — clean air's own molecular
 * scattering — is a table value, and it is kAtmoCommon's, the sky's. The C++ half is
 * here too because the terrain needs the deck's sun transmittance as a number, and because
 * `gpu_native --cloudcheck` measures the two halves against each other. Spec: doc/render/clouds.md. */
#ifndef ATMOHAZE_H
#define ATMOHAZE_H

#include <cmath>
#include <cstdio>
#include <string>
#include "CloudDensity.h"

namespace outshine::Render {

/* ln(1/0.02): the 2 % contrast threshold that DEFINES meteorological visual range, so sigma0 =
 * kKoschmieder / visibility is a derivation and not a fit. */
constexpr float kKoschmieder = 3.912f;

/* TWO scale heights, because the reported visibility is a mixture of two gases with two profiles.
 *   - molecular (Rayleigh): 8 000 m, the ISA density scale height R*T/(M*g) = 8 434 m at 288 K, whose
 *     standard exponential fit over the mass-bearing 0..30 km is 8 km (Bucholtz, Appl. Opt. 34(15),
 *     1995). Air scatters in proportion to its own density, so this one is not a choice.
 *   - aerosol (Mie): 1 200 m. Boundary-layer aerosol is confined to the mixed layer and thins ~6.7x
 *     faster than the air carrying it (Elterman's measured attenuation profiles, AFCRL 1968).
 * BOTH are already in this renderer: kAtmoCommon's getScatteringValues uses exp(-h/8.0) and
 * exp(-h/1.2), from Bruneton & Neyret, "Precomputed Atmospheric Scattering" (EGSR 2008). The haze
 * therefore thins exactly like the sky it fades into instead of on its own authority.
 *
 * One constant did NOT survive: a single 8 km scale height for the whole reported extinction. It is
 * the density scale height, so it thins the AEROSOL — 93 % of a 24 km visibility — at the molecular
 * rate, and the measured cost of that was a white-out (doc/render/clouds.md, Gap 5.7). */
constexpr float kHazeScaleRM = 8000.0f;
constexpr float kHazeScaleAM = 1200.0f;

/* Molecular volume scattering at sea level per RGB channel in 1/Mm, for (680, 550, 440) nm — the same
 * three numbers kAtmoCommon binds to `rayleighScatteringBase` and builds the sky LUT from (Bruneton &
 * Neyret 2008, Table 1). Their ratios are the lambda^-4 law exactly: 13.558/5.802 = (680/550)^4 and
 * 33.1/13.558 = (550/440)^4. The WGSL half reads kAtmoCommon's constant DIRECTLY and a const_assert
 * ties this copy to it, so the picture and this mirror cannot drift apart. */
constexpr float kRayleighBaseRGB[3] = {5.802f, 13.558f, 33.1f};
constexpr float kSigmaRayleigh550 = kRayleighBaseRGB[1] * 1.0e-6f;   /* 1/Mm -> 1/m */

/* Floor on the sun's elevation cosine in every Beer path (march and terrain alike): at grazing sun the
 * slant path through a deck diverges, and one floor keeps deck and ground on the same light. [SET] */
constexpr float kMinSunUp = 0.20f;

inline float HazeSigma0(float visibilityM) {
  return kKoschmieder / (visibilityM > 1.0f ? visibilityM : 1.0f);
}

/* THE SPLIT RULE, and it has no free parameter. Clean air's molecular extinction is a constant of
 * nature (kSigmaRayleigh550 => a Rayleigh-limited visual range of 3.912/1.3558e-5 = 288 km), so it is
 * the molecular part that is fixed and the AEROSOL that carries whatever the weather reports on top of
 * it. The two sum to sigma0 exactly at z = 0 and 550 nm, i.e. the split never changes the visual range
 * that was reported — it only decides how the column thins with height and how it colours. Report a
 * visibility longer than clean air allows and the aerosol term is simply zero, which is the right
 * limit rather than a clamp bolted on. */
inline float HazeSigmaRayleigh(float sigma0) {
  return sigma0 < kSigmaRayleigh550 ? sigma0 : kSigmaRayleigh550;
}
inline float HazeSigmaAerosol(float sigma0) {
  return sigma0 > kSigmaRayleigh550 ? sigma0 - kSigmaRayleigh550 : 0.0f;
}

/* `zEffM` is the MEAN altitude of the sight line — the same approximation on both sides of the seam.
 * The three-argument form is the PHOTOPIC (550 nm) face, which is the wavelength Koschmieder's
 * definition is about; the four-argument form is what the picture draws, and its green channel is the
 * three-argument one by construction. */
inline float HazeOpticalDepth(float sigma0, float zEffM, float distM) {
  const float z = zEffM > 0.0f ? zEffM : 0.0f;
  return (HazeSigmaRayleigh(sigma0) * std::exp(-z / kHazeScaleRM)
        + HazeSigmaAerosol(sigma0) * std::exp(-z / kHazeScaleAM)) * distM;
}
inline void HazeOpticalDepth(float sigma0, float zEffM, float distM, float rgb[3]) {
  const float z = zEffM > 0.0f ? zEffM : 0.0f;
  const float mol = HazeSigmaRayleigh(sigma0) / kSigmaRayleigh550 * std::exp(-z / kHazeScaleRM);
  const float aer = HazeSigmaAerosol(sigma0) * std::exp(-z / kHazeScaleAM);
  for (int i = 0; i < 3; i++) rgb[i] = (kRayleighBaseRGB[i] * 1.0e-6f * mol + aer) * distM;
}
inline float HazeTransmittance(float sigma0, float zEffM, float distM) {
  return std::exp(-HazeOpticalDepth(sigma0, zEffM, distM));
}
inline void HazeTransmittance(float sigma0, float zEffM, float distM, float rgb[3]) {
  HazeOpticalDepth(sigma0, zEffM, distM, rgb);
  for (int i = 0; i < 3; i++) rgb[i] = std::exp(-rgb[i]);
}

/* Optical depth toward the sun through a WHOLE deck, mean-field: the deck's own analytic profile with
 * the erosion at its mean — literally CloudDensity's fully band-limited branch (ErodeFlat = 1), i.e.
 * this deck without its detail, which is what belongs in a quantity that is an average over the whole
 * sky anyway. The column is evaluated at strength c = Cover; the answer barely depends on that choice,
 * because an opaque deck has sigma*thickness ~ 20 and its exponential is zero for any plausible column
 * (MEASURED: doc/render/clouds.md). */
inline float DeckSunOpticalDepth(const CloudDeckParams &d, float sunUp) {
  if (d.Cover <= 0.0f) return 0.0f;
  constexpr int kNodes = 16;   /* midpoint rule over the column; the profile is smooth and analytic */
  float col = 0.0f;
  for (int i = 0; i < kNodes; i++) {
    const float h = ((float)i + 0.5f) / (float)kNodes;
    const float dens = CloudShape(d.Cover, h)
                     - kCloudErodeMean * d.Erosion * CloudMix(kCloudErodeBase, 1.0f, h);
    if (dens > 0.0f) col += dens;
  }
  col /= (float)kNodes;
  return d.SigmaPerM * d.ThicknessM() * col / (sunUp > kMinSunUp ? sunUp : kMinSunUp);
}

/* How much of the deck still stands above `altM`: 1 entirely below it, 0 at or above its top. */
inline float DeckFrac(const CloudDeckParams &d, float altM) {
  const float thick = d.ThicknessM() > 1.0f ? d.ThicknessM() : 1.0f;
  const float t = (d.TopM - altM) / thick;
  return t < 0.0f ? 0.0f : (t > 1.0f ? 1.0f : t);
}

/* How much DIRECT sunlight that leaves. Two terms, and the first one carries the result: `Cover` is
 * calibrated to be an AREA FRACTION (CloudCalibrate, measured by --cloudcheck), so 1 - Cover of the
 * sun rays reach the point having missed the deck entirely and the rest are attenuated. A statistical
 * scalar, deliberately not a shadow map. */
inline float DeckSunTransmittance(float tau, float cover, float frac) {
  if (cover <= 0.0f) return 1.0f;
  return (1.0f - cover) + cover * std::exp(-tau * frac);
}

inline std::string HazeConstsWGSL(void) {
  char buf[256];
  snprintf(buf, sizeof buf,
           "const kKoschmieder : f32 = %.9g;\nconst kHazeScaleRM : f32 = %.9g;\n"
           "const kHazeScaleAM : f32 = %.9g;\nconst kSigmaRayleigh550 : f32 = %.9g;\n"
           "const kMinSunUp : f32 = %.9g;\n",
           (double)kKoschmieder, (double)kHazeScaleRM, (double)kHazeScaleAM,
           (double)kSigmaRayleigh550, (double)kMinSunUp);
  return std::string(buf);
}

/* Requires AtmoCommon.h (`Atmo`, PI) and AtmoSample.h (`skyViewSample`) plus SceneScale.h (kSceneExposure) in scope, and
 * the constants above. Textual splice — never compiled alone. */
static const char *kHazeWGSL = R"(
/* 1/Mm -> 1/m of kAtmoCommon's own molecular coefficients: the haze does not get a second opinion on
 * what air is made of. The assert is the seam to the C++ mirror above — a compile-time gate, so the
 * two halves cannot drift the way two hand-copied numbers would. */
const kHazeRayleighRGB : vec3f = rayleighScatteringBase * 1.0e-6;
const_assert abs(kHazeRayleighRGB.g - kSigmaRayleigh550) < 1.0e-12;

/* The far field is bluer than the near field because the MOLECULAR term takes blue out ~5.7x faster
 * than red (lambda^-4 across kHazeRayleighRGB) while the aerosol term is grey to within a few percent.
 * So the colour of distance is the physics of the two terms, not a tint. */
fn hazeOpticalDepth3(sigma0 : f32, zEffM : f32, distM : f32) -> vec3f {
  let zH = max(zEffM, 0.0);
  let molFrac = min(sigma0, kSigmaRayleigh550) / kSigmaRayleigh550 * exp(-zH / kHazeScaleRM);
  let aerSig = max(sigma0 - kSigmaRayleigh550, 0.0) * exp(-zH / kHazeScaleAM);
  return (kHazeRayleighRGB * molFrac + vec3f(aerSig)) * distM;
}
fn hazeTransmittance3(sigma0 : f32, zEffM : f32, distM : f32) -> vec3f {
  return exp(-hazeOpticalDepth3(sigma0, zEffM, distM));
}
/* The photopic face of the same law, literally its green channel — what Koschmieder's visual range is
 * defined at, and the scalar `--cloudcheck` measures the C++ mirror against. */
fn hazeOpticalDepth(sigma0 : f32, zEffM : f32, distM : f32) -> f32 {
  return hazeOpticalDepth3(sigma0, zEffM, distM).g;
}
fn hazeTransmittance(sigma0 : f32, zEffM : f32, distM : f32) -> f32 {
  return exp(-hazeOpticalDepth(sigma0, zEffM, distM));
}
/* One deck as (baseM, topM, sunOpticalDepth, cover) — DeckSunTransmittance above, evaluated where
 * the fragment actually is. A ridge that stands INSIDE the deck keeps the share of the sun the part
 * of the deck still above it lets through, and one that pokes out the top is in full sunlight. */
fn deckSunThru(dk : vec4f, altM : f32) -> f32 {
  if (dk.w <= 0.0) { return 1.0; }
  let frac = clamp((dk.y - altM) / max(dk.y - dk.x, 1.0), 0.0, 1.0);
  return (1.0 - dk.w) + dk.w * exp(-dk.z * frac);
}
/* The colour the far field converges to as transmittance -> 0, and it has to be the colour the SKY
 * pass paints in that direction or the horizon grows an edge. The sky pass is skyViewSample and
 * nothing else, so this is skyViewSample and nothing else.
 *
 * BELOW the horizon the sky-view LUT is asked a different question than the one we need: it marches
 * single scattering only to the ground intersection, through a MOLECULAR atmosphere whose extinction
 * near the surface is ~20x weaker than a 24 km-visibility aerosol's — that segment is short, blue and
 * dark, and a downward view fading into it goes navy instead of white (MEASURED: the first render of
 * this function). The haze's own source function is not in that LUT and would need a second one. The
 * colour that IS in it is the HORIZON radiance in the same azimuth, where the LUT's own sight line is
 * saturated by the whole atmosphere — the classic fog colour, and the exact value at the seam where
 * terrain meets sky. So a below-horizon direction is projected onto the horizon; above it, nothing
 * changes, and a peak against the sky still fades into the sky it stands in front of. */
fn hazeInscatter(svLUT : texture_2d<f32>, lsamp : sampler, A : Atmo, dir : vec3f) -> vec3f {
  let hazeDir = normalize(dir - A.up.xyz * min(dot(dir, A.up.xyz), 0.0));
  return skyViewSample(svLUT, lsamp, A, hazeDir);
}
)";

} // namespace outshine::Render
#endif

/* THE cloud density function — one formula, two evaluators. This header is the C++ half; the WGSL half
 * (render/stages/CloudDensityWGSL.h) is a literal transliteration whose CONSTANTS are emitted from the
 * ones below, so a number can never drift between the picture and a measurement. "How much cloud is at
 * this point" is the same question a future IR/radar sensor asks, which is why the model lives in core/
 * and not in the renderer.
 *
 * Separable per deck: a 2-D wind-advected coverage FBM (optionally stretched and sheared along the wind
 * = cirrus fibres) x an analytic vertical profile, minus a procedural 3-D erosion term. No textures, no
 * bakes -- the field is a pure function of (east, north, height fraction), which is exactly what makes
 * the two evaluators comparable at all. */
#ifndef CLOUDDENSITY_H
#define CLOUDDENSITY_H

#include <cstdint>
#include <cmath>
#include "WeatherProvider.h"

namespace outshine {

/* ---- Shape constants. Every one of them is [SET] (a look decision), which is why they are named,
 * collected, and emitted verbatim into the shader instead of being typed twice. */
constexpr int   kCloudOctaves   = 4;      /* coverage FBM octaves */
constexpr float kCloudLacunarity= 2.0f;
constexpr float kCloudGain      = 0.5f;
constexpr float kCloudShear     = 0.30f;  /* octave i is skewed by i*shear*across-wind -> hooked streaks */
/* Each octave's frame is rotated by a 3-4-5 angle before the next one. Without it every octave shares
 * the same lattice and the field reads as an axis-aligned checkerboard (measured: the first render of
 * this function). cos/sin of that angle are exact in binary floating point, so the two evaluators stay
 * bit-comparable. */
constexpr float kCloudRotC      = 0.8f;
constexpr float kCloudRotS      = 0.6f;
constexpr float kCloudWarpFreq  = 0.35f;  /* domain-warp noise frequency, relative to the feature size */
constexpr float kCloudWarpCross = 0.50f;  /* cross-wind share of the warp (mares' tails bend, not smear) */
/* The FBM's own distribution, MEASURED over the sample set of `gpu_native --cloudcheck` (4 octaves,
 * gain 0.5, normalised): the remap threshold is placed against it so that "0.76 cover" really is 76 %
 * of the area, instead of being an arbitrary level on an arbitrary field. */
constexpr float kCloudFbmMean   = 0.5007f;   /* measured over 40 000 samples, `--cloudcheck` */
constexpr float kCloudFbmSigma  = 0.1331f;
constexpr float kCloudLogitK    = 1.702f;  /* logistic approximation of the normal quantile function */
constexpr float kCloudRemapHard = 0.35f;  /* remap half-width in SIGMAS at cover -> 0: discrete elements */
constexpr float kCloudRemapSoft = 2.20f;  /* remap half-width in SIGMAS at cover -> 1: closed veil */
constexpr float kCloudProfBase  = 0.18f;  /* lower fraction of the column the profile ramps in over */
constexpr float kCloudProfTop   = 0.60f;  /* upper fraction the column ramps out over */
/* A column reaches HIGHER where the 2-D field is stronger: the same separable field then gives a bumpy
 * deck TOP instead of a mirror-flat lid, at no extra tap. kCloudTopMin is the fraction of the deck a
 * bare-minimum column fills. */
constexpr float kCloudTopMin    = 0.45f;
constexpr int   kCloudErodeOct  = 2;      /* erosion octaves (rotated, like the coverage FBM) */
constexpr float kCloudErodeFreq = 10.0f;  /* erosion noise frequency, relative to the feature size */
constexpr float kCloudErodeVert = 3.0f;   /* erosion cycles across the deck height */
constexpr float kCloudErodeBase = 0.35f;  /* erosion weight at the deck base (top-biased, Nubis) */
/* The erosion FBM's OWN mean — the value a fully band-limited erosion term collapses to (see
 * CloudDeckParams::ErodeFlat). MEASURED by `gpu_native --cloudcheck`; it is 0.5 by construction
 * (CloudNoise3 is centred on 0.5 and the octave sum is normalised), which is exactly why it can be
 * faded toward WITHOUT changing the mean density the way fading the amplitude to zero does. */
constexpr float kCloudErodeMean = 0.5f;

/* ---- One deck. Metres and metres/second throughout; horizontal position is measured in the tangent
 * plane of a fixed anchor, so the field stands still in the world while the aircraft moves. */
struct CloudDeckParams {
  float BaseM = 0.0f, TopM = 0.0f;      /* ASL */
  float Cover = 0.0f;                   /* 0..1; 0 = this deck does not exist this frame */
  float DriftEastM = 0.0f, DriftNorthM = 0.0f;   /* wind x elapsed time — the advection */
  float WindDirE = 1.0f, WindDirN = 0.0f;        /* unit wind direction = the streak axis */
  float Stretch = 1.0f;                 /* along-wind domain stretch (1 = isotropic) */
  float FeatureM = 16000.0f;            /* size of the largest FBM feature */
  float Warp = 0.0f;                    /* domain-warp amplitude in feature units */
  float Erosion = 0.0f;                 /* 3-D erosion weight */
  float SigmaPerM = 0.0f;               /* extinction at density 1, 1/m */
  /* DERIVED from Cover by CloudCalibrate — carried rather than recomputed so the shader does not
   * evaluate a quantile function per sample, and so both evaluators read the same two numbers. */
  float RemapEdge = 1.0f, RemapWidth = 0.1f;
  /* BAND-LIMIT of the erosion octaves, 0 = full detail, 1 = flat. It fades the erosion toward its own
   * MEAN, not toward zero: erosion SUBTRACTS density, so fading the amplitude out would thicken and
   * brighten the deck wherever the filter bites (measured: +3.5 % deck luminance). This is the mip
   * level of a procedural field, and it belongs in the shared function because the answer "how much
   * cloud is on this ray" depends on the footprint of whoever is asking. */
  float ErodeFlat = 0.0f;
  float Pad1 = 0.0f;                    /* 16 floats = 64 B: a uniform array element strides by 16 */

  float ThicknessM(void) const { return TopM - BaseM; }
};

/* The three étages plus the two atmospheric numbers the march needs from the same weather sample. */
struct CloudSky {
  CloudDeckParams Deck[3];            /* 0 = low, 1 = mid, 2 = high (cirrus) */
  float VisibilityM = 100000.0f;        /* surface visibility -> the haze the far field dissolves into */
  /* WHERE the horizontal field's origin sits. The density function's east/north are tangent-plane
   * metres from a point, and WHICH point decides where the holes are: anchoring at the observer would
   * nail the field to the aircraft (it could never fly out from under a hole), and anchoring each
   * observer at its own position would let two aircraft disagree about one sky. So the anchor travels
   * WITH the sample, and whoever resolves the weather for a whole cast gives them all the same one.
   * The renderer pins its own ECEF anchor at the first frame and ignores this
   * pair — a stated phase offset between picture and sensor, not a second field. */
  double AnchorLatDeg = 0.0, AnchorLonDeg = 0.0;
  [[nodiscard]] bool Any(void) const { return Deck[0].Cover > 0.0f || Deck[1].Cover > 0.0f || Deck[2].Cover > 0.0f; }
};

/* ---- The evaluators. Deliberately hand-written instead of std:: equivalents: `smoothstep` and integer
 * hashing must be bit-comparable against WGSL, and the built-ins are not specified to the same rule. */
inline float CloudSmooth(float e0, float e1, float x) {
  float t = (x - e0) / (e1 - e0);
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return t * t * (3.0f - 2.0f * t);
}
inline float CloudMix(float a, float b, float t) { return a + (b - a) * t; }

/* Integer hash. uint32 wraps identically in C++ and WGSL, which is the whole reason the noise is hashed
 * rather than sampled from a texture. */
inline uint32_t CloudHash2(int32_t ix, int32_t iy, uint32_t seed) {
  uint32_t h = (uint32_t)ix * 0x27d4eb2du + (uint32_t)iy * 0x9e3779b1u + seed * 0x85ebca6bu;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  h *= 0x297a2d39u;
  h ^= h >> 15;
  return h;
}
inline uint32_t CloudHash3(int32_t ix, int32_t iy, int32_t iz, uint32_t seed) {
  uint32_t h = (uint32_t)ix * 0x27d4eb2du + (uint32_t)iy * 0x9e3779b1u + (uint32_t)iz * 0x165667b1u
             + seed * 0x85ebca6bu;
  h ^= h >> 15;
  h *= 0x2c1b3c6du;
  h ^= h >> 12;
  h *= 0x297a2d39u;
  h ^= h >> 15;
  return h;
}

/* GRADIENT (Perlin) noise, not value noise, and a quintic (C2) fade. Value noise puts its extrema ON
 * the lattice points, so a hard coverage threshold over it draws the lattice as square cells.
 *
 * The GRADIENT SET matters just as much: four diagonal (+-1,+-1) gradients still draw an axis-aligned
 * rectangular grid once the coverage remap amplifies the finest octave (measured, three renders in).
 * These are Perlin's own sets — 8 directions in 2-D, the 12 cube edges in 3-D — which is exactly what
 * they exist for. */
inline float CloudFade(float t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

inline float CloudGrad2(int32_t ix, int32_t iy, uint32_t seed, float dx, float dy) {
  const uint32_t k = CloudHash2(ix, iy, seed) & 7u;
  float gx, gy;
  if (k < 4u) {
    const float sgn = (k & 1u) ? -1.0f : 1.0f;
    gx = (k & 2u) ? 0.0f : sgn;
    gy = (k & 2u) ? sgn : 0.0f;
  } else {
    gx = (k & 1u) ? -0.7071068f : 0.7071068f;
    gy = (k & 2u) ? -0.7071068f : 0.7071068f;
  }
  return gx * dx + gy * dy;
}
inline float CloudGrad3(int32_t ix, int32_t iy, int32_t iz, uint32_t seed, float dx, float dy, float dz) {
  const uint32_t k = CloudHash3(ix, iy, iz, seed) & 15u;   /* Perlin's improved-noise gradient fold */
  const float u = (k < 8u) ? dx : dy;
  const float v = (k < 4u) ? dy : ((k == 12u || k == 14u) ? dx : dz);
  return ((k & 1u) ? -u : u) + ((k & 2u) ? -v : v);
}

inline float CloudNoise2(float px, float py, uint32_t seed) {
  const float fx = std::floor(px), fy = std::floor(py);
  const int32_t ix = (int32_t)fx, iy = (int32_t)fy;
  const float dx = px - fx, dy = py - fy;
  const float ux = CloudFade(dx), uy = CloudFade(dy);
  const float n00 = CloudGrad2(ix, iy, seed, dx, dy);
  const float n10 = CloudGrad2(ix + 1, iy, seed, dx - 1.0f, dy);
  const float n01 = CloudGrad2(ix, iy + 1, seed, dx, dy - 1.0f);
  const float n11 = CloudGrad2(ix + 1, iy + 1, seed, dx - 1.0f, dy - 1.0f);
  const float n = CloudMix(CloudMix(n00, n10, ux), CloudMix(n01, n11, ux), uy);
  return n + 0.5f;
}
inline float CloudNoise3(float px, float py, float pz, uint32_t seed) {
  const float fx = std::floor(px), fy = std::floor(py), fz = std::floor(pz);
  const int32_t ix = (int32_t)fx, iy = (int32_t)fy, iz = (int32_t)fz;
  const float dx = px - fx, dy = py - fy, dz = pz - fz;
  const float ux = CloudFade(dx), uy = CloudFade(dy), uz = CloudFade(dz);
  const float n000 = CloudGrad3(ix, iy, iz, seed, dx, dy, dz);
  const float n100 = CloudGrad3(ix + 1, iy, iz, seed, dx - 1.0f, dy, dz);
  const float n010 = CloudGrad3(ix, iy + 1, iz, seed, dx, dy - 1.0f, dz);
  const float n110 = CloudGrad3(ix + 1, iy + 1, iz, seed, dx - 1.0f, dy - 1.0f, dz);
  const float n001 = CloudGrad3(ix, iy, iz + 1, seed, dx, dy, dz - 1.0f);
  const float n101 = CloudGrad3(ix + 1, iy, iz + 1, seed, dx - 1.0f, dy, dz - 1.0f);
  const float n011 = CloudGrad3(ix, iy + 1, iz + 1, seed, dx, dy - 1.0f, dz - 1.0f);
  const float n111 = CloudGrad3(ix + 1, iy + 1, iz + 1, seed, dx - 1.0f, dy - 1.0f, dz - 1.0f);
  const float x00 = CloudMix(n000, n100, ux), x10 = CloudMix(n010, n110, ux);
  const float x01 = CloudMix(n001, n101, ux), x11 = CloudMix(n011, n111, ux);
  const float n = CloudMix(CloudMix(x00, x10, uy), CloudMix(x01, x11, uy), uz);
  return n * 0.8f + 0.5f;
}

/* FBM with a per-octave SHEAR along the wind axis: octave i is skewed by i*kCloudShear*(across-wind),
 * so the fine structure leans against the coarse one instead of sitting on top of it. That lean is what
 * reads as a fall streak / hook; without it four isotropic octaves are soft blobs at any stretch. */
inline float CloudFbmShear(float ax, float bx, uint32_t seed) {
  float qa = ax, qb = bx, sum = 0.0f, amp = 1.0f, norm = 0.0f;
  for (int i = 0; i < kCloudOctaves; i++) {
    float sa = qa + kCloudShear * (float)i * qb;
    sum += amp * CloudNoise2(sa, qb, seed + (uint32_t)i * 131u);
    norm += amp;
    amp *= kCloudGain;
    const float ra = qa * kCloudRotC - qb * kCloudRotS;   /* rotate, THEN scale: no shared lattice */
    const float rb = qa * kCloudRotS + qb * kCloudRotC;
    qa = ra * kCloudLacunarity;
    qb = rb * kCloudLacunarity;
  }
  return sum / norm;
}

/* Places the coverage remap against the FBM's measured distribution, so `Cover` is an AREA FRACTION.
 * The logistic approximation of the normal quantile (max error ~0.01 over the useful range) is enough:
 * this is a look mapping, not a statistic. Call it after every change to Cover. */
inline void CloudCalibrate(CloudDeckParams &d) {
  float p = d.Cover;
  if (p < 0.02f) p = 0.02f;
  if (p > 0.98f) p = 0.98f;
  const float z = std::log(p / (1.0f - p)) / kCloudLogitK;   /* ~ the (1-p) quantile, negated */
  d.RemapEdge = kCloudFbmMean - kCloudFbmSigma * z;
  d.RemapWidth = kCloudFbmSigma * CloudMix(kCloudRemapHard, kCloudRemapSoft, d.Cover);
}

/* The separable halves, exposed on their own because the march USES them on their own: the light taps
 * walk the 2-D field (the spec's "2-3 sun taps into the 2D field") and never pay for erosion, and the
 * vertical profile is the closed form the Beer term integrates over.
 *
 * `noiseAB` returns the wind-frame sample coordinate as well, so the erosion term can reuse it instead
 * of recomputing the warp. */
struct CloudSample {
  float Coverage;   /* the remapped 2-D field, [0,1] */
  float A, B;       /* along-wind / across-wind sample coordinate, feature units (warped) */
};

/* `eastM`/`northM` are tangent-plane metres from the sky's anchor. */
inline CloudSample CloudCoverage(const CloudDeckParams &d, float eastM, float northM) {
  CloudSample s{0.0f, 0.0f, 0.0f};
  if (d.Cover <= 0.0f) return s;
  const float inv = 1.0f / d.FeatureM;
  const float x = (eastM - d.DriftEastM) * inv;
  const float y = (northM - d.DriftNorthM) * inv;
  float a = x * d.WindDirE + y * d.WindDirN;      /* along wind */
  float b = -x * d.WindDirN + y * d.WindDirE;     /* across wind */
  a /= d.Stretch;
  if (d.Warp > 0.0f) {
    float w1 = CloudNoise2(a * kCloudWarpFreq + 17.3f, b * kCloudWarpFreq + 5.9f, 7u);
    float w2 = CloudNoise2(a * kCloudWarpFreq - 3.1f, b * kCloudWarpFreq + 23.7f, 13u);
    a += (w1 - 0.5f) * d.Warp;
    b += (w2 - 0.5f) * d.Warp * kCloudWarpCross;
  }
  const float f = CloudFbmShear(a, b, 3u);
  s.Coverage = CloudSmooth(d.RemapEdge - d.RemapWidth, d.RemapEdge + d.RemapWidth, f);
  s.A = a;
  s.B = b;
  return s;
}

/* The analytic column: how much of the deck a column of strength `c` fills, and how the density is
 * distributed over it. `h` is the height fraction in the DECK, 0 = base, 1 = top. */
inline float CloudShape(float c, float h) {
  if (c <= 0.0f) return 0.0f;
  const float top = CloudMix(kCloudTopMin, 1.0f, c);
  const float hn = h / top;
  if (hn >= 1.0f) return 0.0f;
  return c * CloudSmooth(0.0f, kCloudProfBase, hn) * CloudSmooth(1.0f, 1.0f - kCloudProfTop, hn);
}

/* Erosion: the same rotated-octave construction as the coverage FBM, in 3-D. One un-rotated octave is
 * a visible axis-aligned lattice (measured: the second render of this function). */
inline float CloudErosionFbm(float ax, float bx, float h) {
  float qa = ax, qb = bx, qz = h, sum = 0.0f, amp = 1.0f, norm = 0.0f;
  for (int i = 0; i < kCloudErodeOct; i++) {
    sum += amp * CloudNoise3(qa, qb, qz, 41u + (uint32_t)i * 977u);
    norm += amp;
    amp *= kCloudGain;
    const float ra = qa * kCloudRotC - qb * kCloudRotS;
    const float rb = qa * kCloudRotS + qb * kCloudRotC;
    qa = ra * kCloudLacunarity;
    qb = rb * kCloudLacunarity;
    qz *= kCloudLacunarity;
  }
  return sum / norm;
}

/* THE function. `h` is the height fraction inside the deck (0 = base, 1 = top). Returns [0,1] —
 * multiply by SigmaPerM for extinction. */
inline float CloudDensity(const CloudDeckParams &d, float eastM, float northM, float h) {
  const CloudSample s = CloudCoverage(d, eastM, northM);
  if (s.Coverage <= 0.0f) return 0.0f;
  float dens = CloudShape(s.Coverage, h);
  if (d.Erosion > 0.0f && dens > 0.0f) {
    float e = kCloudErodeMean;
    if (d.ErodeFlat < 1.0f) {
      const float raw = CloudErosionFbm(s.A * kCloudErodeFreq, s.B * kCloudErodeFreq, h * kCloudErodeVert);
      e = CloudMix(raw, kCloudErodeMean, d.ErodeFlat);
    }
    dens -= e * d.Erosion * CloudMix(kCloudErodeBase, 1.0f, h);
  }
  if (dens < 0.0f) dens = 0.0f;
  if (dens > 1.0f) dens = 1.0f;
  return dens;
}

/* ---- Deck GEOMETRY from a weather sample. GFS reports cover per étage and ONE ceiling; where the deck
 * actually sits is a modelling decision, so every number here is [SET] and named.
 *
 * The ceiling is the base of the lowest BROKEN-or-more layer, so it is spent on the lowest deck whose
 * own cover is broken AND whose étage band it falls in — a ceiling of 8.6 km reported under 95 % high
 * cloud belongs to the cirrus, not to a stratus deck that is not there. */
constexpr float kCloudLowDefaultBaseM  = 1200.0f;   /* [SET] */
constexpr float kCloudMidDefaultBaseM  = 4200.0f;   /* [SET] */
constexpr float kCloudHighDefaultBaseM = 9000.0f;   /* [SET] — the spec's ~9 km cirrus level */
constexpr float kCloudLowThickM  = 900.0f;          /* [SET] stratocumulus deck */
constexpr float kCloudMidThickM  = 1400.0f;         /* [SET] altostratus/altocumulus */
constexpr float kCloudHighThickM = 500.0f;          /* [SET] "a few hundred metres" (spec) */
/* Étage bands the reported ceiling is allowed to land in [SET]. NOT the WMO étage limits: GFS' "low
 * cloud" diagnostic runs to ~642 hPa (~3.7 km), and a first attempt with the textbook 2.5 km low limit
 * threw away a measured 2 991 m ceiling under 76 % low cloud and put the deck at the 1 200 m default
 * instead. The bands are the reporting layers' own extent, so they overlap. */
constexpr float kCloudBandM[3][2] = {{150.0f, 4000.0f}, {2000.0f, 7500.0f}, {5500.0f, 13000.0f}};
/* Width of the HANDOVER between two étages [SET]. It is the whole answer to "one ceiling, three decks":
 * inside its band a deck takes the ceiling as its base, outside it the next étage up does, and in
 * between both weights slide. Neither of the two obvious rules works — CLAMPING the ceiling into the
 * band leaves the base stuck at the band edge while the reported ceiling walks on (a deck that stops
 * rising), and REJECTING it drops the base to the default in one frame (measured: 2.7 km). 800 m of
 * ceiling change is ~340 km of track at the fixture's own along-corridor gradient (70 m per 30 km), so
 * the handover is a slide of a few metres per second, not an event. */
constexpr float kCloudCeilBlendM = 2000.0f;
/* A deck that is barely there cannot own the ceiling either — and "barely there" has to be a RAMP, not
 * a threshold, or the ownership flips the instant a cover crosses it (which is exactly the defect the
 * predecessor's broken-threshold carried). [SET]: a tenth of the sky. */
constexpr float kCloudCeilExistsFrac = 0.10f;

/* Feature size, stretch, warp, erosion and extinction per étage — the character of each deck [SET].
 * Extinction is chosen from the optical depth it produces over the deck's own thickness: low
 * 0.040/m x 900 m = 36 (opaque), mid 0.030 x 1400 = 42 (opaque), high 0.0060 x 500 m = 3.0 (a
 * translucent veil, which is what a cirrostratus measures). */
constexpr float kCloudFeatureM[3] = {16000.0f, 26000.0f, 12000.0f};
constexpr float kCloudStretch[3]  = {1.0f, 1.35f, 7.0f};     /* cirrus 7:1, inside the spec's 5-10:1 */
constexpr float kCloudWarp[3]     = {0.25f, 0.35f, 0.30f};
constexpr float kCloudErosion[3]  = {0.35f, 0.25f, 0.15f};
constexpr float kCloudSigma[3]    = {0.022f, 0.018f, 0.0060f};
/* The high deck's streak axis and drift are read at the 250 hPa level rather than at its own mid
 * altitude: 250 hPa IS the cirrus level, and it is the level the spec names. */
constexpr double kCloudHighWindSampleM = 10800.0;   /* [SET] ~250 hPa geopotential in mid-latitudes */

/* The advection offset is WRAPPED, and that is a correctness rule rather than tidiness: the sample
 * coordinate is `(east - drift)/FeatureM`, evaluated in f32 by the shader. An unwrapped drift of
 * 1.8e10 m (which is what handing this function a unix timestamp produces) leaves the finest octave's
 * coordinate at ~9e6, where one f32 ulp is a whole lattice cell — the fractional part is gone and the
 * noise degenerates into flat, axis-aligned rectangles. MEASURED: that was exactly the artefact.
 * At the wrap below the finest octave sits near 2e3, where an ulp is 1e-4 of a cell. */
constexpr double kCloudDriftWrapM = 4.0e6;   /* [SET] ~2 days of drift at 20 m/s */

/* Builds the three decks for one point and one instant. `timeS` drives the advection only; the DECKS
 * are sampled at (latDeg,lonDeg) while the horizontal FIELD is measured from (anchorLat,anchorLon) —
 * see CloudSky::AnchorLatDeg. The three-argument overload below anchors at the sample point, which is
 * what a single-observer caller (the renderer) wants. */
inline CloudSky CloudSkyFromWeather(const WeatherProvider &wx, double latDeg, double lonDeg,
                                        double timeS, double anchorLatDeg, double anchorLonDeg) {
  const CloudLayers L = wx.Clouds(latDeg, lonDeg);
  const float cover[3] = {(float)(L.LowPct * 0.01), (float)(L.MidPct * 0.01), (float)(L.HighPct * 0.01)};
  const float defBase[3] = {kCloudLowDefaultBaseM, kCloudMidDefaultBaseM, kCloudHighDefaultBaseM};
  const float thick[3] = {kCloudLowThickM, kCloudMidThickM, kCloudHighThickM};

  CloudSky sky;
  sky.VisibilityM = (float)wx.VisibilityM(latDeg, lonDeg);
  /* WHO OWNS THE ONE REPORTED CEILING — as a WEIGHT per deck, not as a choice between decks, because
   * every choice this rule could make is a discontinuity somewhere. `own[i]` is "this étage's band
   * contains the ceiling" (smooth-edged, kCloudCeilBlendM wide) times "no lower étage that exists has
   * claimed it", so the weights hand over instead of switching. A deck that is not there
   * (cover 0) cannot own a ceiling, and it does not block the deck above it either.
   *
   * The predecessor picked the lowest BROKEN deck and CLAMPED the ceiling into its band. That carried
   * two discontinuities: the base stuck at the band edge once the reported ceiling walked past it, and
   * the choice itself flipped decks the instant a cover crossed the broken threshold. Both are gone —
   * cover now enters only as "exists at all", monotonically. */
  float own[3] = {0.0f, 0.0f, 0.0f};
  float ceilFor[3] = {0.0f, 0.0f, 0.0f};
  if (L.HaveCeiling) {
    float free = 1.0f;   /* how much of the ceiling the étages below have left unclaimed */
    for (int i = 0; i < 3; i++) {
      const float lo = kCloudBandM[i][0], hi = kCloudBandM[i][1];
      float c = (float)L.CeilingM;
      /* The outermost edges SATURATE rather than blend: there is no étage below the lowest or above the
       * highest to hand a ceiling to, so a 100 m fog stays the low deck's base and a 14 km one the
       * cirrus'. Every INNER edge blends, and that is where the sticking used to happen. */
      if (i == 0 && c < lo) c = lo;
      if (i == 2 && c > hi) c = hi;
      ceilFor[i] = c;
      const float w = CloudSmooth(lo - kCloudCeilBlendM, lo, c)
                    * (1.0f - CloudSmooth(hi, hi + kCloudCeilBlendM, c));
      const float exists = CloudSmooth(0.0f, kCloudCeilExistsFrac, cover[i]);
      own[i] = w * exists * free;
      free -= own[i];
    }
  }
  for (int i = 0; i < 3; i++) {
    CloudDeckParams &d = sky.Deck[i];
    d.Cover = cover[i] < 0.0f ? 0.0f : (cover[i] > 1.0f ? 1.0f : cover[i]);
    d.BaseM = CloudMix(defBase[i], ceilFor[i], own[i]);
    d.TopM = d.BaseM + thick[i];
    d.FeatureM = kCloudFeatureM[i];
    d.Stretch = kCloudStretch[i];
    d.Warp = kCloudWarp[i];
    d.Erosion = kCloudErosion[i];
    d.SigmaPerM = kCloudSigma[i];
    const double sampleM = (i == 2) ? kCloudHighWindSampleM : 0.5 * (d.BaseM + d.TopM);
    const WindNed w = wx.WindNedMs(latDeg, lonDeg, sampleM);
    d.DriftEastM = (float)std::fmod(w.E * timeS, kCloudDriftWrapM);
    d.DriftNorthM = (float)std::fmod(w.N * timeS, kCloudDriftWrapM);
    const double sp = std::sqrt(w.E * w.E + w.N * w.N);
    if (sp > 0.1) { d.WindDirE = (float)(w.E / sp); d.WindDirN = (float)(w.N / sp); }
    else { d.WindDirE = 1.0f; d.WindDirN = 0.0f; }   /* calm: no preferred axis, stretch reads as arbitrary */
    CloudCalibrate(d);
  }
  sky.AnchorLatDeg = anchorLatDeg;
  sky.AnchorLonDeg = anchorLonDeg;
  return sky;
}

inline CloudSky CloudSkyFromWeather(const WeatherProvider &wx, double latDeg, double lonDeg,
                                        double timeS) {
  return CloudSkyFromWeather(wx, latDeg, lonDeg, timeS, latDeg, lonDeg);
}

} // namespace outshine
#endif

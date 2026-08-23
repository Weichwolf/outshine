#ifndef CLOUDDENSITY_H
#define CLOUDDENSITY_H

#include <cstdint>
#include <cmath>
#include "WeatherProvider.h"

namespace outshine {

constexpr int   kCloudOctaves   = 4;
constexpr float kCloudLacunarity= 2.0f;
constexpr float kCloudGain      = 0.5f;
constexpr float kCloudShear     = 0.30f;

constexpr float kCloudRotC      = 0.8f;
constexpr float kCloudRotS      = 0.6f;
constexpr float kCloudWarpFreq  = 0.35f;
constexpr float kCloudWarpCross = 0.50f;

constexpr float kCloudFbmMean   = 0.5007f;
constexpr float kCloudFbmSigma  = 0.1331f;
constexpr float kCloudLogitK    = 1.702f;
constexpr float kCloudRemapHard = 0.35f;
constexpr float kCloudRemapSoft = 2.20f;
constexpr float kCloudProfBase  = 0.18f;
constexpr float kCloudProfTop   = 0.60f;

constexpr float kCloudTopMin    = 0.45f;
constexpr int   kCloudErodeOct  = 2;
constexpr float kCloudErodeFreq = 10.0f;
constexpr float kCloudErodeVert = 3.0f;
constexpr float kCloudErodeBase = 0.35f;

constexpr float kCloudErodeMean = 0.5f;

struct CloudDeckParams {
  float BaseM = 0.0f, TopM = 0.0f;
  float Cover = 0.0f;
  float DriftEastM = 0.0f, DriftNorthM = 0.0f;
  float WindDirE = 1.0f, WindDirN = 0.0f;
  float Stretch = 1.0f;
  float FeatureM = 16000.0f;
  float Warp = 0.0f;
  float Erosion = 0.0f;
  float SigmaPerM = 0.0f;

  float RemapEdge = 1.0f, RemapWidth = 0.1f;

  float ErodeFlat = 0.0f;
  float Pad1 = 0.0f;

  float ThicknessM() const { return TopM - BaseM; }
};

struct CloudSky {
  CloudDeckParams Deck[3];
  float VisibilityM = 100000.0f;

  double AnchorLatDeg = 0.0, AnchorLonDeg = 0.0;
  [[nodiscard]] bool Any() const { return Deck[0].Cover > 0.0f || Deck[1].Cover > 0.0f || Deck[2].Cover > 0.0f; }
};

inline float CloudSmooth(float e0, float e1, float x) {
  float t = (x - e0) / (e1 - e0);
  if (t < 0.0f) t = 0.0f;
  if (t > 1.0f) t = 1.0f;
  return t * t * (3.0f - 2.0f * t);
}
inline float CloudMix(float a, float b, float t) { return a + (b - a) * t; }

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
  const uint32_t k = CloudHash3(ix, iy, iz, seed) & 15u;
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

inline float CloudFbmShear(float ax, float bx, uint32_t seed) {
  float qa = ax, qb = bx, sum = 0.0f, amp = 1.0f, norm = 0.0f;
  for (int i = 0; i < kCloudOctaves; i++) {
    float sa = qa + kCloudShear * (float)i * qb;
    sum += amp * CloudNoise2(sa, qb, seed + (uint32_t)i * 131u);
    norm += amp;
    amp *= kCloudGain;
    const float ra = qa * kCloudRotC - qb * kCloudRotS;
    const float rb = qa * kCloudRotS + qb * kCloudRotC;
    qa = ra * kCloudLacunarity;
    qb = rb * kCloudLacunarity;
  }
  return sum / norm;
}

inline void CloudCalibrate(CloudDeckParams &d) {
  float p = d.Cover;
  if (p < 0.02f) p = 0.02f;
  if (p > 0.98f) p = 0.98f;
  const float z = std::log(p / (1.0f - p)) / kCloudLogitK;
  d.RemapEdge = kCloudFbmMean - kCloudFbmSigma * z;
  d.RemapWidth = kCloudFbmSigma * CloudMix(kCloudRemapHard, kCloudRemapSoft, d.Cover);
}

struct CloudSample {
  float Coverage;
  float A, B;
};

inline CloudSample CloudCoverage(const CloudDeckParams &d, float eastM, float northM) {
  CloudSample s{0.0f, 0.0f, 0.0f};
  if (d.Cover <= 0.0f) return s;
  const float inv = 1.0f / d.FeatureM;
  const float x = (eastM - d.DriftEastM) * inv;
  const float y = (northM - d.DriftNorthM) * inv;
  float a = x * d.WindDirE + y * d.WindDirN;
  float b = -x * d.WindDirN + y * d.WindDirE;
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

inline float CloudShape(float c, float h) {
  if (c <= 0.0f) return 0.0f;
  const float top = CloudMix(kCloudTopMin, 1.0f, c);
  const float hn = h / top;
  if (hn >= 1.0f) return 0.0f;
  return c * CloudSmooth(0.0f, kCloudProfBase, hn) * CloudSmooth(1.0f, 1.0f - kCloudProfTop, hn);
}

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

constexpr float kCloudLowDefaultBaseM  = 1200.0f;
constexpr float kCloudMidDefaultBaseM  = 4200.0f;
constexpr float kCloudHighDefaultBaseM = 9000.0f;
constexpr float kCloudLowThickM  = 900.0f;
constexpr float kCloudMidThickM  = 1400.0f;
constexpr float kCloudHighThickM = 500.0f;

constexpr float kCloudBandM[3][2] = {{150.0f, 4000.0f}, {2000.0f, 7500.0f}, {5500.0f, 13000.0f}};

constexpr float kCloudCeilBlendM = 2000.0f;

constexpr float kCloudCeilExistsFrac = 0.10f;

constexpr float kCloudFeatureM[3] = {16000.0f, 26000.0f, 12000.0f};
constexpr float kCloudStretch[3]  = {1.0f, 1.35f, 7.0f};
constexpr float kCloudWarp[3]     = {0.25f, 0.35f, 0.30f};
constexpr float kCloudErosion[3]  = {0.35f, 0.25f, 0.15f};
constexpr float kCloudSigma[3]    = {0.022f, 0.018f, 0.0060f};

constexpr double kCloudHighWindSampleM = 10800.0;

constexpr double kCloudDriftWrapM = 4.0e6;

inline CloudSky CloudSkyFromWeather(const WeatherProvider &wx, double latDeg, double lonDeg,
                                        double timeS, double anchorLatDeg, double anchorLonDeg) {
  const CloudLayers L = wx.Clouds(latDeg, lonDeg);
  const float cover[3] = {(float)(L.LowPct * 0.01), (float)(L.MidPct * 0.01), (float)(L.HighPct * 0.01)};
  const float defBase[3] = {kCloudLowDefaultBaseM, kCloudMidDefaultBaseM, kCloudHighDefaultBaseM};
  const float thick[3] = {kCloudLowThickM, kCloudMidThickM, kCloudHighThickM};

  CloudSky sky;
  sky.VisibilityM = (float)wx.VisibilityM(latDeg, lonDeg);

  float own[3] = {0.0f, 0.0f, 0.0f};
  float ceilFor[3] = {0.0f, 0.0f, 0.0f};
  if (L.HaveCeiling) {
    float free = 1.0f;
    for (int i = 0; i < 3; i++) {
      const float lo = kCloudBandM[i][0], hi = kCloudBandM[i][1];
      float c = (float)L.CeilingM;

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
    else { d.WindDirE = 1.0f; d.WindDirN = 0.0f; }
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

}
#endif

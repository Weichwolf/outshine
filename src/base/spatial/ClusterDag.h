#ifndef OUTSHINE_BASE_SPATIAL_CLUSTERDAG_H
#define OUTSHINE_BASE_SPATIAL_CLUSTERDAG_H

#include "math/Units.h"
#include "math/Vec3.h"
#include <algorithm>
#include <numbers>
#include <cmath>
#include <cstdint>
#include <span>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace outshine {

constexpr double kLeastClusterM = 0.05;

struct DagCluster {
  Vec3f SelfCenter = {{0, 0, 0}};
  float SelfRadius = 0.0f;
  Vec3f ParentCenter = {{0, 0, 0}};
  float ParentRadius = 0.0f;
  float SelfErr = 0.0f;
  float ParentErr = 0.0f;
  uint32_t First = 0, Count = 0;
  uint32_t Level = 0;
};

inline constexpr float kDagRootErr = 3.0e38f;

struct Bounding {
  Vec3f CentreM;
  float RadiusM = 0.0f;
};

struct Strided {
  const float *Floats = nullptr;
  uint32_t Count = 0;
  int Stride = 0;
};

[[nodiscard]] inline Bounding BoundingSphere(Strided over) {
  const float *const verts = over.Floats;
  const uint32_t nverts = over.Count;
  const int stride = over.Stride;
  Bounding out;
  Vec3f &ctr = out.CentreM;
  if ((verts == nullptr) || nverts == 0) {
    ctr[0] = ctr[1] = ctr[2] = 0.0f;
    return out;
  }
  Vec3f lo;
  Vec3f hi;
  for (int a = 0; a < 3; a++) {
    lo[a] = verts[a];
    hi[a] = verts[a];
  }
  for (uint32_t i = 1; i < nverts; i++) {
    for (int a = 0; a < 3; a++) {
      const float v =
          verts[static_cast<size_t>(i) * static_cast<size_t>(stride) + static_cast<size_t>(a)];
      lo[a] = std::min(v, lo[a]);
      hi[a] = std::max(v, hi[a]);
    }
  }
  double r2 = 0.0;
  for (int a = 0; a < 3; a++) {
    ctr[a] = 0.5f * (lo[a] + hi[a]);
    const double h = 0.5 * (static_cast<double>(hi[a]) - static_cast<double>(lo[a]));
    r2 += h * h;
  }
  out.RadiusM = static_cast<float>(std::sqrt(r2));
  return out;
}

inline constexpr float kPixelTau = 1.0f;

struct ClusterDag {
  std::vector<float> Verts;
  std::vector<uint32_t> Idx;
  std::vector<DagCluster> Clusters;
  int Stride = 8;
  int Levels = 0;
  uint32_t BaseTris = 0;
  uint32_t AllTris = 0;
};

struct ClusterDagOpts {
  int MaxTrisPerCluster = 128;
  int GroupSize = 16;
  float TargetRatio = 0.5f;
  int MinLevelTris = 8;

  int (*ClassOf)(const float *v) = nullptr;

  Vec3f Up = {{0.0f, 0.0f, 0.0f}};
};

inline float DagCrossFactor(const Vec3f &ctr, float rad, const Vec3 &eye, const Vec3f &up) {
  const double u2 = static_cast<double>(up[0]) * up[0] + static_cast<double>(up[1]) * up[1] +
                    static_cast<double>(up[2]) * up[2];
  if (u2 < kParallelCross) { return 1.0f; }
  const double dx = static_cast<double>(ctr[0]) - eye[0];
  const double dy = static_cast<double>(ctr[1]) - eye[1];
  const double dz = static_cast<double>(ctr[2]) - eye[2];
  const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (!(d > kLeastRunM) || static_cast<double>(rad) >= d) { return 1.0f; }
  const double iu = 1.0 / std::sqrt(u2);
  const double cosT = (static_cast<double>(up[0]) * dx + static_cast<double>(up[1]) * dy +
                       static_cast<double>(up[2]) * dz) *
                      iu / d;
  const double theta = std::acos(std::clamp(cosT, -1.0, 1.0));
  const double alpha = std::asin(static_cast<double>(rad) / d);
  const double lo = theta - alpha;
  const double hi = theta + alpha;
  const double kHalfPi = 0.5 * std::numbers::pi;
  if (lo <= kHalfPi && hi >= kHalfPi) { return 1.0f; }
  return static_cast<float>(std::max(std::sin(lo < 0.0 ? 0.0 : lo),
                                     std::sin(hi > std::numbers::pi ? std::numbers::pi : hi)));
}

inline float
DagSse(const Vec3f &ctr, float rad, float err, const Vec3 &eye, float fPx, const Vec3f &up) {
  if (!(err > 0.0f)) { return 0.0f; }
  if (err >= kDagRootErr) { return kDagRootErr; }
  const double dx = static_cast<double>(ctr[0]) - eye[0];
  const double dy = static_cast<double>(ctr[1]) - eye[1];
  const double dz = static_cast<double>(ctr[2]) - eye[2];
  double d = std::sqrt(dx * dx + dy * dy + dz * dz) - static_cast<double>(rad);
  d = std::max(d, kLeastClusterM);
  return static_cast<float>(static_cast<double>(err) *
                            static_cast<double>(DagCrossFactor(ctr, rad, eye, up)) *
                            static_cast<double>(fPx) / d);
}

inline double DagEdgeSq(double errM, float fPx, float tau) {
  const double edge = errM * static_cast<double>(fPx) / static_cast<double>(tau);
  return edge * edge;
}

[[nodiscard]] inline bool
DagSelect(const DagCluster &c, const Vec3 &eye, float fPx, float tau, const Vec3f &up) {
  return DagSse(c.SelfCenter, c.SelfRadius, c.SelfErr, eye, fPx, up) <= tau &&
         DagSse(c.ParentCenter, c.ParentRadius, c.ParentErr, eye, fPx, up) > tau;
}

} // namespace outshine
#endif

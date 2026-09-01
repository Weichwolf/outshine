#ifndef OUTSHINE_BASE_SPATIAL_CLUSTERDAG_H
#define OUTSHINE_BASE_SPATIAL_CLUSTERDAG_H

#include <algorithm>
#include <numbers>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <span>
#include <cstring>
#include <cstdlib>
#include <unordered_map>
#include <vector>

namespace outshine {

struct DagCluster {
  float SelfCenter[3] = {0, 0, 0};
  float SelfRadius = 0.0f;
  float ParentCenter[3] = {0, 0, 0};
  float ParentRadius = 0.0f;
  float SelfErr = 0.0f;
  float ParentErr = 0.0f;
  uint32_t First = 0, Count = 0;
  uint32_t Level = 0;
};

inline constexpr float kDagRootErr = 3.0e38f;

inline void
BoundingSphere(const float *verts, uint32_t nverts, int stride, float ctr[3], float *rad) {
  if ((verts == nullptr) || nverts == 0) {
    ctr[0] = ctr[1] = ctr[2] = 0.0f;
    *rad = 0.0f;
    return;
  }
  float lo[3];
  float hi[3];
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
  *rad = static_cast<float>(std::sqrt(r2));
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

  float Up[3] = {0.0f, 0.0f, 0.0f};
};

inline float DagCrossFactor(const float ctr[3], float rad, const double eye[3], const float up[3]) {
  const double u2 = static_cast<double>(up[0]) * up[0] + static_cast<double>(up[1]) * up[1] +
                    static_cast<double>(up[2]) * up[2];
  if (u2 < 1.0e-12) { return 1.0f; }
  const double dx = static_cast<double>(ctr[0]) - eye[0];
  const double dy = static_cast<double>(ctr[1]) - eye[1];
  const double dz = static_cast<double>(ctr[2]) - eye[2];
  const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (!(d > 1.0e-6) || static_cast<double>(rad) >= d) { return 1.0f; }
  const double iu = 1.0 / std::sqrt(u2);
  const double cosT = (static_cast<double>(up[0]) * dx + static_cast<double>(up[1]) * dy +
                       static_cast<double>(up[2]) * dz) *
                      iu / d;
  const double theta = std::acos(cosT < -1.0 ? -1.0 : (cosT > 1.0 ? 1.0 : cosT));
  const double alpha = std::asin(static_cast<double>(rad) / d);
  const double lo = theta - alpha;
  const double hi = theta + alpha;
  const double kHalfPi = 0.5 * std::numbers::pi;
  if (lo <= kHalfPi && hi >= kHalfPi) { return 1.0f; }
  return static_cast<float>(std::max(std::sin(lo < 0.0 ? 0.0 : lo),
                                     std::sin(hi > std::numbers::pi ? std::numbers::pi : hi)));
}

inline float DagSse(
    const float ctr[3], float rad, float err, const double eye[3], float fPx, const float up[3]) {
  if (!(err > 0.0f)) { return 0.0f; }
  if (err >= kDagRootErr) { return kDagRootErr; }
  const double dx = static_cast<double>(ctr[0]) - eye[0];
  const double dy = static_cast<double>(ctr[1]) - eye[1];
  const double dz = static_cast<double>(ctr[2]) - eye[2];
  double d = std::sqrt(dx * dx + dy * dy + dz * dz) - static_cast<double>(rad);
  d = std::max(d, 0.05);
  return static_cast<float>(static_cast<double>(err) *
                            static_cast<double>(DagCrossFactor(ctr, rad, eye, up)) *
                            static_cast<double>(fPx) / d);
}

inline double DagEdgeSq(double errM, float fPx, float tau) {
  const double edge = errM * static_cast<double>(fPx) / static_cast<double>(tau);
  return edge * edge;
}

[[nodiscard]] inline bool
DagSelect(const DagCluster &c, const double eye[3], float fPx, float tau, const float up[3]) {
  return DagSse(c.SelfCenter, c.SelfRadius, c.SelfErr, eye, fPx, up) <= tau &&
         DagSse(c.ParentCenter, c.ParentRadius, c.ParentErr, eye, fPx, up) > tau;
}

} // namespace outshine
#endif

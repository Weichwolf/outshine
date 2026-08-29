#ifndef OUTSHINE_BASE_SPATIAL_CLUSTERDAG_H
#define OUTSHINE_BASE_SPATIAL_CLUSTERDAG_H

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
  uint32_t First = 0, Count = 0;
  float SelfCenter[3] = {0, 0, 0};
  float SelfRadius = 0.0f;
  float SelfErr = 0.0f;
  float ParentCenter[3] = {0, 0, 0};
  float ParentRadius = 0.0f;
  float ParentErr = 0.0f;
  uint8_t Level = 0;
};

inline constexpr float kDagRootErr = 3.0e38f;

inline void BoundingSphere(const float *verts, uint32_t nverts, int stride, float ctr[3], float *rad) {
  if (!verts || nverts == 0) {
    ctr[0] = ctr[1] = ctr[2] = 0.0f;
    *rad = 0.0f;
    return;
  }
  float lo[3], hi[3];
  for (int a = 0; a < 3; a++) { lo[a] = verts[a]; hi[a] = verts[a]; }
  for (uint32_t i = 1; i < nverts; i++)
    for (int a = 0; a < 3; a++) {
      const float v = verts[(size_t)i * (size_t)stride + (size_t)a];
      if (v < lo[a]) lo[a] = v;
      if (v > hi[a]) hi[a] = v;
    }
  double r2 = 0.0;
  for (int a = 0; a < 3; a++) {
    ctr[a] = 0.5f * (lo[a] + hi[a]);
    const double h = 0.5 * ((double)hi[a] - (double)lo[a]);
    r2 += h * h;
  }
  *rad = (float)std::sqrt(r2);
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
  const double u2 = (double)up[0] * up[0] + (double)up[1] * up[1] + (double)up[2] * up[2];
  if (u2 < 1.0e-12) return 1.0f;
  const double dx = (double)ctr[0] - eye[0], dy = (double)ctr[1] - eye[1], dz = (double)ctr[2] - eye[2];
  const double d = std::sqrt(dx * dx + dy * dy + dz * dz);
  if (!(d > 1.0e-6) || (double)rad >= d) return 1.0f;
  const double iu = 1.0 / std::sqrt(u2);
  const double cosT = ((double)up[0] * dx + (double)up[1] * dy + (double)up[2] * dz) * iu / d;
  const double theta = std::acos(cosT < -1.0 ? -1.0 : (cosT > 1.0 ? 1.0 : cosT));
  const double alpha = std::asin((double)rad / d);
  const double lo = theta - alpha, hi = theta + alpha;
  const double kHalfPi = 0.5 * std::numbers::pi;
  if (lo <= kHalfPi && hi >= kHalfPi) return 1.0f;
  return (float)std::max(std::sin(lo < 0.0 ? 0.0 : lo), std::sin(hi > std::numbers::pi ? std::numbers::pi : hi));
}

inline float DagSse(const float ctr[3], float rad, float err, const double eye[3], float fPx,
                    const float up[3]) {
  if (!(err > 0.0f)) return 0.0f;
  if (err >= kDagRootErr) return kDagRootErr;
  const double dx = (double)ctr[0] - eye[0], dy = (double)ctr[1] - eye[1], dz = (double)ctr[2] - eye[2];
  double d = std::sqrt(dx * dx + dy * dy + dz * dz) - (double)rad;
  if (d < 0.05) d = 0.05;
  return (float)((double)err * (double)DagCrossFactor(ctr, rad, eye, up) * (double)fPx / d);
}

inline double DagEdgeSq(double errM, float fPx, float tau) {
  const double edge = errM * (double)fPx / (double)tau;
  return edge * edge;
}

[[nodiscard]] inline bool DagSelect(const DagCluster &c, const double eye[3], float fPx, float tau,
                      const float up[3]) {
  return DagSse(c.SelfCenter, c.SelfRadius, c.SelfErr, eye, fPx, up) <= tau &&
         DagSse(c.ParentCenter, c.ParentRadius, c.ParentErr, eye, fPx, up) > tau;
}

// WHAT THE CUT NEEDS AND NOTHING MORE. A DAG BUILDER stood here -- `ClusterDagBuild` and a `dag::`
// namespace of quadrics, clustering, group simplification and absorption, 770 lines of it -- and
// nothing in the engine reached any of it. It was written for board:1991, cooked nothing, and took
// an `outshine::Geometry` while every path that would have used it carried the importer's own
// carrier instead.
//
// Deleted rather than kept for later, which is this page's own rule: a capability no declaration
// reaches is the commonest defect here, and one kept because it might be wanted is the same defect
// with an excuse. When board:1949 makes ONE value, the cooker gets built against THAT value -- which
// is a different builder from this one, so keeping it would have preserved the wrong shape.
//
// What stays is what `GroundPatchwork` actually calls: the cluster record, the screen-space error,
// and the selection.



}
#endif

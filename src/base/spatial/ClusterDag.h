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

namespace dag {

struct Quadric {
  double A[6];
  double B[3];
  double C;
  double W;
};

inline void QAddPlane(Quadric &q, const double n[3], double d, double w) {
  q.A[0] += w * n[0] * n[0]; q.A[1] += w * n[0] * n[1]; q.A[2] += w * n[0] * n[2];
  q.A[3] += w * n[1] * n[1]; q.A[4] += w * n[1] * n[2]; q.A[5] += w * n[2] * n[2];
  q.B[0] += w * d * n[0]; q.B[1] += w * d * n[1]; q.B[2] += w * d * n[2];
  q.C += w * d * d;
  q.W += w;
}

inline void QAdd(Quadric &a, const Quadric &b) {
  for (int i = 0; i < 6; i++) a.A[i] += b.A[i];
  for (int i = 0; i < 3; i++) a.B[i] += b.B[i];
  a.C += b.C;
  a.W += b.W;
}

inline double QEval(const Quadric &q, const double p[3]) {
  const double ax = q.A[0] * p[0] + q.A[1] * p[1] + q.A[2] * p[2];
  const double ay = q.A[1] * p[0] + q.A[3] * p[1] + q.A[4] * p[2];
  const double az = q.A[2] * p[0] + q.A[4] * p[1] + q.A[5] * p[2];
  return p[0] * ax + p[1] * ay + p[2] * az +
         2.0 * (q.B[0] * p[0] + q.B[1] * p[1] + q.B[2] * p[2]) + q.C;
}

inline double QDist(const Quadric &q, const double p[3]) {
  return std::sqrt(std::max(0.0, QEval(q, p)) / std::max(q.W, 1.0e-12));
}

inline uint32_t Part1By2(uint32_t v) {
  v &= 0x3ff;
  v = (v | (v << 16)) & 0x030000FF;
  v = (v | (v << 8)) & 0x0300F00F;
  v = (v | (v << 4)) & 0x030C30C3;
  v = (v | (v << 2)) & 0x09249249;
  return v;
}
inline uint32_t Morton(uint32_t x, uint32_t y, uint32_t z) {
  return Part1By2(x) | (Part1By2(y) << 1) | (Part1By2(z) << 2);
}

struct Clustered {
  std::vector<float> V;
  std::vector<uint32_t> Corner;
  std::vector<uint32_t> Pos;
  std::vector<uint8_t> Cls;
  std::vector<double> PP;
  int Stride = 8;
  int NPos = 0;
  const float *P(uint32_t v) const { return &V[(size_t)v * (size_t)Stride]; }
};

struct PosKey { int64_t x, y, z; };
struct PosKeyHash {
  size_t operator()(const PosKey &k) const {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&h](uint64_t v) { h ^= v; h *= 1099511628211ull; };
    mix((uint64_t)k.x); mix((uint64_t)k.y); mix((uint64_t)k.z);
    return (size_t)h;
  }
};
struct PosKeyEq {
  [[nodiscard]] bool operator()(const PosKey &a, const PosKey &b) const {
    return a.x == b.x && a.y == b.y && a.z == b.z;
  }
};

inline void Weld(const float *soup, uint32_t nverts, int stride, int (*classOf)(const float *),
                 Clustered *m) {
  m->Stride = stride;
  m->V.clear();
  m->Pos.clear();
  m->Cls.clear();
  m->PP.clear();
  m->Corner.resize(nverts);

  std::unordered_map<PosKey, uint32_t, PosKeyHash, PosKeyEq> pmap;

  std::unordered_multimap<uint64_t, uint32_t> amap;
  pmap.reserve(nverts);
  amap.reserve(nverts);
  const size_t vbytes = (size_t)stride * sizeof(float);
  for (uint32_t i = 0; i < nverts; i++) {
    const float *v = soup + (size_t)i * (size_t)stride;
    const PosKey pk{(int64_t)std::llround((double)v[0] * 1.0e4),
                    (int64_t)std::llround((double)v[1] * 1.0e4),
                    (int64_t)std::llround((double)v[2] * 1.0e4)};
    uint32_t pid;
    auto pit = pmap.find(pk);
    if (pit != pmap.end()) pid = pit->second;
    else {
      pid = (uint32_t)(m->PP.size() / 3);
      m->PP.push_back(v[0]); m->PP.push_back(v[1]); m->PP.push_back(v[2]);
      pmap.emplace(pk, pid);
    }
    const uint32_t cls = classOf ? (uint32_t)classOf(v) : 0u;
    uint64_t ak = ((uint64_t)pid << 8) | (cls & 0xff);
    {
      uint64_t h = ak * 1099511628211ull;
      const unsigned char *b = (const unsigned char *)v;
      for (size_t k = 0; k < vbytes; k++) { h ^= b[k]; h *= 1099511628211ull; }
      ak = h;
    }
    uint32_t id = 0xffffffffu;
    for (auto it = amap.equal_range(ak); it.first != it.second; ++it.first) {
      const uint32_t cand = it.first->second;
      if (m->Pos[cand] == pid && m->Cls[cand] == (uint8_t)cls &&
          std::memcmp(&m->V[(size_t)cand * (size_t)stride], v, vbytes) == 0) { id = cand; break; }
    }
    if (id == 0xffffffffu) {
      id = (uint32_t)(m->V.size() / (size_t)stride);
      m->V.insert(m->V.end(), v, v + stride);
      m->Pos.push_back(pid);
      m->Cls.push_back((uint8_t)cls);
      amap.emplace(ak, id);
    }
    m->Corner[i] = id;
  }
  m->NPos = (int)(m->PP.size() / 3);
}

inline void Partition(const Clustered &m, std::span<const uint32_t> tri, int maxTris,
                      std::vector<uint32_t> *order, std::vector<uint32_t> *starts) {
  const size_t n = tri.size() / 3;
  double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
  std::vector<double> ctr(n * 3);
  for (size_t t = 0; t < n; t++) {
    double c[3] = {0, 0, 0};
    for (int k = 0; k < 3; k++) {
      const float *p = m.P(tri[t * 3 + (size_t)k]);
      c[0] += p[0]; c[1] += p[1]; c[2] += p[2];
    }
    for (int a = 0; a < 3; a++) {
      c[a] /= 3.0;
      ctr[t * 3 + (size_t)a] = c[a];
      if (c[a] < lo[a]) lo[a] = c[a];
      if (c[a] > hi[a]) hi[a] = c[a];
    }
  }
  std::vector<std::pair<uint32_t, uint32_t>> key(n);
  for (size_t t = 0; t < n; t++) {
    uint32_t q[3];
    for (int a = 0; a < 3; a++) {
      const double s = hi[a] - lo[a];
      double f = s > 1e-9 ? (ctr[t * 3 + (size_t)a] - lo[a]) / s : 0.0;
      if (f < 0.0) f = 0.0;
      if (f > 1.0) f = 1.0;
      q[a] = (uint32_t)(f * 1023.0);
    }
    key[t] = {Morton(q[0], q[1], q[2]), (uint32_t)t};
  }
  std::sort(key.begin(), key.end());
  order->resize(n);
  for (size_t i = 0; i < n; i++) (*order)[i] = key[i].second;
  starts->clear();
  for (size_t i = 0; i < n; i += (size_t)maxTris) starts->push_back((uint32_t)i);
  starts->push_back((uint32_t)n);
}

struct Absorb {
  std::vector<int32_t> Head, Tail, Next;
  void Init(int n) {
    Head.resize((size_t)n);
    Tail.resize((size_t)n);
    Next.assign((size_t)n, -1);
    for (int i = 0; i < n; i++) { Head[(size_t)i] = i; Tail[(size_t)i] = i; }
  }
  void Merge(uint32_t dst, uint32_t src) {
    if (Head[src] < 0) return;
    Next[(size_t)Tail[dst]] = Head[src];
    Tail[dst] = Tail[src];
    Head[src] = -1;
    Tail[src] = -1;
  }
};

struct DevMesh {
  bool Vertical = false;
  double U[3] = {0, 0, 0}, A[3] = {1, 0, 0}, B[3] = {0, 1, 0};
  double Lo[2] = {0, 0}, Cell = 1.0;
  int NX = 1, NY = 1;
  std::vector<double> T;
  std::vector<uint32_t> Start, Idx;

  void Set(std::span<const double> corners, const float up[3]) {
    T.assign(corners.begin(), corners.end());
    const size_t n = T.size() / 9;
    Vertical = up && (up[0] * up[0] + up[1] * up[1] + up[2] * up[2]) > 1.0e-12f;
    if (Vertical) {
      const double l = std::sqrt((double)up[0] * up[0] + (double)up[1] * up[1] + (double)up[2] * up[2]);
      for (int a = 0; a < 3; a++) U[a] = (double)up[a] / l;
      const int k = std::fabs(U[0]) < 0.9 ? 0 : 1;
      double t[3] = {0, 0, 0};
      t[k] = 1.0;
      const double d = t[0] * U[0] + t[1] * U[1] + t[2] * U[2];
      for (int a = 0; a < 3; a++) A[a] = t[a] - d * U[a];
      const double al = std::sqrt(A[0] * A[0] + A[1] * A[1] + A[2] * A[2]);
      for (int a = 0; a < 3; a++) A[a] /= al;
      B[0] = U[1] * A[2] - U[2] * A[1];
      B[1] = U[2] * A[0] - U[0] * A[2];
      B[2] = U[0] * A[1] - U[1] * A[0];
    } else {
      double lo[3] = {1e300, 1e300, 1e300}, hi[3] = {-1e300, -1e300, -1e300};
      for (size_t i = 0; i < T.size(); i += 3)
        for (int a = 0; a < 3; a++) {
          lo[a] = std::min(lo[a], T[i + (size_t)a]);
          hi[a] = std::max(hi[a], T[i + (size_t)a]);
        }
      int order[3] = {0, 1, 2};
      for (int i = 0; i < 2; i++)
        for (int j = i + 1; j < 3; j++)
          if (hi[order[j]] - lo[order[j]] > hi[order[i]] - lo[order[i]]) std::swap(order[i], order[j]);
      for (int a = 0; a < 3; a++) { A[a] = 0.0; B[a] = 0.0; U[a] = 0.0; }
      A[order[0]] = 1.0;
      B[order[1]] = 1.0;
    }

    double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300};
    for (size_t i = 0; i < T.size(); i += 3) {
      const double u = Pa(&T[i]), v = Pb(&T[i]);
      lo[0] = std::min(lo[0], u); hi[0] = std::max(hi[0], u);
      lo[1] = std::min(lo[1], v); hi[1] = std::max(hi[1], v);
    }
    if (!n) { Start.assign(2, 0); Idx.clear(); NX = NY = 1; return; }
    Lo[0] = lo[0]; Lo[1] = lo[1];
    const double ext = std::max(std::max(hi[0] - lo[0], hi[1] - lo[1]), 1.0e-9);
    Cell = ext / std::max(1.0, std::sqrt((double)n));
    NX = (int)std::floor((hi[0] - lo[0]) / Cell) + 1;
    NY = (int)std::floor((hi[1] - lo[1]) / Cell) + 1;

    std::vector<uint32_t> count((size_t)NX * (size_t)NY + 1, 0);
    for (size_t t = 0; t < n; t++) {
      int x0, x1, y0, y1;
      Span(t, &x0, &x1, &y0, &y1);
      for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) count[(size_t)y * (size_t)NX + (size_t)x + 1]++;
    }
    for (size_t i = 1; i < count.size(); i++) count[i] += count[i - 1];
    Start = count;
    Idx.resize(count.back());
    for (size_t t = 0; t < n; t++) {
      int x0, x1, y0, y1;
      Span(t, &x0, &x1, &y0, &y1);
      for (int y = y0; y <= y1; y++)
        for (int x = x0; x <= x1; x++) Idx[count[(size_t)y * (size_t)NX + (size_t)x]++] = (uint32_t)t;
    }
  }

  double Dev(const double p[3]) const {
    if (T.empty()) return 0.0;
    if (Vertical) {
      const double d = Column(p);
      if (d >= 0.0) return d;
    }
    return Nearest(p);
  }

private:
  double Pa(const double *q) const { return q[0] * A[0] + q[1] * A[1] + q[2] * A[2]; }
  double Pb(const double *q) const { return q[0] * B[0] + q[1] * B[1] + q[2] * B[2]; }
  double Pu(const double *q) const { return q[0] * U[0] + q[1] * U[1] + q[2] * U[2]; }

  void Span(size_t t, int *x0, int *x1, int *y0, int *y1) const {
    double lo[2] = {1e300, 1e300}, hi[2] = {-1e300, -1e300};
    for (int k = 0; k < 3; k++) {
      const double *q = &T[t * 9 + (size_t)k * 3];
      const double u = Pa(q), v = Pb(q);
      lo[0] = std::min(lo[0], u); hi[0] = std::max(hi[0], u);
      lo[1] = std::min(lo[1], v); hi[1] = std::max(hi[1], v);
    }
    *x0 = Clamp((int)std::floor((lo[0] - Lo[0]) / Cell), NX);
    *x1 = Clamp((int)std::floor((hi[0] - Lo[0]) / Cell), NX);
    *y0 = Clamp((int)std::floor((lo[1] - Lo[1]) / Cell), NY);
    *y1 = Clamp((int)std::floor((hi[1] - Lo[1]) / Cell), NY);
  }
  static int Clamp(int v, int n) { return v < 0 ? 0 : (v >= n ? n - 1 : v); }

  double Column(const double p[3]) const {
    const int cx = Clamp((int)std::floor((Pa(p) - Lo[0]) / Cell), NX);
    const int cy = Clamp((int)std::floor((Pb(p) - Lo[1]) / Cell), NY);
    const double pu = Pa(p), pv = Pb(p), ph = Pu(p);
    double best = -1.0;

    const size_t c = (size_t)cy * (size_t)NX + (size_t)cx;
    for (uint32_t i = Start[c]; i < Start[c + 1]; i++) {
      const double *q = &T[(size_t)Idx[i] * 9];
      const double au = Pa(q), av = Pb(q), bu = Pa(q + 3), bv = Pb(q + 3), cu = Pa(q + 6),
                   cv = Pb(q + 6);
      const double den = (bv - cv) * (au - cu) + (cu - bu) * (av - cv);
      if (std::fabs(den) < 1.0e-14) continue;
      const double l0 = ((bv - cv) * (pu - cu) + (cu - bu) * (pv - cv)) / den;
      const double l1 = ((cv - av) * (pu - cu) + (au - cu) * (pv - cv)) / den;
      const double l2 = 1.0 - l0 - l1;
      if (l0 < -1.0e-9 || l1 < -1.0e-9 || l2 < -1.0e-9) continue;
      const double h = l0 * Pu(q) + l1 * Pu(q + 3) + l2 * Pu(q + 6);
      best = std::max(best, std::fabs(ph - h));
    }
    return best;
  }

  static double PointTri(const double p[3], const double *q) {
    const double *a = q, *b = q + 3, *c = q + 6;
    double ab[3], ac[3], ap[3];
    for (int i = 0; i < 3; i++) { ab[i] = b[i] - a[i]; ac[i] = c[i] - a[i]; ap[i] = p[i] - a[i]; }
    const double d1 = ab[0] * ap[0] + ab[1] * ap[1] + ab[2] * ap[2];
    const double d2 = ac[0] * ap[0] + ac[1] * ap[1] + ac[2] * ap[2];
    double cp[3];
    auto Len2 = [](const double v[3]) { return v[0] * v[0] + v[1] * v[1] + v[2] * v[2]; };
    if (d1 <= 0.0 && d2 <= 0.0) return std::sqrt(Len2(ap));
    double bp[3];
    for (int i = 0; i < 3; i++) bp[i] = p[i] - b[i];
    const double d3 = ab[0] * bp[0] + ab[1] * bp[1] + ab[2] * bp[2];
    const double d4 = ac[0] * bp[0] + ac[1] * bp[1] + ac[2] * bp[2];
    if (d3 >= 0.0 && d4 <= d3) return std::sqrt(Len2(bp));
    const double vc = d1 * d4 - d3 * d2;
    if (vc <= 0.0 && d1 >= 0.0 && d3 <= 0.0) {
      const double v = d1 / (d1 - d3);
      for (int i = 0; i < 3; i++) cp[i] = ap[i] - v * ab[i];
      return std::sqrt(Len2(cp));
    }
    for (int i = 0; i < 3; i++) cp[i] = p[i] - c[i];
    const double d5 = ab[0] * cp[0] + ab[1] * cp[1] + ab[2] * cp[2];
    const double d6 = ac[0] * cp[0] + ac[1] * cp[1] + ac[2] * cp[2];
    if (d6 >= 0.0 && d5 <= d6) return std::sqrt(Len2(cp));
    const double vb = d5 * d2 - d1 * d6;
    if (vb <= 0.0 && d2 >= 0.0 && d6 <= 0.0) {
      const double w = d2 / (d2 - d6);
      double t[3];
      for (int i = 0; i < 3; i++) t[i] = ap[i] - w * ac[i];
      return std::sqrt(Len2(t));
    }
    const double va = d3 * d6 - d5 * d4;
    if (va <= 0.0 && (d4 - d3) >= 0.0 && (d5 - d6) >= 0.0) {
      const double w = (d4 - d3) / ((d4 - d3) + (d5 - d6));
      double t[3];
      for (int i = 0; i < 3; i++) t[i] = p[i] - (b[i] + w * (c[i] - b[i]));
      return std::sqrt(Len2(t));
    }
    const double den = va + vb + vc;
    const double v = vb / den, w = vc / den;
    double t[3];
    for (int i = 0; i < 3; i++) t[i] = ap[i] - (v * ab[i] + w * ac[i]);
    return std::sqrt(Len2(t));
  }

  double Nearest(const double p[3]) const {
    const int cx = Clamp((int)std::floor((Pa(p) - Lo[0]) / Cell), NX);
    const int cy = Clamp((int)std::floor((Pb(p) - Lo[1]) / Cell), NY);
    double best = 1e300;
    const int maxR = std::max(NX, NY);
    for (int r = 0; r <= maxR; r++) {
      if (best < 1e299 && (double)(r - 1) * Cell > best) break;
      for (int y = cy - r; y <= cy + r; y++) {
        if (y < 0 || y >= NY) continue;
        for (int x = cx - r; x <= cx + r; x++) {
          if (x < 0 || x >= NX) continue;
          if (r > 0 && std::abs(x - cx) != r && std::abs(y - cy) != r) continue;
          const size_t c = (size_t)y * (size_t)NX + (size_t)x;
          for (uint32_t i = Start[c]; i < Start[c + 1]; i++)
            best = std::min(best, PointTri(p, &T[(size_t)Idx[i] * 9]));
        }
      }
    }
    return best < 1e299 ? best : 0.0;
  }
};

struct Collapse {
  double Cost;
  uint32_t P0, P1;
  uint32_t Stamp;
  [[nodiscard]] bool operator<(const Collapse &o) const { return Cost > o.Cost; }
};

inline void SimplifyGroup(const Clustered &m, std::vector<uint32_t> &tri, size_t targetTris,
                          std::span<const uint8_t> sharedPos, Absorb &ab) {
  const size_t n0 = tri.size() / 3;
  if (n0 <= targetTris || n0 == 0) return;

  std::vector<uint32_t> pv;
  pv.reserve(n0 * 3);
  for (uint32_t v : tri) pv.push_back(m.Pos[v]);
  std::sort(pv.begin(), pv.end());
  pv.erase(std::unique(pv.begin(), pv.end()), pv.end());
  const size_t nv = pv.size();
  std::unordered_map<uint32_t, uint32_t> loc;
  loc.reserve(nv * 2);
  for (uint32_t i = 0; i < nv; i++) loc.emplace(pv[i], i);

  std::vector<double> pos(nv * 3);
  for (size_t i = 0; i < nv; i++)
    for (int a = 0; a < 3; a++) pos[i * 3 + (size_t)a] = m.PP[(size_t)pv[i] * 3 + (size_t)a];

  std::unordered_map<uint64_t, uint32_t> attrAt;
  attrAt.reserve(n0 * 3);
  std::vector<uint32_t> clsMask(nv, 0);
  for (uint32_t v : tri) {
    const uint32_t p = loc[m.Pos[v]];
    attrAt.emplace(((uint64_t)p << 8) | m.Cls[v], v);
    clsMask[p] |= 1u << (m.Cls[v] & 31);
  }

  std::vector<uint32_t> A(tri);
  std::vector<uint32_t> T(tri.size());
  for (size_t i = 0; i < tri.size(); i++) T[i] = loc[m.Pos[tri[i]]];

  std::vector<uint8_t> live(n0, 1), locked(nv, 0), dead(nv, 0);
  std::vector<Quadric> Q(nv);
  std::memset(Q.data(), 0, Q.size() * sizeof(Quadric));
  std::vector<std::vector<uint32_t>> inc(nv);

  auto TriNormal = [&](size_t t, double n[3]) {
    const double *a = &pos[(size_t)T[t * 3 + 0] * 3], *b = &pos[(size_t)T[t * 3 + 1] * 3],
                 *c = &pos[(size_t)T[t * 3 + 2] * 3];
    const double e0[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    const double e1[3] = {c[0] - a[0], c[1] - a[1], c[2] - a[2]};
    n[0] = e0[1] * e1[2] - e0[2] * e1[1];
    n[1] = e0[2] * e1[0] - e0[0] * e1[2];
    n[2] = e0[0] * e1[1] - e0[1] * e1[0];
    return std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
  };

  for (size_t t = 0; t < n0; t++) {
    double nrm[3];
    const double len = TriNormal(t, nrm);
    for (int k = 0; k < 3; k++) inc[T[t * 3 + (size_t)k]].push_back((uint32_t)t);
    if (len < 1e-12) { live[t] = 0; continue; }
    const double un[3] = {nrm[0] / len, nrm[1] / len, nrm[2] / len};
    const double *a = &pos[(size_t)T[t * 3 + 0] * 3];
    const double d = -(un[0] * a[0] + un[1] * a[1] + un[2] * a[2]);
    for (int k = 0; k < 3; k++) QAddPlane(Q[T[t * 3 + (size_t)k]], un, d, len * 0.5);
  }

  {
    std::unordered_map<uint64_t, int> ecount;
    ecount.reserve(n0 * 3);
    for (size_t t = 0; t < n0; t++) {
      if (!live[t]) continue;
      for (int k = 0; k < 3; k++) {
        uint32_t a = T[t * 3 + (size_t)k], b = T[t * 3 + (size_t)((k + 1) % 3)];
        if (a > b) std::swap(a, b);
        ecount[((uint64_t)a << 32) | b]++;
      }
    }
    for (const auto &kv : ecount) {
      if (kv.second == 2) continue;
      locked[(uint32_t)(kv.first >> 32)] = 1;
      locked[(uint32_t)(kv.first & 0xffffffffu)] = 1;
    }
  }

  for (uint32_t i = 0; i < (uint32_t)nv; i++)
    if (sharedPos[pv[i]]) locked[i] = 1;

  std::vector<uint32_t> stamp(nv, 0);

  auto Neighbours = [&](uint32_t v, std::vector<uint32_t> &out) {
    out.clear();
    for (uint32_t t : inc[v]) {
      if (!live[t]) continue;
      for (int k = 0; k < 3; k++) {
        const uint32_t w = T[(size_t)t * 3 + (size_t)k];
        if (w != v) out.push_back(w);
      }
    }
    std::sort(out.begin(), out.end());
    out.erase(std::unique(out.begin(), out.end()), out.end());
  };

  std::vector<Collapse> heap;
  heap.reserve(n0 * 3);
  auto Cost = [&](uint32_t p0, uint32_t p1) {
    Quadric q = Q[p0];
    QAdd(q, Q[p1]);
    return QDist(q, &pos[(size_t)p1 * 3]);
  };
  auto Push = [&](uint32_t p0, uint32_t p1) {
    if (dead[p0] || dead[p1] || locked[p0]) return;
    if ((clsMask[p0] & clsMask[p1]) != clsMask[p0]) return;
    heap.push_back(Collapse{Cost(p0, p1), p0, p1, stamp[p0] + stamp[p1]});
    std::push_heap(heap.begin(), heap.end());
  };

  {
    std::vector<uint32_t> nb;
    for (uint32_t v = 0; v < (uint32_t)nv; v++) {
      if (locked[v]) continue;
      Neighbours(v, nb);
      for (uint32_t w : nb) Push(v, w);
    }
  }

  size_t liveTris = 0;
  for (size_t t = 0; t < n0; t++) liveTris += live[t] ? 1 : 0;

  std::vector<uint32_t> nb0, nb1, opp, common;
  while (liveTris > targetTris && !heap.empty()) {
    std::pop_heap(heap.begin(), heap.end());
    const Collapse c = heap.back();
    heap.pop_back();
    if (dead[c.P0] || dead[c.P1] || locked[c.P0]) continue;
    if (stamp[c.P0] + stamp[c.P1] != c.Stamp) continue;

    Neighbours(c.P0, nb0);
    Neighbours(c.P1, nb1);
    opp.clear();
    size_t shared = 0;
    for (uint32_t t : inc[c.P0]) {
      if (!live[t]) continue;
      bool has1 = false;
      uint32_t third = 0xffffffffu;
      for (int k = 0; k < 3; k++) {
        const uint32_t w = T[(size_t)t * 3 + (size_t)k];
        if (w == c.P1) has1 = true;
        else if (w != c.P0) third = w;
      }
      if (has1) { shared++; opp.push_back(third); }
    }
    if (shared == 0) continue;
    common.clear();
    std::set_intersection(nb0.begin(), nb0.end(), nb1.begin(), nb1.end(), std::back_inserter(common));
    std::sort(opp.begin(), opp.end());
    opp.erase(std::unique(opp.begin(), opp.end()), opp.end());
    if (common != opp) continue;

    bool flip = false;
    for (uint32_t t : inc[c.P0]) {
      if (!live[t] || flip) continue;
      bool has1 = false;
      for (int k = 0; k < 3; k++) has1 = has1 || T[(size_t)t * 3 + (size_t)k] == c.P1;
      if (has1) continue;
      double nOld[3];
      const double lo = TriNormal(t, nOld);
      uint32_t save[3];
      for (int k = 0; k < 3; k++) {
        save[k] = T[(size_t)t * 3 + (size_t)k];
        if (save[k] == c.P0) T[(size_t)t * 3 + (size_t)k] = c.P1;
      }
      double nNew[3];
      const double ln = TriNormal(t, nNew);
      double emax = 0.0;
      for (int k = 0; k < 3; k++) {
        const double *u = &pos[(size_t)T[(size_t)t * 3 + (size_t)k] * 3];
        const double *w = &pos[(size_t)T[(size_t)t * 3 + (size_t)((k + 1) % 3)] * 3];
        const double dx = w[0] - u[0], dy = w[1] - u[1], dz = w[2] - u[2];
        emax = std::max(emax, dx * dx + dy * dy + dz * dz);
      }
      for (int k = 0; k < 3; k++) T[(size_t)t * 3 + (size_t)k] = save[k];
      if (ln < 1e-12 || lo < 1e-12) { flip = true; break; }
      if (ln < 0.10 * emax) { flip = true; break; }
      if (nOld[0] * nNew[0] + nOld[1] * nNew[1] + nOld[2] * nNew[2] <= 0.0) flip = true;
    }
    if (flip) continue;

    for (uint32_t t : inc[c.P0]) {
      if (!live[t]) continue;
      bool has1 = false;
      for (int k = 0; k < 3; k++) has1 = has1 || T[(size_t)t * 3 + (size_t)k] == c.P1;
      if (has1) { live[t] = 0; liveTris--; continue; }
      for (int k = 0; k < 3; k++) {
        const size_t idx = (size_t)t * 3 + (size_t)k;
        if (T[idx] != c.P0) continue;
        T[idx] = c.P1;
        auto it = attrAt.find(((uint64_t)c.P1 << 8) | m.Cls[A[idx]]);
        if (it != attrAt.end()) A[idx] = it->second;
      }
      inc[c.P1].push_back(t);
    }
    dead[c.P0] = 1;
    QAdd(Q[c.P1], Q[c.P0]);
    ab.Merge(pv[c.P1], pv[c.P0]);
    stamp[c.P1]++;
    Neighbours(c.P1, nb1);
    for (uint32_t w : nb1) { Push(c.P1, w); Push(w, c.P1); }
  }

  tri.clear();
  for (size_t t = 0; t < n0; t++) {
    if (!live[t]) continue;
    for (int k = 0; k < 3; k++) tri.push_back(A[t * 3 + (size_t)k]);
  }
}

inline void Sphere(const Clustered &m, std::span<const uint32_t> tri, size_t first, size_t count,
                   float ctr[3], float *rad) {
  double c[3] = {0, 0, 0};
  size_t n = 0;
  for (size_t i = first; i < first + count; i++) {
    const float *p = m.P(tri[i]);
    c[0] += p[0]; c[1] += p[1]; c[2] += p[2];
    n++;
  }
  if (!n) { ctr[0] = ctr[1] = ctr[2] = 0.0f; *rad = 0.0f; return; }
  for (int a = 0; a < 3; a++) c[a] /= (double)n;
  double r2 = 0.0;
  for (size_t i = first; i < first + count; i++) {
    const float *p = m.P(tri[i]);
    const double dx = p[0] - c[0], dy = p[1] - c[1], dz = p[2] - c[2];
    r2 = std::max(r2, dx * dx + dy * dy + dz * dz);
  }
  ctr[0] = (float)c[0]; ctr[1] = (float)c[1]; ctr[2] = (float)c[2];
  *rad = (float)std::sqrt(r2);
}

}

[[nodiscard]] inline bool ClusterDagBuild(const float *soup, uint32_t nverts, int stride,
                            const ClusterDagOpts &opts, ClusterDag *out) {
  if (!soup || !out || nverts < 3 || stride < 3) return false;
  out->Verts.clear();
  out->Idx.clear();
  out->Clusters.clear();
  out->Stride = stride;
  out->Levels = 0;
  out->BaseTris = nverts / 3;
  out->AllTris = 0;

  dag::Clustered m;
  dag::Weld(soup, nverts, stride, opts.ClassOf, &m);

  std::vector<uint32_t> tri(m.Corner.begin(), m.Corner.end());
  std::vector<uint32_t> order, starts;
  dag::Partition(m, tri, opts.MaxTrisPerCluster, &order, &starts);

  out->Verts = m.V;

  std::vector<uint32_t> lvlTri;
  std::vector<std::pair<uint32_t, uint32_t>> lvlRange;
  lvlTri.reserve(tri.size());
  for (size_t g = 0; g + 1 < starts.size(); g++) {
    const uint32_t first = (uint32_t)(lvlTri.size() / 3);
    for (uint32_t i = starts[g]; i < starts[g + 1]; i++)
      for (int k = 0; k < 3; k++) lvlTri.push_back(tri[(size_t)order[i] * 3 + (size_t)k]);
    lvlRange.emplace_back(first, (uint32_t)(lvlTri.size() / 3) - first);
  }

  dag::Absorb absorb;
  absorb.Init(m.NPos);
  std::vector<DagCluster> self(lvlRange.size());

  for (int level = 0;; level++) {
    const size_t nClusters = lvlRange.size();
    const size_t firstIdx = out->Clusters.size();
    for (size_t ci = 0; ci < nClusters; ci++) {
      DagCluster c = self[ci];
      c.Level = (uint8_t)level;
      c.ParentErr = kDagRootErr;
      c.First = (uint32_t)out->Idx.size();
      c.Count = lvlRange[ci].second * 3;
      const size_t base = (size_t)lvlRange[ci].first * 3;
      out->Idx.insert(out->Idx.end(), lvlTri.begin() + (long)base,
                      lvlTri.begin() + (long)(base + c.Count));
      if (level == 0) dag::Sphere(m, lvlTri, base, c.Count, c.SelfCenter, &c.SelfRadius);
      out->Clusters.push_back(c);
    }
    out->AllTris += (uint32_t)(lvlTri.size() / 3);
    out->Levels = level + 1;

    if (nClusters <= 1 || lvlTri.size() / 3 <= (size_t)opts.MinLevelTris) break;

    std::vector<uint32_t> nextTri;
    std::vector<std::pair<uint32_t, uint32_t>> nextRange;
    std::vector<DagCluster> nextSelf;
    std::vector<float> pErr, pSphere;
    std::vector<uint32_t> go, gs, gpos;
    const size_t nGroups = (nClusters + (size_t)opts.GroupSize - 1) / (size_t)opts.GroupSize;
    std::vector<std::vector<uint32_t>> group(nGroups);
    std::vector<float> gSphere(nGroups * 4, 0.0f), gChild(nGroups, 0.0f);
    const size_t before = lvlTri.size() / 3;
    size_t after = 0;
    for (size_t g0 = 0, gi = 0; g0 < nClusters; g0 += (size_t)opts.GroupSize, gi++) {
      const size_t g1 = std::min(g0 + (size_t)opts.GroupSize, nClusters);
      std::vector<uint32_t> &gt = group[gi];
      for (size_t ci = g0; ci < g1; ci++) {
        const size_t b = (size_t)lvlRange[ci].first * 3;
        gt.insert(gt.end(), lvlTri.begin() + (long)b,
                  lvlTri.begin() + (long)(b + (size_t)lvlRange[ci].second * 3));
      }
      float gc[3] = {0, 0, 0}, gr = 0.0f;
      dag::Sphere(m, gt, 0, gt.size(), gc, &gr);

      for (size_t ci = g0; ci < g1; ci++) {
        const DagCluster &cc = out->Clusters[firstIdx + ci];
        gChild[gi] = std::max(gChild[gi], cc.SelfErr);
        const double dx = (double)cc.SelfCenter[0] - gc[0], dy = (double)cc.SelfCenter[1] - gc[1],
                     dz = (double)cc.SelfCenter[2] - gc[2];
        gr = std::max(gr, (float)(std::sqrt(dx * dx + dy * dy + dz * dz) + (double)cc.SelfRadius));
      }
      for (int a = 0; a < 3; a++) gSphere[gi * 4 + (size_t)a] = gc[a];
      gSphere[gi * 4 + 3] = gr;
    }

    std::vector<int32_t> owner((size_t)m.NPos, -1);
    std::vector<uint8_t> shared((size_t)m.NPos, 0);
    for (size_t gi = 0; gi < nGroups; gi++)
      for (uint32_t v : group[gi]) {
        const uint32_t p = m.Pos[v];
        if (owner[p] < 0) owner[p] = (int32_t)gi;
        else if (owner[p] != (int32_t)gi) shared[p] = 1;
      }

    for (size_t gi = 0; gi < nGroups; gi++) {
      std::vector<uint32_t> &gt = group[gi];
      const size_t target = (size_t)((double)(gt.size() / 3) * (double)opts.TargetRatio);
      dag::SimplifyGroup(m, gt, target, shared, absorb);
      after += gt.size() / 3;
    }

    std::vector<double> corners;
    corners.reserve(after * 9);
    for (const std::vector<uint32_t> &g : group)
      for (uint32_t v : g) {
        const double *q = &m.PP[(size_t)m.Pos[v] * 3];
        corners.push_back(q[0]); corners.push_back(q[1]); corners.push_back(q[2]);
      }
    dag::DevMesh dm;
    dm.Set(corners, opts.Up);

    for (size_t gi = 0; gi < nGroups; gi++) {
      const std::vector<uint32_t> &gt = group[gi];
      gpos.clear();
      for (uint32_t v : gt) gpos.push_back(m.Pos[v]);
      std::sort(gpos.begin(), gpos.end());
      gpos.erase(std::unique(gpos.begin(), gpos.end()), gpos.end());
      float ge = gChild[gi];
      for (uint32_t pIdx : gpos)
        for (int32_t o = absorb.Head[pIdx]; o >= 0; o = absorb.Next[(size_t)o])
          ge = std::max(ge, (float)dm.Dev(&m.PP[(size_t)o * 3]));
      if (!(ge > gChild[gi])) ge = gChild[gi] + 1.0e-4f;

      pErr.push_back(ge);
      for (int a = 0; a < 4; a++) pSphere.push_back(gSphere[gi * 4 + (size_t)a]);
      if (gt.empty()) continue;

      dag::Partition(m, gt, opts.MaxTrisPerCluster, &go, &gs);
      for (size_t s = 0; s + 1 < gs.size(); s++) {
        const uint32_t first = (uint32_t)(nextTri.size() / 3);
        for (uint32_t i = gs[s]; i < gs[s + 1]; i++)
          for (int k = 0; k < 3; k++) nextTri.push_back(gt[(size_t)go[i] * 3 + (size_t)k]);
        nextRange.emplace_back(first, (uint32_t)(nextTri.size() / 3) - first);
        DagCluster nc{};
        nc.SelfErr = ge;
        for (int a = 0; a < 3; a++) nc.SelfCenter[a] = gSphere[gi * 4 + (size_t)a];
        nc.SelfRadius = gSphere[gi * 4 + 3];
        nextSelf.push_back(nc);
      }
    }
    if (nextRange.empty() || after >= (size_t)((double)before * 0.95)) break;

    for (size_t g0 = 0, gi = 0; g0 < nClusters; g0 += (size_t)opts.GroupSize, gi++) {
      const size_t g1 = std::min(g0 + (size_t)opts.GroupSize, nClusters);
      for (size_t ci = g0; ci < g1; ci++) {
        DagCluster &c = out->Clusters[firstIdx + ci];
        c.ParentErr = pErr[gi];
        for (int a = 0; a < 3; a++) c.ParentCenter[a] = pSphere[gi * 4 + (size_t)a];
        c.ParentRadius = pSphere[gi * 4 + 3];
      }
    }
    lvlTri.swap(nextTri);
    lvlRange.swap(nextRange);
    self.swap(nextSelf);
  }
  return !out->Clusters.empty();
}


}
#endif

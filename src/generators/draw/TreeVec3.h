#ifndef OUTSHINE_GENERATORS_DRAW_TREEVEC3_H
#define OUTSHINE_GENERATORS_DRAW_TREEVEC3_H

#include <cmath>

namespace outshine::Generators {

struct TreeVec3 {
  float X = 0.0f, Y = 0.0f, Z = 0.0f;
};

inline TreeVec3 Vec3(float x, float y, float z) {
  return TreeVec3{.X = x, .Y = y, .Z = z};
}

inline TreeVec3 operator+(TreeVec3 a, TreeVec3 b) {
  return {.X = a.X + b.X, .Y = a.Y + b.Y, .Z = a.Z + b.Z};
}

inline TreeVec3 operator-(TreeVec3 a, TreeVec3 b) {
  return {.X = a.X - b.X, .Y = a.Y - b.Y, .Z = a.Z - b.Z};
}

inline TreeVec3 operator*(TreeVec3 a, float s) {
  return {.X = a.X * s, .Y = a.Y * s, .Z = a.Z * s};
}

inline float Dot(TreeVec3 a, TreeVec3 b) {
  return a.X * b.X + a.Y * b.Y + a.Z * b.Z;
}

inline TreeVec3 Cross(TreeVec3 a, TreeVec3 b) {
  return {.X = a.Y * b.Z - a.Z * b.Y, .Y = a.Z * b.X - a.X * b.Z, .Z = a.X * b.Y - a.Y * b.X};
}

inline float Length(TreeVec3 a) {
  return std::sqrt(Dot(a, a));
}

inline TreeVec3 Normalize(TreeVec3 a) {
  const float l = Length(a);
  return l > 1e-8f ? a * (1.0f / l) : TreeVec3{.X = 0.0f, .Y = 1.0f, .Z = 0.0f};
}

inline void FrameFrom(TreeVec3 t, TreeVec3 ref, TreeVec3 &n, TreeVec3 &b) {
  const TreeVec3 tt = Normalize(t);
  TreeVec3 nn = ref - tt * Dot(ref, tt);
  if (Length(nn) < 1e-5f) {
    const TreeVec3 alt = std::fabs(tt.Y) < 0.9f ? TreeVec3{.X = 0.0f, .Y = 1.0f, .Z = 0.0f}
                                                : TreeVec3{.X = 1.0f, .Y = 0.0f, .Z = 0.0f};
    nn = alt - tt * Dot(alt, tt);
  }
  n = Normalize(nn);
  b = Normalize(Cross(tt, n));
}

inline TreeVec3 RmfDouble(TreeVec3 p0, TreeVec3 p1, TreeVec3 t0, TreeVec3 t1, TreeVec3 x0) {
  const TreeVec3 v1 = p1 - p0;
  const float c1 = Dot(v1, v1);
  if (c1 < 1e-12f) { return x0; }
  const TreeVec3 rL = x0 - v1 * (2.0f / c1 * Dot(v1, x0));
  const TreeVec3 tL = t0 - v1 * (2.0f / c1 * Dot(v1, t0));
  const TreeVec3 v2 = t1 - tL;
  const float c2 = Dot(v2, v2);
  if (c2 < 1e-12f) { return Normalize(rL); }
  return Normalize(rL - v2 * (2.0f / c2 * Dot(v2, rL)));
}

} // namespace outshine::Generators
#endif

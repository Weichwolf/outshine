#ifndef OUTSHINE_GENERATORS_DRAW_TREEFRAME_H
#define OUTSHINE_GENERATORS_DRAW_TREEFRAME_H

#include <cmath>

#include "Vec3.h"

namespace outshine::Generators {

inline Vec3f DirectionOrUp(Vec3f a) {
  const float l = Length(a);
  return l > 1e-8f ? a * (1.0f / l) : Vec3f{{0.0f, 1.0f, 0.0f}};
}

inline void FrameFrom(Vec3f t, Vec3f ref, Vec3f &n, Vec3f &b) {
  const Vec3f tt = DirectionOrUp(t);
  Vec3f nn = ref - tt * Dot(ref, tt);
  if (Length(nn) < 1e-5f) {
    const Vec3f alt =
        std::fabs(tt[1]) < 0.9f ? Vec3f{{0.0f, 1.0f, 0.0f}} : Vec3f{{1.0f, 0.0f, 0.0f}};
    nn = alt - tt * Dot(alt, tt);
  }
  n = DirectionOrUp(nn);
  b = DirectionOrUp(Cross(tt, n));
}

inline Vec3f RmfDouble(Vec3f p0, Vec3f p1, Vec3f t0, Vec3f t1, Vec3f x0) {
  const Vec3f v1 = p1 - p0;
  const float c1 = Dot(v1, v1);
  if (c1 < 1e-12f) { return x0; }
  const Vec3f rL = x0 - v1 * (2.0f / c1 * Dot(v1, x0));
  const Vec3f tL = t0 - v1 * (2.0f / c1 * Dot(v1, t0));
  const Vec3f v2 = t1 - tL;
  const float c2 = Dot(v2, v2);
  if (c2 < 1e-12f) { return DirectionOrUp(rL); }
  return DirectionOrUp(rL - v2 * (2.0f / c2 * Dot(v2, rL)));
}

} // namespace outshine::Generators
#endif

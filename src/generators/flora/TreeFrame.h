#ifndef OUTSHINE_GENERATORS_FLORA_TREEFRAME_H
#define OUTSHINE_GENERATORS_FLORA_TREEFRAME_H

#include <cmath>

#include "math/Vec3.h"

namespace outshine::Generators {

constexpr float kLeastLengthM = 1e-8f;
constexpr float kLeastPerpendicularM = 1e-5f;
constexpr float kLeastChordSquared = 1e-12f;

inline Vec3f DirectionOrUp(Vec3f a) {
  const float l = Length(a);
  return l > kLeastLengthM ? a * (1.0f / l) : Vec3f{{0.0f, 1.0f, 0.0f}};
}

struct Framing {
  Vec3f Along;
  Vec3f Reference;
};

struct Frame {
  Vec3f Normal;
  Vec3f Binormal;
};

[[nodiscard]] inline Frame FrameFrom(Framing from) {
  const Vec3f tt = DirectionOrUp(from.Along);
  Vec3f nn = from.Reference - tt * Dot(from.Reference, tt);
  if (Length(nn) < kLeastPerpendicularM) {
    const Vec3f alt =
        std::fabs(tt[1]) < 0.9f ? Vec3f{{0.0f, 1.0f, 0.0f}} : Vec3f{{1.0f, 0.0f, 0.0f}};
    nn = alt - tt * Dot(alt, tt);
  }
  const Vec3f normal = DirectionOrUp(nn);
  return {.Normal = normal, .Binormal = DirectionOrUp(Cross(tt, normal))};
}

inline Vec3f RmfDouble(Vec3f p0, Vec3f p1, Vec3f t0, Vec3f t1, Vec3f x0) {
  const Vec3f v1 = p1 - p0;
  const float c1 = Dot(v1, v1);
  if (c1 < kLeastChordSquared) { return x0; }
  const Vec3f rL = x0 - v1 * (2.0f / c1 * Dot(v1, x0));
  const Vec3f tL = t0 - v1 * (2.0f / c1 * Dot(v1, t0));
  const Vec3f v2 = t1 - tL;
  const float c2 = Dot(v2, v2);
  if (c2 < kLeastChordSquared) { return DirectionOrUp(rL); }
  return DirectionOrUp(rL - v2 * (2.0f / c2 * Dot(v2, rL)));
}

} // namespace outshine::Generators
#endif

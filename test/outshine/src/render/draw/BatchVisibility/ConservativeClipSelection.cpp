#include "BatchVisibility.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <span>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;

std::array<float, 4> Project(const FrameContext &frame,
                             const Mat4 &row,
                             const Vec3f &point,
                             const Vec3 &anchor = {},
                             bool flat = false) {
  std::array<float, 4> world{0, 0, 0, 1};
  for (size_t axis = 0; axis < 3; ++axis) {
    const float shift = static_cast<float>(anchor[axis] + frame.PreViewTranslation[axis]);
    const float translation = static_cast<float>(row.At(axis, 3));
    world[axis] = static_cast<float>(row.At(axis, 0)) * point[0] +
                  static_cast<float>(row.At(axis, 1)) * point[1] +
                  static_cast<float>(row.At(axis, 2)) * point[2] +
                  (flat ? translation + shift : translation);
    if (!flat) { world[axis] += shift; }
  }
  std::array<float, 4> clip{};
  for (size_t axis = 0; axis < 4; ++axis) {
    clip[axis] = frame.Mvp.At(axis, 0) * world[0] + frame.Mvp.At(axis, 1) * world[1] +
                 frame.Mvp.At(axis, 2) * world[2] + frame.Mvp.At(axis, 3);
  }
  return clip;
}

bool Inside(const std::array<float, 4> &clip) {
  return clip[0] >= -clip[3] && clip[0] <= clip[3] && clip[1] >= -clip[3] && clip[1] <= clip[3];
}

void FixedCases() {
  FrameContext frame;
  const BatchFrustum ortho(frame, {});
  CHECK(!ortho.Outside(BatchSphere{0, 0, 0, 0.1f}), "central orthographic content stays");
  CHECK(ortho.Outside(BatchSphere{2, 0, 0, 0.1f}) && ortho.Outside(BatchSphere{-2, 0, 0, 0.1f}) &&
            ortho.Outside(BatchSphere{0, 2, 0, 0.1f}) && ortho.Outside(BatchSphere{0, -2, 0, 0.1f}),
        "all four lateral sides reject separated content");
  CHECK(!ortho.Outside(BatchSphere{0, 0, 1e10f, 0.1f}), "depth clamp is not redefined");
  CHECK(!ortho.Outside(BatchSphere{1.1f, 0, 0, 0.1f}), "tangent content stays");
  CHECK(!ortho.Outside(std::nullopt), "unknown bounds stay");
  CHECK(!ortho.Outside(BatchSphere{2, 0, 0, -1}), "invalid radius stays");
  CHECK(!ortho.Outside(BatchSphere{2, 0, 0, std::numeric_limits<float>::infinity()}),
        "nonfinite bounds stay");
  frame.PreViewTranslation = {{-100, 0, 0}};
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{100, 0, 0, 0.1f}),
        "camera-relative translation is applied once");
  CHECK(!BatchFrustum(frame, {{100, 0, 0}}).Outside(BatchSphere{0, 0, 0, 0.1f}),
        "scene anchor is added before casting the camera shift");
  frame.PreViewTranslation = {{-1e12, 0, 0}};
  CHECK(!BatchFrustum(frame, {{1e12, 0, 0}}).Outside(BatchSphere{0, 0, 0, 0.1f}),
        "large scene origin and camera cancel in Float64 before Float32 conversion");
  frame.PreViewTranslation[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{200, 0, 0, 0.1f}),
        "invalid camera stays open");
  frame = {};
  frame.Mvp.At(0, 0) = 1e38f;
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{100, 0, 0, 0.1f}),
        "potentially overflowing shader clip arithmetic stays open");
  frame = {};
  frame.Mvp.At(1, 1) = 1e38f;
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{100, 100, 0, 0.1f}),
        "an earlier separating plane cannot bypass a later overflowing component");
  frame = {};
  frame.Mvp.At(3, 3) = 0;
  frame.Mvp.At(3, 2) = -1;
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{0, 0, -10, 0.1f}) &&
            BatchFrustum(frame, {}).Outside(BatchSphere{20, 0, -10, 0.1f}) &&
            BatchFrustum(frame, {}).Outside(BatchSphere{0, 0, 10, 0.1f}),
        "perspective side planes distinguish front, side and rear");
}

void TransformedPoints() {
  uint32_t seed = 1729;
  const auto sample = [&]() {
    seed = seed * 1664525u + 1013904223u;
    return static_cast<float>(seed >> 8) * (2.0f / 16777215.0f) - 1;
  };
  std::array<StoredVertex, 64> vertices{};
  for (auto &vertex : vertices) { vertex.pos = {{sample(), sample(), sample()}}; }
  const auto local = LocalSphereOf(vertices);
  CHECK(local.has_value(), "finite source vertices have a bound");
  bool contains = true;
  bool retainsVisible = true;
  size_t visibleCases = 0;
  size_t rejectedCases = 0;
  for (size_t trial = 0; trial < 2000; ++trial) {
    std::array<Mat4, 2> rows{};
    const double magnitude = trial % 2 == 0 ? 1 : 1e7;
    FrameContext frame;
    Vec3 anchor;
    for (size_t axis = 0; axis < 3; ++axis) {
      frame.PreViewTranslation[axis] = magnitude * sample();
      anchor[axis] = magnitude * sample();
      for (size_t column = 0; column < 4; ++column) { frame.Mvp.At(axis, column) = 2 * sample(); }
      frame.Mvp.At(3, axis) = trial % 3 == 0 ? sample() : 0;
    }
    for (auto &row : rows) {
      for (size_t axis = 0; axis < 3; ++axis) {
        row.At(axis, 3) =
            -static_cast<float>(anchor[axis] + frame.PreViewTranslation[axis]) + 4 * sample();
        if (axis == 0 && trial % 4 == 0) { row.At(axis, 3) += 100; }
        for (size_t column = 0; column < 3; ++column) { row.At(axis, column) = 2 * sample(); }
      }
    }
    const auto sphere = WorldSphereOf(local, rows);
    CHECK(sphere.has_value(), "affine sheared instances have a bound");
    if (!sphere) { continue; }
    const bool rejected = BatchFrustum(frame, anchor).Outside(sphere);
    rejectedCases += rejected;
    bool anyVisible = false;
    for (const auto &row : rows) {
      const auto single = WorldSphereOf(local, std::span(&row, 1));
      const bool singleRejected = BatchFrustum(frame, anchor).Outside(single);
      for (const auto &vertex : vertices) {
        const bool pointVisible = Inside(Project(frame, row, vertex.pos, anchor)) ||
                                  Inside(Project(frame, row, vertex.pos, anchor, true));
        anyVisible |= pointVisible;
        retainsVisible &= !singleRejected || !pointVisible;
        FrameContext identity;
        const auto world = Project(identity, row, vertex.pos);
        contains &= std::hypot(double(world[0]) - (*sphere)[0],
                               double(world[1]) - (*sphere)[1],
                               double(world[2]) - (*sphere)[2]) <= (*sphere)[3];
        contains &= single && std::hypot(double(world[0]) - (*single)[0],
                                         double(world[1]) - (*single)[1],
                                         double(world[2]) - (*single)[2]) <= (*single)[3];
      }
    }
    visibleCases += anyVisible;
    retainsVisible &= !rejected || !anyVisible;
  }
  CHECK(contains, "bounds contain independently transformed Float32 vertices");
  CHECK(retainsVisible && visibleCases > 100, "visible Float32 points are never rejected");
  CHECK(rejectedCases > 0, "random controls exercise real rejection");
}

void InvalidTransforms() {
  std::array<StoredVertex, 2> vertices{};
  vertices[0].pos = {{-1, -1, -1}};
  vertices[1].pos = {{1, 1, 1}};
  const auto local = LocalSphereOf(vertices);
  std::array<Mat4, 1> rows{};
  CHECK(WorldSphereOf(local, rows).has_value(), "single instance is bounded");
  rows[0].At(3, 0) = 0.5;
  CHECK(!WorldSphereOf(local, rows), "projective placement is not falsely treated as affine");
  rows[0] = {};
  rows[0].At(0, 0) = std::numeric_limits<double>::infinity();
  CHECK(!WorldSphereOf(local, rows), "infinite placement is not bounded");
  rows[0] = {};
  rows[0].At(0, 0) = std::numeric_limits<float>::denorm_min();
  CHECK(!WorldSphereOf(local, rows), "FTZ-sensitive placement stays open");
  rows[0] = {};
  rows[0].At(0, 0) = 1e38;
  rows[0].At(0, 1) = -1e38;
  CHECK(!WorldSphereOf(BatchSphere{4, 4, 0, 0.1f}, rows),
        "cancelling Float32 products cannot hide an overflowing intermediate");
  vertices[0].pos[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!LocalSphereOf(vertices), "nonfinite source geometry stays open");
  CHECK(!LocalSphereOf({}) && !WorldSphereOf(local, {}), "empty products have no draw bound");
}
}

int main() {
  FixedCases();
  TransformedPoints();
  InvalidTransforms();
  return Report();
}

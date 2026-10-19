#include "BatchVisibility.h"
#include "Check.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <cstddef>
#include <optional>
#include <limits>
#include <span>

namespace {
using namespace outshine;
using namespace outshine::Render;
using namespace outshine::Test;
constexpr float kClipMargin = 0.1f;
constexpr float kFarClampedDepth = 1e10f;
constexpr float kTangentOffset = 1.1f;
constexpr double kOffsetM = 100;
constexpr double kLargeOriginM = 1e12;
constexpr double kLargeFiniteMagnitude = 1e38;
constexpr float kLargeClip = static_cast<float>(kLargeFiniteMagnitude);
constexpr uint32_t kSampleSeed = 1729;
constexpr uint32_t kSampleMultiplier = 1664525u;
constexpr uint32_t kSampleIncrement = 1013904223u;
constexpr float kSampleMaximum = 16777215.0f;
constexpr size_t kTransformTrials = 2000;
constexpr size_t kMinimumVisibleCases = 100;

std::array<float, 4> Project(const FrameContext &frame,
                             const Mat4 &row,
                             const Vec3f &point,
                             const Vec3 &anchor = {},
                             bool flat = false) {
  std::array<float, 4> world{0, 0, 0, 1};
  for (size_t axis = 0; axis < 3; ++axis) {
    const auto shift = static_cast<float>(anchor[axis] + frame.PreViewTranslation[axis]);
    const auto translation = static_cast<float>(row.At(axis, 3));
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
  CHECK(!ortho.Outside(BatchSphere{0, 0, 0, kClipMargin}), "central orthographic content stays");
  CHECK(ortho.Outside(BatchSphere{2, 0, 0, kClipMargin}) &&
            ortho.Outside(BatchSphere{-2, 0, 0, kClipMargin}) &&
            ortho.Outside(BatchSphere{0, 2, 0, kClipMargin}) &&
            ortho.Outside(BatchSphere{0, -2, 0, kClipMargin}),
        "all four lateral sides reject separated content");
  CHECK(!ortho.Outside(BatchSphere{0, 0, kFarClampedDepth, kClipMargin}),
        "depth clamp is not redefined");
  CHECK(!ortho.Outside(BatchSphere{kTangentOffset, 0, 0, kClipMargin}), "tangent content stays");
  CHECK(!ortho.Outside(std::nullopt), "unknown bounds stay");
  CHECK(!ortho.Outside(BatchSphere{2, 0, 0, -1}), "invalid radius stays");
  CHECK(!ortho.Outside(BatchSphere{2, 0, 0, std::numeric_limits<float>::infinity()}),
        "nonfinite bounds stay");
  frame.PreViewTranslation = {{-kOffsetM, 0, 0}};
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{kOffsetM, 0, 0, kClipMargin}),
        "camera-relative translation is applied once");
  CHECK(!BatchFrustum(frame, {{kOffsetM, 0, 0}}).Outside(BatchSphere{0, 0, 0, kClipMargin}),
        "scene anchor is added before casting the camera shift");
  frame.PreViewTranslation = {{-kLargeOriginM, 0, 0}};
  CHECK(!BatchFrustum(frame, {{kLargeOriginM, 0, 0}}).Outside(BatchSphere{0, 0, 0, kClipMargin}),
        "large scene origin and camera cancel in Float64 before Float32 conversion");
  frame.PreViewTranslation[0] = std::numeric_limits<double>::quiet_NaN();
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{200, 0, 0, kClipMargin}),
        "invalid camera stays open");
  frame = {};
  frame.Mvp.At(0, 0) = kLargeClip;
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{kOffsetM, 0, 0, kClipMargin}),
        "potentially overflowing shader clip arithmetic stays open");
  frame = {};
  frame.Mvp.At(1, 1) = kLargeClip;
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{kOffsetM, 100, 0, kClipMargin}),
        "an earlier separating plane cannot bypass a later overflowing component");
  frame = {};
  frame.Mvp.At(3, 3) = 0;
  frame.Mvp.At(3, 2) = -1;
  CHECK(!BatchFrustum(frame, {}).Outside(BatchSphere{0, 0, -10, kClipMargin}) &&
            BatchFrustum(frame, {}).Outside(BatchSphere{20, 0, -10, kClipMargin}) &&
            BatchFrustum(frame, {}).Outside(BatchSphere{0, 0, 10, kClipMargin}),
        "perspective side planes distinguish front, side and rear");
}

struct PointChecks {
  bool Contains = true;
  bool Retains = true;
  bool Visible = false;
};

PointChecks PlacementPoints(std::span<const StoredVertex> vertices,
                            const FrameContext &frame,
                            const Mat4 &row,
                            const Vec3 &anchor,
                            const std::optional<BatchSphere> &local,
                            const BatchSphere &aggregate) {
  PointChecks result;
  const auto single = WorldSphereOf(local, std::span(&row, 1));
  const bool singleRejected = BatchFrustum(frame, anchor).Outside(single);
  for (const auto &vertex : vertices) {
    const bool pointVisible = Inside(Project(frame, row, vertex.pos, anchor)) ||
                              Inside(Project(frame, row, vertex.pos, anchor, true));
    result.Visible |= pointVisible;
    result.Retains &= !singleRejected || !pointVisible;
    const FrameContext identity;
    const auto world = Project(identity, row, vertex.pos);
    result.Contains &= std::hypot(static_cast<double>(world[0]) - aggregate[0],
                                  static_cast<double>(world[1]) - aggregate[1],
                                  static_cast<double>(world[2]) - aggregate[2]) <= aggregate[3];
    result.Contains &=
        single && std::hypot(static_cast<double>(world[0]) - (*single)[0],
                             static_cast<double>(world[1]) - (*single)[1],
                             static_cast<double>(world[2]) - (*single)[2]) <= (*single)[3];
  }
  return result;
}

void RandomFrame(FrameContext &frame, Vec3 &anchor, size_t trial, const auto &sample) {
  const double magnitude = trial % 2 == 0 ? 1 : 1e7;
  for (size_t axis = 0; axis < 3; ++axis) {
    frame.PreViewTranslation[axis] = magnitude * sample();
    anchor[axis] = magnitude * sample();
    for (size_t column = 0; column < 4; ++column) { frame.Mvp.At(axis, column) = 2 * sample(); }
    frame.Mvp.At(3, axis) = trial % 3 == 0 ? sample() : 0;
  }
}

void RandomPlacements(std::span<Mat4> rows,
                      const FrameContext &frame,
                      const Vec3 &anchor,
                      size_t trial,
                      const auto &sample) {
  for (auto &row : rows) {
    for (size_t axis = 0; axis < 3; ++axis) {
      row.At(axis, 3) =
          -static_cast<float>(anchor[axis] + frame.PreViewTranslation[axis]) + 4 * sample();
      if (axis == 0 && trial % 4 == 0) { row.At(axis, 3) += kOffsetM; }
      for (size_t column = 0; column < 3; ++column) { row.At(axis, column) = 2 * sample(); }
    }
  }
}

void TransformedPoints() {
  uint32_t seed = kSampleSeed;
  const auto sample = [&] {
    seed = seed * kSampleMultiplier + kSampleIncrement;
    return static_cast<float>(seed >> 8) * (2.0f / kSampleMaximum) - 1;
  };
  std::array<StoredVertex, 64> vertices{};
  for (auto &vertex : vertices) { vertex.pos = {{sample(), sample(), sample()}}; }
  const auto local = LocalSphereOf(vertices);
  CHECK(local.has_value(), "finite source vertices have a bound");
  bool contains = true;
  bool retainsVisible = true;
  size_t visibleCases = 0;
  size_t rejectedCases = 0;
  for (size_t trial = 0; trial < kTransformTrials; ++trial) {
    std::array<Mat4, 2> rows{};
    FrameContext frame;
    Vec3 anchor;
    RandomFrame(frame, anchor, trial, sample);
    RandomPlacements(rows, frame, anchor, trial, sample);
    const auto sphere = WorldSphereOf(local, rows);
    CHECK(sphere.has_value(), "affine sheared instances have a bound");
    if (!sphere) { continue; }
    const bool rejected = BatchFrustum(frame, anchor).Outside(sphere);
    if (rejected) { ++rejectedCases; }
    bool anyVisible = false;
    for (const auto &row : rows) {
      const auto result = PlacementPoints(vertices, frame, row, anchor, local, *sphere);
      anyVisible |= result.Visible;
      retainsVisible &= result.Retains;
      contains &= result.Contains;
    }
    if (anyVisible) { ++visibleCases; }
    retainsVisible &= !rejected || !anyVisible;
  }
  CHECK(contains, "bounds contain independently transformed Float32 vertices");
  CHECK(retainsVisible && visibleCases > kMinimumVisibleCases,
        "visible Float32 points are never rejected");
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
  rows[0].At(0, 0) = kLargeFiniteMagnitude;
  rows[0].At(0, 1) = -kLargeFiniteMagnitude;
  CHECK(!WorldSphereOf(BatchSphere{4, 4, 0, kClipMargin}, rows),
        "cancelling Float32 products cannot hide an overflowing intermediate");
  vertices[0].pos[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!LocalSphereOf(vertices), "nonfinite source geometry stays open");
  CHECK(!LocalSphereOf({}) && !WorldSphereOf(local, {}), "empty products have no draw bound");
}

void ClusterBounds() {
  constexpr float firstCentre = -8;
  constexpr float secondCentre = 8;
  constexpr float firstRadius = 3;
  constexpr float secondRadius = 4;
  constexpr size_t corners = 3;
  std::array<DagCluster, 2> clusters{
      {{.SelfCenter = {{firstCentre, 0, 0}}, .SelfRadius = firstRadius, .Count = corners},
       {.SelfCenter = {{secondCentre, 0, 0}},
        .SelfRadius = secondRadius,
        .SelfErr = kDagRootErr,
        .First = corners,
        .Count = corners,
        .Level = 1}}};
  const auto bound = LocalSphereOf(clusters, corners * clusters.size());
  CHECK(bound.has_value(), "complete index coverage across all LOD levels has a local bound");
  if (!bound) { return; }
  bool contained = true;
  for (const auto &cluster : clusters) {
    const double reach = std::hypot(static_cast<double>(cluster.SelfCenter[0]) - (*bound)[0],
                                    static_cast<double>(cluster.SelfCenter[1]) - (*bound)[1],
                                    static_cast<double>(cluster.SelfCenter[2]) - (*bound)[2]) +
                         cluster.SelfRadius;
    contained &= reach <= (*bound)[3];
  }
  CHECK(contained, "packed whole-piece sphere contains every complete input sphere");
  CHECK(!LocalSphereOf(clusters, corners * (clusters.size() + 1)),
        "unclustered trailing shadow indices remain unbounded");
  clusters[1].First = corners * 2;
  CHECK(!LocalSphereOf(clusters, corners * 3), "an uncovered index gap remains unbounded");
  clusters[1].First = corners;
  clusters[1].Count = corners + 1;
  CHECK(!LocalSphereOf(clusters, corners * 3), "partial triangle coverage remains unbounded");
  clusters[1].Count = corners;
  clusters[1].SelfRadius = -1;
  CHECK(!LocalSphereOf(clusters, corners * 2), "negative input radius remains unbounded");
  clusters[1].SelfRadius = std::numeric_limits<float>::denorm_min();
  CHECK(!LocalSphereOf(clusters, corners * 2), "FTZ-sensitive input radius remains unbounded");
  clusters[1].SelfRadius = secondRadius;
  clusters[1].SelfCenter[0] = std::numeric_limits<float>::quiet_NaN();
  CHECK(!LocalSphereOf(clusters, corners * 2), "nonfinite input centre remains unbounded");
  clusters[1].SelfCenter[0] = std::numeric_limits<float>::max();
  clusters[1].SelfRadius = std::numeric_limits<float>::max();
  CHECK(!LocalSphereOf(clusters, corners * 2), "unrepresentable union remains unbounded");
  CHECK(!LocalSphereOf(std::span<const DagCluster>{}, 0), "empty hierarchy remains unbounded");
}
}

int main() {
  FixedCases();
  TransformedPoints();
  InvalidTransforms();
  ClusterBounds();
  return Report();
}

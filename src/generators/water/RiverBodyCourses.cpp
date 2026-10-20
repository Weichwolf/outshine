#include "RiverBodyPlan.h"
#include "WaterAsset.h"
#include "TangentFrame.h"
#include "Geodesy.h"
#include "math/Box.h"
#include "math/Vec3.h"
#include "math/Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Point = std::array<double, 2>;

bool SameRiverSource(const WaterAsset &water,
                     const WaterAsset::Input &surface,
                     const WaterAsset::Course &course) {
  const auto *input = water.InputOf(course);
  if (input == nullptr || input->Kind != surface.Kind || input->Bridge || input->Tunnel ||
      !input->Crossing.empty() || input->Source >= water.Sources().size()) {
    return false;
  }
  const auto &a = water.Sources()[surface.Source].Identity;
  const auto &b = water.Sources()[input->Source].Identity;
  return a.From == b.From && a.Kind == b.Kind && a.SourceId == b.SourceId &&
         a.Revision == b.Revision;
}

std::optional<double> SegmentCoverage(const WaterSurfaceTopology &topology,
                                      uint32_t face,
                                      const std::array<Point, 2> &segment) {
  const auto &from = segment[0];
  const auto &to = segment[1];
  const auto &triangle = topology.Faces[face].Vertices;
  double low = 0;
  double high = 1;
  for (size_t edge = 0; edge < triangle.size(); ++edge) {
    const auto &a = topology.VerticesXY[triangle[edge]];
    const auto &b = topology.VerticesXY[triangle[(edge + 1) % triangle.size()]];
    const double dx = b[0] - a[0];
    const double dy = b[1] - a[1];
    const double first = dx * (from[1] - a[1]) - dy * (from[0] - a[0]);
    const double last = dx * (to[1] - a[1]) - dy * (to[0] - a[0]);
    if (first < 0 && last < 0) { return std::nullopt; }
    if (first < 0) { low = std::max(low, first / (first - last)); }
    if (last < 0) { high = std::min(high, first / (first - last)); }
    if (!(high > low)) { return std::nullopt; }
  }
  return low;
}

std::optional<Point> PlacedPoint(const WaterAsset &water, const TangentFrame &frame, size_t point) {
  const auto points = water.Points();
  if (point >= points.size() / 2) { return std::nullopt; }
  const LongitudeLatitude geographic{.LongitudeDeg = points[point * 2 + 1],
                                     .LatitudeDeg = points[point * 2]};
  if (!std::isfinite(geographic.LongitudeDeg) || !std::isfinite(geographic.LatitudeDeg) ||
      std::abs(geographic.LongitudeDeg) > kDegPerHalfTurn ||
      std::abs(geographic.LatitudeDeg) > kDegPerHalfTurn / 2) {
    return std::nullopt;
  }
  const auto axes =
      EnuAxesEcef({.LongitudeDeg = geographic.LongitudeDeg, .LatitudeDeg = geographic.LatitudeDeg});
  if (frame.ToLocalDirection(axes.Up).UpM <= 0) { return std::nullopt; }
  const auto xy = frame.ToLocalGroundPosition(geographic);
  if (!std::isfinite(xy.EastM) || !std::isfinite(xy.NorthM)) { return std::nullopt; }
  return Point{xy.EastM, xy.NorthM};
}

bool CoversCourse(const WaterAsset &water,
                  const WaterAsset::Course &course,
                  const TangentFrame &frame,
                  RiverBody &body,
                  uint64_t &tests) {
  if (course.PointCount < 2 || course.FirstPoint > water.Points().size() / 2 ||
      course.PointCount > water.Points().size() / 2 - course.FirstPoint) {
    return false;
  }
  auto from = PlacedPoint(water, frame, course.FirstPoint);
  for (uint32_t at = 1; at < course.PointCount; ++at) {
    const auto to = PlacedPoint(water, frame, static_cast<size_t>(course.FirstPoint) + at);
    if (!from || !to) { return false; }
    if (*from != *to) {
      const auto hit =
          body.FaceIndex.TraceClosest({{(*from)[0], (*from)[1], 0}},
                                      {{(*to)[0] - (*from)[0], (*to)[1] - (*from)[1], 0}},
                                      0,
                                      1,
                                      [&](uint32_t face, double) {
                                        ++tests;
                                        return SegmentCoverage(body.Surface, face, {*from, *to});
                                      });
      if (hit) { return true; }
    }
    from = to;
  }
  return false;
}

std::expected<PlanHierarchy, WaterTopologyError> FaceIndex(const WaterSurfaceTopology &surface) {
  std::vector<Box> bounds;
  bounds.reserve(surface.Faces.size());
  for (const auto &face : surface.Faces) {
    Box box;
    for (const auto vertex : face.Vertices) {
      const auto &xy = surface.VerticesXY[vertex];
      box.Cover(Vec3{{xy[0], xy[1], 0}});
    }
    bounds.push_back(box);
  }
  auto hierarchy = PlanHierarchy::Build(bounds);
  if (!hierarchy) { return std::unexpected(WaterTopologyError::InvalidGeometry); }
  return std::move(*hierarchy);
}
}

std::expected<void, WaterTopologyError> BindRiverCourses(const WaterAsset &water,
                                                         const TangentFrame &frame,
                                                         RiverBodyPlan &plan,
                                                         const std::stop_token &stop) {
  for (auto &body : plan.Bodies) {
    if (stop.stop_requested()) { return std::unexpected(WaterTopologyError::Canceled); }
    auto index = FaceIndex(body.Surface);
    if (!index) { return std::unexpected(index.error()); }
    body.FaceIndex = std::move(*index);
    const auto source = body.Surface.FaceSources.front();
    const auto *input = water.InputOf(water.Surfaces()[source]);
    for (uint32_t at = 0; at < water.Courses().size(); ++at) {
      const auto &course = water.Courses()[at];
      if (SameRiverSource(water, *input, course) &&
          CoversCourse(water, course, frame, body, plan.CourseTriangleTests)) {
        body.Courses.push_back(at);
      }
    }
  }
  return {};
}

std::optional<double> WaterCourseFaceEntry(const WaterSurfaceTopology &topology,
                                           uint32_t face,
                                           const std::array<std::array<double, 2>, 2> &segment) {
  return SegmentCoverage(topology, face, segment);
}
}

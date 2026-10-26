#include "RiverBodyHeight.h"
#include "WaterCourseProfile.h"
#include "WaterPhysicalFrame.h"
#include "Box.h"
#include "Vec3.h"
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
using Error = WaterHeightError;

struct Courses {
  std::vector<WaterCourseSegment> Segments;
  std::vector<Vec3> Directions;
};

std::expected<Courses, Error>
ReadCourses(const WaterAsset &water, const TangentFrame &frame, const RiverBody &body) {
  Courses result;
  for (const auto index : body.Courses) {
    const auto &course = water.Courses()[index];
    if (course.FirstLevel > water.Levels().size() ||
        course.PointCount > water.Levels().size() - course.FirstLevel) {
      return std::unexpected(Error::InvalidInput);
    }
    for (uint32_t at = 1; at < course.PointCount; ++at) {
      if (result.Segments.size() == WaterCourseProfile::MaximumSegments) {
        return std::unexpected(Error::CapacityExceeded);
      }
      std::array<Vec3, 2> placed;
      WaterCourseSegment segment;
      for (size_t end = 0; end < placed.size(); ++end) {
        const size_t vertex = static_cast<size_t>(at - 1) + end;
        const size_t point = (static_cast<size_t>(course.FirstPoint) + vertex) * 2;
        const auto enu = frame.ToLocalPosition({.LongitudeDeg = water.Points()[point + 1],
                                                .LatitudeDeg = water.Points()[point],
                                                .HeightM = 0});
        placed[end] = {{enu.EastM, enu.NorthM, enu.UpM}};
        segment.Points[end] = {.EastM = enu.EastM, .NorthM = enu.NorthM};
        segment.ModelHeightsM[end] =
            water.Levels()[static_cast<size_t>(course.FirstLevel) + vertex];
      }
      if (segment.Points[0].EastM == segment.Points[1].EastM &&
          segment.Points[0].NorthM == segment.Points[1].NorthM) {
        continue;
      }
      const auto direction = placed[1] - placed[0];
      result.Segments.push_back(segment);
      result.Directions.push_back(direction);
    }
  }
  return result;
}

bool Overlap(const Box &a, const Box &b) {
  return a.Min[0] <= b.Max[0] && b.Min[0] <= a.Max[0] && a.Min[1] <= b.Max[1] &&
         b.Min[1] <= a.Max[1];
}

std::expected<void, Error> CourseGrades(const Courses &courses,
                                        const RiverBody &body,
                                        const RiverHeightBudget &budget,
                                        std::vector<WaterGradeConstraint> &grades) {
  const auto &surface = body.Surface;
  for (size_t at = 0; at < courses.Segments.size(); ++at) {
    const auto &points = courses.Segments[at].Points;
    const std::array<std::array<double, 2>, 2> segment{
        {{points[0].EastM, points[0].NorthM}, {points[1].EastM, points[1].NorthM}}};
    Box bounds;
    bounds.Cover(Vec3{{points[0].EastM, points[0].NorthM, 0}});
    bounds.Cover(Vec3{{points[1].EastM, points[1].NorthM, 0}});
    std::optional<Error> failure;
    body.FaceIndex.SelectNodes(
        [&](const PlanHierarchy::Node &node) {
          return failure.has_value() || !Overlap(bounds, node.Bounds);
        },
        [&](const PlanHierarchy::Node &node) {
          if (failure || node.Count != 1 || !Overlap(bounds, node.Bounds)) { return; }
          const auto face = body.FaceIndex.SourceOrder()[node.First];
          if (!WaterCourseFaceEntry(surface, face, segment)) { return; }
          const uint64_t rows = 2 * (static_cast<uint64_t>(grades.size()) + 1) +
                                surface.Faces.size() + 2 * surface.VerticesXY.size();
          if (rows > budget.Fit.Rows) {
            failure = Error::CapacityExceeded;
            return;
          }
          grades.push_back({.Face = face,
                            .DirectionEnu = courses.Directions[at],
                            .Lower = -budget.DownGrade,
                            .Upper = budget.DownGrade});
        });
    if (failure) { return std::unexpected(*failure); }
  }
  return {};
}

std::expected<std::vector<WaterGradeConstraint>, Error>
PrimaryGrades(const Courses &courses,
              const WaterCourseProfile &directionProfile,
              const RiverBody &body,
              const TangentFrame &frame,
              const RiverHeightBudget &budget,
              std::array<double, 2> bounds) {
  const auto &surface = body.Surface;
  const size_t count = surface.Faces.size();
  std::vector<WaterGradeConstraint> grades(count * 2);
  for (uint32_t at = 0; at < count; ++at) {
    std::array<std::array<double, 2>, 3> triangle;
    EastNorth center;
    for (size_t vertex = 0; vertex < triangle.size(); ++vertex) {
      triangle[vertex] = surface.VerticesXY[surface.Faces[at].Vertices[vertex]];
      center.EastM += triangle[vertex][0] / 3;
      center.NorthM += triangle[vertex][1] / 3;
    }
    const auto nearest = directionProfile.Sample(center);
    const auto cap = BoundWaterFacetNormals(frame, triangle, bounds);
    if (!nearest || !cap) { return std::unexpected(Error::OutsidePhysicalDomain); }
    const auto direction = courses.Directions[nearest->NearestSegment];
    const auto tangent = direction - cap->Up * Dot(direction, cap->Up);
    const double length = Length(tangent);
    if (!std::isfinite(length) || length <= 0) { return std::unexpected(Error::InvalidInput); }
    grades[at] = {.Face = at,
                  .DirectionEnu = direction,
                  .Lower = -budget.DownGrade,
                  .Upper = budget.DownGrade};
    grades[at + count] = {.Face = at,
                          .DirectionEnu = Cross(cap->Up, tangent * (1 / length)),
                          .Lower = -budget.CrossGrade,
                          .Upper = budget.CrossGrade};
  }
  return grades;
}
}

std::expected<WaterHeightFit, WaterHeightError> FitRiverBody(const WaterAsset &water,
                                                             const TangentFrame &frame,
                                                             const RiverBody &body,
                                                             RiverHeightBudget budget,
                                                             const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto &surface = body.Surface;
  if (surface.VerticesXY.empty() || surface.Faces.empty() || body.Courses.empty() ||
      !std::isfinite(budget.DownGrade) || budget.DownGrade <= 0 ||
      !std::isfinite(budget.CrossGrade) || budget.CrossGrade <= 0 ||
      !std::isfinite(budget.TargetMarginM) || budget.TargetMarginM <= 0) {
    return std::unexpected(Error::InvalidInput);
  }
  if (surface.VerticesXY.size() > budget.Fit.Vertices || surface.Faces.size() > budget.Fit.Faces ||
      5 * static_cast<uint64_t>(surface.Faces.size()) + 2 * surface.VerticesXY.size() >
          budget.Fit.Rows) {
    return std::unexpected(Error::CapacityExceeded);
  }
  const auto courses = ReadCourses(water, frame, body);
  if (!courses) { return std::unexpected(courses.error()); }
  const auto profile = WaterCourseProfile::Build(courses->Segments);
  if (!profile) { return std::unexpected(Error::InvalidInput); }
  std::vector<double> targets;
  targets.reserve(surface.VerticesXY.size());
  for (const auto &xy : surface.VerticesXY) {
    if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
    const auto sample = profile->Sample({.EastM = xy[0], .NorthM = xy[1]});
    if (!sample) { return std::unexpected(Error::CapacityExceeded); }
    const auto target =
        WaterHeightTarget(frame, {.EastM = xy[0], .NorthM = xy[1]}, sample->ModelHeightM);
    if (!target) { return std::unexpected(Error::OutsidePhysicalDomain); }
    targets.push_back(*target);
  }
  const auto range = std::ranges::minmax_element(targets);
  const std::array<double, 2> bounds{*range.min - budget.TargetMarginM,
                                     *range.max + budget.TargetMarginM};
  auto grades = PrimaryGrades(*courses, *profile, body, frame, budget, bounds);
  if (!grades) { return std::unexpected(grades.error()); }
  const auto extra = CourseGrades(*courses, body, budget, *grades);
  if (!extra) { return std::unexpected(extra.error()); }
  std::vector<std::array<uint32_t, 2>> primary(surface.Faces.size());
  for (uint32_t at = 0; at < primary.size(); ++at) {
    primary[at] = {at, at + static_cast<uint32_t>(primary.size())};
  }
  return FitWaterSurfaceHeights({.Frame = frame,
                                 .VerticesXY = surface.VerticesXY,
                                 .Faces = surface.Faces,
                                 .TargetUpM = targets,
                                 .Grades = *grades,
                                 .PrimaryGrades = primary,
                                 .UpBoundsM = bounds,
                                 .Contacts = {}},
                                budget.Fit,
                                stop);
}
}

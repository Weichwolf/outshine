#include "RoadGroundFacetInternal.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>

namespace outshine::Generators::RoadGround {
namespace {
constexpr double kBarycentricTolerance = 1e-12;
constexpr double kCoordinateMostM = 1e6;
constexpr double kHeightEqualityToleranceM = 1e-7;
constexpr double kHeightChordMostM = .01;
constexpr double kNormalizedGradeMost = 1.0000001;

double Area(EastNorth a, EastNorth b, EastNorth c) noexcept {
  return (b.EastM - a.EastM) * (c.NorthM - a.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - a.EastM);
}

bool Contains(const std::array<EastNorth, 3> &triangle, EastNorth at) noexcept {
  const double area = Area(triangle[0], triangle[1], triangle[2]);
  if (!(area > 0) || !std::isfinite(area)) { return false; }
  const double u = Area(triangle[0], at, triangle[2]) / area;
  const double v = Area(triangle[0], triangle[1], at) / area;
  return u >= -kBarycentricTolerance && v >= -kBarycentricTolerance &&
         u + v <= 1 + kBarycentricTolerance;
}
}

std::expected<Facet, RoadGroundError> CheckFacet(const RoadPortFace &source,
                                                 const RoadPortField &field,
                                                 const PlanarFace &indices,
                                                 std::span<const PlanarPoint> points,
                                                 std::span<const double> heightsM,
                                                 RoadGroundFacetAudit &audit,
                                                 size_t at) noexcept {
  std::array<EastNorth, 3> triangle;
  std::array<double, 3> heights;
  for (size_t corner = 0; corner < 3; ++corner) {
    const auto index = indices[corner];
    if (index >= points.size()) { return std::unexpected(RoadGroundError::InvalidGeometry); }
    triangle[corner] = {.EastM = points[index][0], .NorthM = points[index][1]};
    heights[corner] = heightsM[index];
    const auto &p = triangle[corner];
    if (!std::isfinite(p.EastM) || !std::isfinite(p.NorthM) || !std::isfinite(heights[corner]) ||
        std::abs(p.EastM) > kCoordinateMostM || std::abs(p.NorthM) > kCoordinateMostM ||
        std::abs(heights[corner]) > kCoordinateMostM || !Contains(source.Triangle, p)) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
    const auto sampled = field.Evaluate(source.Patch, p);
    if (!sampled || std::abs(sampled->HeightM - heights[corner]) > kHeightEqualityToleranceM) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
  }
  const double area = Area(triangle[0], triangle[1], triangle[2]);
  if (!(area > 0) || !std::isfinite(area)) {
    return std::unexpected(RoadGroundError::InvalidGeometry);
  }
  const auto bound = field.Bounds(source.Patch, triangle, source.GradeFrame);
  if (!bound) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  const double east = triangle[1].EastM - triangle[0].EastM;
  const double north = triangle[1].NorthM - triangle[0].NorthM;
  const double otherEast = triangle[2].EastM - triangle[0].EastM;
  const double otherNorth = triangle[2].NorthM - triangle[0].NorthM;
  const double height = heights[1] - heights[0];
  const double otherHeight = heights[2] - heights[0];
  const RoadPlane gradient{.SlopeE = (height * otherNorth - otherHeight * north) / area,
                           .SlopeN = (east * otherHeight - otherEast * height) / area};
  const auto &frame = source.GradeFrame;
  const double grade = std::hypot(frame[0] * gradient.SlopeE + frame[1] * gradient.SlopeN,
                                  frame[2] * gradient.SlopeE + frame[3] * gradient.SlopeN);
  if (!std::isfinite(grade)) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  audit.MaximumHeightChordErrorM =
      std::max(audit.MaximumHeightChordErrorM, bound->HeightChordErrorM);
  audit.MaximumNormalizedContinuousGrade =
      std::max(audit.MaximumNormalizedContinuousGrade, bound->NormalizedGradientMaximum);
  audit.MaximumNormalizedFiniteGrade = std::max(audit.MaximumNormalizedFiniteGrade, grade);
  if (bound->HeightChordErrorM > kHeightChordMostM ||
      bound->NormalizedGradientMaximum > kNormalizedGradeMost || grade > kNormalizedGradeMost) {
    audit.Refinement[at] = 1;
  }
  return Facet{.Gradient = gradient, .Patch = source.Patch};
}
}

#include "RoadPortFieldQuery.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>

namespace outshine::Generators {
namespace {
constexpr double kCoordinateMostM = 1e6;
constexpr double kBarycentricTolerance = 1e-12;

bool Valid(EastNorth at) noexcept {
  return std::isfinite(at.EastM) && std::isfinite(at.NorthM) &&
         std::abs(at.EastM) <= kCoordinateMostM && std::abs(at.NorthM) <= kCoordinateMostM;
}

double Determinant(EastNorth a, EastNorth b, EastNorth c) noexcept {
  return (b.EastM - a.EastM) * (c.NorthM - a.NorthM) - (b.NorthM - a.NorthM) * (c.EastM - a.EastM);
}

bool Contains(const std::array<EastNorth, 3> &triangle, EastNorth at) noexcept {
  const auto [minE, maxE] = std::minmax({triangle[0].EastM, triangle[1].EastM, triangle[2].EastM});
  const auto [minN, maxN] =
      std::minmax({triangle[0].NorthM, triangle[1].NorthM, triangle[2].NorthM});
  if (at.EastM < minE || at.EastM > maxE || at.NorthM < minN || at.NorthM > maxN) { return false; }
  const double det = Determinant(triangle[0], triangle[1], triangle[2]);
  const double u = Determinant(triangle[0], at, triangle[2]) / det;
  const double v = Determinant(triangle[0], triangle[1], at) / det;
  return u >= -kBarycentricTolerance && v >= -kBarycentricTolerance &&
         u + v <= 1 + kBarycentricTolerance;
}
}

RoadPortFieldQuery::RoadPortFieldQuery(const RoadPortField &field,
                                       std::span<const RoadPortFace> faces,
                                       RoadPortQueryBudget budget) noexcept
    : Field_(&field), Faces_(faces), Budget_(budget) {}

std::expected<RoadPortFieldQuery, RoadPortQueryError>
RoadPortFieldQuery::Make(const RoadPortField &field,
                         std::span<const RoadPortFace> faces,
                         RoadPortQueryBudget budget) noexcept {
  if (faces.size() > budget.Faces) { return std::unexpected(RoadPortQueryError::CapacityExceeded); }
  if (faces.empty()) { return std::unexpected(RoadPortQueryError::InvalidInput); }
  for (const auto &face : faces) {
    for (const auto at : face.Triangle) {
      if (!Valid(at)) { return std::unexpected(RoadPortQueryError::InvalidInput); }
      if (!field.Evaluate(face.Patch, at)) {
        return std::unexpected(RoadPortQueryError::InvalidField);
      }
    }
    if (Determinant(face.Triangle[0], face.Triangle[1], face.Triangle[2]) == 0) {
      return std::unexpected(RoadPortQueryError::InvalidInput);
    }
  }
  return RoadPortFieldQuery(field, faces, budget);
}

std::expected<RoadPortField::Sample, RoadPortQueryError>
RoadPortFieldQuery::At(EastNorth at) noexcept {
  if (!Valid(at)) { return std::unexpected(RoadPortQueryError::InvalidInput); }
  if (Work_.Queries >= Budget_.Queries ||
      Faces_.size() > Budget_.FaceBoundsTests - Work_.FaceBoundsTests) {
    return std::unexpected(RoadPortQueryError::CapacityExceeded);
  }
  ++Work_.Queries;
  Work_.FaceBoundsTests += Faces_.size();
  std::optional<uint32_t> patch;
  for (const auto &face : Faces_) {
    if (Contains(face.Triangle, at) && (!patch || face.Patch < *patch)) { patch = face.Patch; }
  }
  if (!patch) { return std::unexpected(RoadPortQueryError::OutsideDomain); }
  const auto sample = Field_->Evaluate(*patch, at);
  if (!sample) { return std::unexpected(RoadPortQueryError::InvalidField); }
  return *sample;
}
}

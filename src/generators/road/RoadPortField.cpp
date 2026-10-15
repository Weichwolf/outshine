#include "RoadPortField.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <span>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr size_t kPlanesMost = 16384;
constexpr size_t kPatchesMost = 8192;
constexpr double kCoordinateMostM = 4.0e7;
constexpr double kScaleLeastM = 1.0e-8;
constexpr double kDenominatorLeast = 1.0e-8;
constexpr double kUnitNormalTolerance = 1.0e-12;

bool Bounded(double value) {
  return std::isfinite(value) && std::abs(value) <= kCoordinateMostM;
}

bool Bounded(EastNorth at) {
  return Bounded(at.EastM) && Bounded(at.NorthM);
}

bool Valid(const RoadPortField::Plane &plane) {
  return Bounded(plane.Origin) && Bounded(plane.HeightM) && Bounded(plane.Gradient.SlopeE) &&
         Bounded(plane.Gradient.SlopeN);
}

bool Valid(const RoadPortField::Patch &patch, size_t planes) {
  if (patch.Parents[0] >= planes || patch.Parents[1] >= planes || !std::isfinite(patch.ScaleM) ||
      patch.ScaleM < kScaleLeastM || patch.ScaleM > kCoordinateMostM) {
    return false;
  }
  for (size_t end = 0; end < 2; ++end) {
    const auto normal = patch.InwardNormals[end];
    if (!Bounded(patch.CutCentres[end]) || !std::isfinite(normal.EastM) ||
        !std::isfinite(normal.NorthM)) {
      return false;
    }
    if (patch.Parents[0] != patch.Parents[1] &&
        std::abs(std::hypot(normal.EastM, normal.NorthM) - 1.0) > kUnitNormalTolerance) {
      return false;
    }
  }
  return true;
}

double HeightAt(const RoadPortField::Plane &plane, EastNorth at) {
  return plane.HeightM + plane.Gradient.SlopeE * (at.EastM - plane.Origin.EastM) +
         plane.Gradient.SlopeN * (at.NorthM - plane.Origin.NorthM);
}

struct Blend {
  double Weight;
  RoadPlane Gradient;
  double Denominator = 1.0;
};

std::expected<Blend, RoadPortField::Error> BlendAt(const RoadPortField::Patch &patch,
                                                   EastNorth at) {
  std::array<double, 2> signedDistance{};
  std::array<RoadPlane, 2> gradient{};
  for (size_t end = 0; end < 2; ++end) {
    gradient[end] = {.SlopeE = patch.InwardNormals[end].EastM / patch.ScaleM,
                     .SlopeN = patch.InwardNormals[end].NorthM / patch.ScaleM};
    signedDistance[end] = (at.EastM - patch.CutCentres[end].EastM) * gradient[end].SlopeE +
                          (at.NorthM - patch.CutCentres[end].NorthM) * gradient[end].SlopeN;
  }
  const double u = signedDistance[0];
  const double denominator = u + signedDistance[1];
  if (!(denominator > kDenominatorLeast)) {
    return std::unexpected(RoadPortField::Error::NonpositiveDenominator);
  }
  const double t = u / denominator;
  const double factor = 6.0 * t * (1.0 - t) / (denominator * denominator);
  return Blend{.Weight = t * t * (3.0 - 2.0 * t),
               .Gradient = {.SlopeE = factor * (gradient[0].SlopeE * denominator -
                                                u * (gradient[0].SlopeE + gradient[1].SlopeE)),
                            .SlopeN = factor * (gradient[0].SlopeN * denominator -
                                                u * (gradient[0].SlopeN + gradient[1].SlopeN))},
               .Denominator = denominator};
}
}

RoadPortField::RoadPortField(std::vector<Plane> planes, std::vector<Patch> patches)
    : Planes_(std::move(planes)), Patches_(std::move(patches)) {}

std::expected<RoadPortField, RoadPortField::Error>
RoadPortField::Make(std::span<const Plane> planes, std::span<const Patch> patches) {
  if (planes.empty() || patches.empty() || planes.size() > kPlanesMost ||
      patches.size() > kPatchesMost) {
    return std::unexpected(Error::InputBudget);
  }
  if (!std::ranges::all_of(planes, [](const Plane &plane) { return Valid(plane); })) {
    return std::unexpected(Error::InvalidPlane);
  }
  if (!std::ranges::all_of(
          patches, [count = planes.size()](const Patch &patch) { return Valid(patch, count); })) {
    return std::unexpected(Error::InvalidPatch);
  }
  return RoadPortField(std::vector<Plane>(planes.begin(), planes.end()),
                       std::vector<Patch>(patches.begin(), patches.end()));
}

std::expected<RoadPortField::Sample, RoadPortField::Error>
RoadPortField::Evaluate(size_t patch, EastNorth at) const noexcept {
  if (patch >= Patches_.size() || !Bounded(at)) { return std::unexpected(Error::InvalidQuery); }
  const Patch &part = Patches_[patch];
  const Plane &left = Planes_[part.Parents[0]];
  const double height = HeightAt(left, at);
  if (part.Parents[0] == part.Parents[1]) {
    return Sample{.HeightM = height, .Gradient = left.Gradient};
  }
  const auto blend = BlendAt(part, at);
  if (!blend) { return std::unexpected(blend.error()); }
  const Plane &right = Planes_[part.Parents[1]];
  const double delta = HeightAt(right, at) - height;
  return Sample{
      .HeightM = height + blend->Weight * delta,
      .Gradient = {.SlopeE = left.Gradient.SlopeE +
                             blend->Weight * (right.Gradient.SlopeE - left.Gradient.SlopeE) +
                             blend->Gradient.SlopeE * delta,
                   .SlopeN = left.Gradient.SlopeN +
                             blend->Weight * (right.Gradient.SlopeN - left.Gradient.SlopeN) +
                             blend->Gradient.SlopeN * delta}};
}

std::expected<RoadPortField::Basis, RoadPortField::Error>
RoadPortField::BasisAt(size_t patch, EastNorth at) const noexcept {
  if (patch >= Patches_.size() || !Bounded(at)) { return std::unexpected(Error::InvalidQuery); }
  const Patch &part = Patches_[patch];
  Blend blend{.Weight = 0.0, .Gradient = {}};
  if (part.Parents[0] != part.Parents[1]) {
    const auto sampled = BlendAt(part, at);
    if (!sampled) { return std::unexpected(sampled.error()); }
    blend = *sampled;
  }
  Basis basis;
  basis.Denominator = blend.Denominator;
  for (size_t end = 0; end < 2; ++end) {
    const double weight = end == 0 ? 1.0 - blend.Weight : blend.Weight;
    const double sign = end == 0 ? -1.0 : 1.0;
    const auto origin = Planes_[part.Parents[end]].Origin;
    const std::array local{1.0, at.EastM - origin.EastM, at.NorthM - origin.NorthM};
    for (size_t column = 0; column < 3; ++column) {
      basis.Height[3 * end + column] = weight * local[column];
      basis.Gradient[0][3 * end + column] =
          (column == 1 ? weight : 0.0) + sign * blend.Gradient.SlopeE * local[column];
      basis.Gradient[1][3 * end + column] =
          (column == 2 ? weight : 0.0) + sign * blend.Gradient.SlopeN * local[column];
    }
  }
  return basis;
}

}

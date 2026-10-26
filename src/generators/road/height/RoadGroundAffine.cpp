#include "RoadGroundAffine.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <numeric>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr size_t kColumns = 3;
constexpr size_t kRoundingRounds = 8;
constexpr size_t kSamplesMost = 4096;
constexpr size_t kControlsMost = 256;
constexpr double kFrameCoefficientMost = 1e12;

struct LeastSquares {
  std::array<std::array<double, kColumns>, kColumns> R{};
  std::array<double, kColumns> Target{};
  size_t Rotations = 0;

  void Add(std::array<double, kColumns> row, double value) noexcept {
    for (size_t pivot = 0; pivot < kColumns; ++pivot) {
      const double radius = std::hypot(R[pivot][pivot], row[pivot]);
      if (radius == 0) { continue; }
      const double cosine = R[pivot][pivot] / radius;
      const double sine = row[pivot] / radius;
      for (size_t column = pivot; column < kColumns; ++column) {
        const double old = R[pivot][column];
        R[pivot][column] = cosine * old + sine * row[column];
        row[column] = -sine * old + cosine * row[column];
      }
      const double old = Target[pivot];
      Target[pivot] = cosine * old + sine * value;
      value = -sine * old + cosine * value;
      ++Rotations;
    }
  }

  [[nodiscard]] std::expected<RoadPlane, RoadGroundError> Gradient(size_t rows) const noexcept {
    double norm = 0;
    for (const auto &row : R) {
      for (const auto value : row) { norm = std::hypot(norm, value); }
    }
    const double threshold = std::numeric_limits<double>::epsilon() *
                             static_cast<double>(std::max(rows, kColumns)) * norm;
    auto coefficients = Target;
    for (size_t at = kColumns; at-- > 0;) {
      if (!std::isfinite(R[at][at]) || std::abs(R[at][at]) <= threshold) {
        return std::unexpected(RoadGroundError::InvalidGeometry);
      }
      for (size_t column = at + 1; column < kColumns; ++column) {
        coefficients[at] -= R[at][column] * coefficients[column];
      }
      coefficients[at] /= R[at][at];
    }
    if (!std::isfinite(coefficients[1]) || !std::isfinite(coefficients[2])) {
      return std::unexpected(RoadGroundError::InvalidGeometry);
    }
    return RoadPlane{.SlopeE = coefficients[1], .SlopeN = coefficients[2]};
  }
};

double Median(std::vector<double> &values) {
  const auto middle = values.begin() + static_cast<std::ptrdiff_t>(values.size() / 2);
  std::nth_element(values.begin(), middle, values.end());
  return values.size() % 2 != 0 ? *middle
                                : std::midpoint(*std::max_element(values.begin(), middle), *middle);
}

double Norm(std::span<const RoadPortFace> faces, RoadPlane gradient, size_t &tests) {
  double maximum = 0;
  for (const auto &face : faces) {
    ++tests;
    const auto &f = face.GradeFrame;
    const double value = std::hypot(f[0] * gradient.SlopeE + f[1] * gradient.SlopeN,
                                    f[2] * gradient.SlopeE + f[3] * gradient.SlopeN);
    if (!std::isfinite(value)) { return std::numeric_limits<double>::infinity(); }
    maximum = std::max(maximum, value);
  }
  return maximum;
}

bool Valid(const RoadGroundField &input) {
  if (input.Priors.size() < kColumns || input.Faces.empty() || input.Patches.empty()) {
    return false;
  }
  for (const auto &prior : input.Priors) {
    if (prior.Patch >= input.Patches.size() || !std::isfinite(prior.At.EastM) ||
        !std::isfinite(prior.At.NorthM) || !std::isfinite(prior.HeightM)) {
      return false;
    }
  }
  for (const auto &face : input.Faces) {
    if (face.Patch >= input.Patches.size()) { return false; }
    for (const auto value : face.GradeFrame) {
      if (!std::isfinite(value) || std::abs(value) > kFrameCoefficientMost) { return false; }
    }
    const auto &f = face.GradeFrame;
    if (f[0] * f[3] - f[1] * f[2] == 0) { return false; }
  }
  return true;
}
}

std::expected<RoadGroundAffine, RoadGroundError> FitRoadGroundAffine(const RoadGroundField &input,
                                                                     RoadGroundBudget budget,
                                                                     const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
  if (input.Priors.size() > std::min(budget.TerrainSamples, kSamplesMost) ||
      input.Faces.size() > 2 * kSamplesMost ||
      input.Patches.size() / 2 >= std::min(budget.Controls, kControlsMost) ||
      input.Patches.size() > budget.ContainerBytes / (2 * sizeof(RoadPortField::Patch))) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  const size_t fixed = 2 * input.Patches.size() * sizeof(RoadPortField::Patch);
  if (sizeof(RoadPortField::Plane) > budget.ContainerBytes - fixed ||
      input.Priors.size() >
          (budget.ContainerBytes - fixed - sizeof(RoadPortField::Plane)) / sizeof(double)) {
    return std::unexpected(RoadGroundError::CapacityExceeded);
  }
  if (!Valid(input)) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  std::vector<double> targets;
  targets.reserve(input.Priors.size());
  EastNorth origin;
  for (const auto &prior : input.Priors) {
    origin.EastM += prior.At.EastM;
    origin.NorthM += prior.At.NorthM;
    targets.push_back(prior.HeightM);
  }
  origin.EastM /= static_cast<double>(targets.size());
  origin.NorthM /= static_cast<double>(targets.size());
  const double datum = Median(targets);
  LeastSquares fit;
  for (const auto &prior : input.Priors) {
    if (stop.stop_requested()) { return std::unexpected(RoadGroundError::Canceled); }
    fit.Add({1, prior.At.EastM - origin.EastM, prior.At.NorthM - origin.NorthM},
            prior.HeightM - datum);
  }
  auto gradient = fit.Gradient(targets.size());
  if (!gradient) { return std::unexpected(gradient.error()); }
  size_t tests = 0;
  double maximum = Norm(input.Faces, *gradient, tests);
  if (!std::isfinite(maximum)) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  if (maximum > 1) {
    gradient->SlopeE /= maximum;
    gradient->SlopeN /= maximum;
    maximum = Norm(input.Faces, *gradient, tests);
    for (size_t round = 0; maximum > 1 && round < kRoundingRounds; ++round) {
      gradient->SlopeE = std::nextafter(gradient->SlopeE, 0.);
      gradient->SlopeN = std::nextafter(gradient->SlopeN, 0.);
      maximum = Norm(input.Faces, *gradient, tests);
    }
    if (maximum > 1) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  }
  double low = std::numeric_limits<double>::infinity();
  double high = -low;
  for (const auto &prior : input.Priors) {
    const double residual = prior.HeightM - (prior.At.EastM - origin.EastM) * gradient->SlopeE -
                            (prior.At.NorthM - origin.NorthM) * gradient->SlopeN;
    low = std::min(low, residual);
    high = std::max(high, residual);
  }
  const RoadPortField::Plane plane{
      .Origin = origin, .HeightM = std::midpoint(low, high), .Gradient = *gradient};
  const std::vector<RoadPortField::Patch> patches(input.Patches.size());
  auto field = RoadPortField::Make(std::span(&plane, 1), patches);
  if (!field) { return std::unexpected(RoadGroundError::InvalidGeometry); }
  double error = 0;
  for (const auto &prior : input.Priors) {
    const auto sample = field->Evaluate(prior.Patch, prior.At);
    if (!sample) { return std::unexpected(RoadGroundError::InvalidGeometry); }
    error = std::max(error, std::abs(sample->HeightM - prior.HeightM));
  }
  return RoadGroundAffine{.Field = std::move(*field),
                          .MaximumPriorErrorM = error,
                          .MaximumNormalizedGrade = maximum,
                          .Observations = input.Priors.size(),
                          .Rotations = fit.Rotations,
                          .GradeFrameTests = tests};
}
}

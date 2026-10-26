#include "RoadPortField.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr size_t kDegree = 4;
constexpr size_t kControls = 15;
using Matrix = std::array<std::array<double, kControls>, kControls>;

constexpr auto PowersOf(double value) {
  std::array<double, kDegree + 1> result{};
  result[0] = 1.0;
  for (size_t n = 1; n <= kDegree; ++n) { result[n] = result[n - 1] * value; }
  return result;
}

constexpr auto Powers() {
  std::array<std::array<size_t, 3>, kControls> result{};
  size_t at = 0;
  for (size_t i = 0; i <= kDegree; ++i) {
    for (size_t j = 0; i + j <= kDegree; ++j) { result[at++] = {i, j, kDegree - i - j}; }
  }
  return result;
}

constexpr auto kPowers = Powers();

constexpr double Weight(size_t node, size_t corner) {
  return static_cast<double>(kPowers[node][corner]) / static_cast<double>(kDegree);
}

constexpr double Magnitude(double value) {
  return value < 0.0 ? -value : value;
}

consteval Matrix BernsteinMatrix() {
  constexpr std::array factorial{1.0, 1.0, 2.0, 6.0, 24.0};
  Matrix result{};
  for (size_t row = 0; row < kControls; ++row) {
    for (size_t column = 0; column < kControls; ++column) {
      double value = factorial[kDegree];
      for (size_t corner = 0; corner < 3; ++corner) {
        value *= PowersOf(Weight(row, corner))[kPowers[column][corner]] /
                 factorial[kPowers[column][corner]];
      }
      result[row][column] = value;
    }
  }
  return result;
}

consteval Matrix BernsteinInverse() {
  Matrix matrix = BernsteinMatrix();
  Matrix inverse{};
  for (size_t row = 0; row < kControls; ++row) { inverse[row][row] = 1.0; }
  for (size_t column = 0; column < kControls; ++column) {
    size_t pivot = column;
    for (size_t row = column + 1; row < kControls; ++row) {
      if (Magnitude(matrix[row][column]) > Magnitude(matrix[pivot][column])) { pivot = row; }
    }
    std::swap(matrix[column], matrix[pivot]);
    std::swap(inverse[column], inverse[pivot]);
    const double divisor = matrix[column][column];
    for (size_t entry = 0; entry < kControls; ++entry) {
      matrix[column][entry] /= divisor;
      inverse[column][entry] /= divisor;
    }
    for (size_t row = 0; row < kControls; ++row) {
      if (row == column) { continue; }
      const double factor = matrix[row][column];
      for (size_t entry = 0; entry < kControls; ++entry) {
        matrix[row][entry] -= factor * matrix[column][entry];
        inverse[row][entry] -= factor * inverse[column][entry];
      }
    }
  }
  return inverse;
}

constexpr auto kInverse = BernsteinInverse();

bool ValidFrame(const std::array<double, 4> &frame) {
  constexpr double most = 1.e12;
  return std::ranges::all_of(
             frame, [](double value) { return std::isfinite(value) && std::abs(value) <= most; }) &&
         frame[0] * frame[3] - frame[1] * frame[2] != 0.0;
}

std::array<double, 2> Denominators(const std::array<double, 3> &corners, size_t control) {
  double fourth = 1.0;
  double derivative = 0.0;
  for (size_t corner = 0; corner < 3; ++corner) {
    fourth *= PowersOf(corners[corner])[kPowers[control][corner]];
    derivative += static_cast<double>(kPowers[control][corner]) / corners[corner];
  }
  return {fourth, fourth * derivative / static_cast<double>(kDegree)};
}

template <class Controls>
Controls BernsteinControls(const Controls &weighted, const std::array<double, 3> &corners) {
  Controls result{};
  for (size_t control = 0; control < kControls; ++control) {
    const auto denominator = Denominators(corners, control);
    for (size_t column = 0; column < 6; ++column) {
      for (size_t node = 0; node < kControls; ++node) {
        result[control].Error[column] +=
            kInverse[control][node] * weighted[node].Error[column] / denominator[1];
        for (size_t axis = 0; axis < 2; ++axis) {
          result[control].Gradient[axis][column] +=
              kInverse[control][node] * weighted[node].Gradient[axis][column] / denominator[0];
        }
      }
    }
  }
  return result;
}
}

std::expected<std::array<RoadPortField::Control, 15>, RoadPortField::Error>
RoadPortField::Controls(size_t patch, const std::array<EastNorth, 3> &triangle) const noexcept {
  std::array<Basis, 3> corners;
  std::array<double, 3> cornerDenominators{};
  for (size_t corner = 0; corner < 3; ++corner) {
    const auto basis = BasisAt(patch, triangle[corner]);
    if (!basis) { return std::unexpected(basis.error()); }
    corners[corner] = *basis;
    cornerDenominators[corner] = basis->Denominator;
  }
  std::array<Control, kControls> weighted{};
  for (size_t node = 0; node < kControls; ++node) {
    EastNorth at{};
    for (size_t corner = 0; corner < 3; ++corner) {
      at.EastM += Weight(node, corner) * triangle[corner].EastM;
      at.NorthM += Weight(node, corner) * triangle[corner].NorthM;
    }
    const auto basis = BasisAt(patch, at);
    if (!basis) { return std::unexpected(basis.error()); }
    const double third = PowersOf(basis->Denominator)[3];
    const double fourth = third * basis->Denominator;
    for (size_t column = 0; column < 6; ++column) {
      double linear = 0.0;
      for (size_t corner = 0; corner < 3; ++corner) {
        linear += Weight(node, corner) * corners[corner].Height[column];
      }
      weighted[node].Error[column] = (basis->Height[column] - linear) * third;
      for (size_t axis = 0; axis < 2; ++axis) {
        weighted[node].Gradient[axis][column] = basis->Gradient[axis][column] * fourth;
      }
    }
  }
  return BernsteinControls(weighted, cornerDenominators);
}

std::expected<RoadPortField::FaceBounds, RoadPortField::Error>
RoadPortField::Bounds(size_t patch,
                      const std::array<EastNorth, 3> &triangle,
                      const std::array<double, 4> &gradeFrame) const noexcept {
  if (!ValidFrame(gradeFrame)) { return std::unexpected(Error::InvalidGradeFrame); }
  const auto controls = Controls(patch, triangle);
  if (!controls) { return std::unexpected(controls.error()); }
  const auto &parents = Patches_[patch].Parents;
  const Plane &left = Planes_[parents[0]];
  const Plane &right = Planes_[parents[1]];
  const std::array coefficients{left.HeightM, right.HeightM};
  const std::array values{coefficients[0],
                          left.Gradient.SlopeE,
                          left.Gradient.SlopeN,
                          coefficients[1],
                          right.Gradient.SlopeE,
                          right.Gradient.SlopeN};
  FaceBounds result;
  double absoluteError = 1.0;
  for (const auto &control : *controls) {
    std::array<double, 2> gradient{};
    double error = 0.0;
    double absolute = 0.0;
    for (size_t column = 0; column < 6; ++column) {
      error += control.Error[column] * values[column];
      absolute += std::abs(control.Error[column]) * std::abs(values[column]);
      for (size_t axis = 0; axis < 2; ++axis) {
        gradient[axis] += control.Gradient[axis][column] * values[column];
      }
    }
    const double normalized = std::hypot(gradeFrame[0] * gradient[0] + gradeFrame[1] * gradient[1],
                                         gradeFrame[2] * gradient[0] + gradeFrame[3] * gradient[1]);
    if (!std::isfinite(normalized) || !std::isfinite(error) || !std::isfinite(absolute)) {
      return std::unexpected(Error::NumericRange);
    }
    result.NormalizedGradientMaximum = std::max(result.NormalizedGradientMaximum, normalized);
    result.HeightChordErrorM = std::max(result.HeightChordErrorM, std::abs(error));
    absoluteError = std::max(absoluteError, absolute);
  }
  constexpr double guard = 256.0 * std::numeric_limits<double>::epsilon();
  result.HeightChordErrorM += guard * absoluteError;
  return result;
}

}

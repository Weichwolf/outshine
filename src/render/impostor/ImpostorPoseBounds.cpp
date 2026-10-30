#include "ImpostorPoseBounds.h"
#include "ImpostorViewFrame.h"
#include "Units.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <string>

namespace outshine::Render {
namespace {
using namespace DistanceArithmetic;
using Point = std::array<Interval, 3>;
using Clip = std::array<Interval, 4>;
constexpr double kBasisTolerance = 1e-9;
constexpr double kLargestRaster = 1u << 24u;

Interval Exact(double value) {
  return {.Lower = value, .Upper = value};
}

bool Finite(Interval value) {
  return std::isfinite(value.Lower) && std::isfinite(value.Upper) && value.Lower <= value.Upper;
}

Interval Divide(Interval a, Interval b) {
  if (b.Lower <= 0 && b.Upper >= 0) {
    return {.Lower = -std::numeric_limits<double>::infinity(),
            .Upper = std::numeric_limits<double>::infinity()};
  }
  return Multiply(a, Enclose(1 / b.Upper, 1 / b.Lower));
}

Point Exact(const Vec3 &value) {
  return {Exact(value[0]), Exact(value[1]), Exact(value[2])};
}

Interval Dot(const Point &a, const Point &b) {
  Interval sum = Exact(0);
  for (size_t i = 0; i < 3; ++i) { sum = Add(sum, Multiply(a[i], b[i])); }
  return sum;
}

Point Cross(const Point &a, const Point &b) {
  Point result;
  for (size_t i = 0; i < 3; ++i) {
    const size_t j = (i + 1) % 3;
    const size_t k = (i + 2) % 3;
    result[i] = Subtract(Multiply(a[j], b[k]), Multiply(a[k], b[j]));
  }
  return result;
}

Point Relative(const Point &point, const Vec3 &origin) {
  Point result;
  for (size_t i = 0; i < 3; ++i) { result[i] = Subtract(point[i], Exact(origin[i])); }
  return result;
}

bool Valid(const CameraBasis &camera, const Lens &lens) {
  const std::array basis{camera.Right, camera.Up, camera.Forward};
  for (size_t i = 0; i < 3; ++i) {
    if (!std::isfinite(camera.EyeM[i])) { return false; }
    for (size_t j = 0; j < 3; ++j) {
      const double value = outshine::Dot(basis[i], basis[j]);
      if (!std::isfinite(value) || std::abs(value - (i == j ? 1.0 : 0.0)) > kBasisTolerance) {
        return false;
      }
    }
  }
  for (const double pixels : {lens.WidePx, lens.HighPx}) {
    if (!std::isfinite(pixels) || pixels < 1 || pixels > kLargestRaster ||
        std::floor(pixels) != pixels) {
      return false;
    }
  }
  const bool orthographic = lens.OrthoM > 0;
  return std::isfinite(lens.NearM) && lens.NearM >= 0 && std::isfinite(lens.FarM) &&
         (lens.FarM == 0 || lens.FarM > lens.NearM) &&
         (orthographic ? std::isfinite(lens.OrthoM) && std::isfinite(lens.OrthoWidthM) &&
                             lens.OrthoWidthM > 0 && lens.FarM > lens.NearM
                       : lens.OrthoM == 0 && std::isfinite(lens.FovDeg) && lens.FovDeg > 0 &&
                             lens.FovDeg < kDegPerHalfTurn && lens.NearM > 0);
}

Clip ProjectPoint(const Point &point,
                  const CameraBasis &camera,
                  const Mat4f &matrix,
                  bool direction) {
  const Clip local{Dot(point, Exact(camera.Right)),
                   Dot(point, Exact(camera.Up)),
                   Multiply(Dot(point, Exact(camera.Forward)), Exact(-1)),
                   Exact(direction ? 0 : 1)};
  Clip result;
  for (size_t row = 0; row < 4; ++row) {
    result[row] = Exact(0);
    for (size_t column = 0; column < 4; ++column) {
      result[row] = Add(result[row], Multiply(Exact(matrix.At(row, column)), local[column]));
    }
  }
  return result;
}

double Magnitude(Interval value) {
  return std::max(std::abs(value.Lower), std::abs(value.Upper));
}

struct ProjectionExtrema {
  double Height = 0;
  std::array<double, 2> Numerator{};
  std::array<double, 2> MinimumW{std::numeric_limits<double>::infinity(),
                                 std::numeric_limits<double>::infinity()};
  bool DepthInside = true;

  bool Include(Interval height, const Clip &original, const Clip &direction) {
    Clip support;
    for (size_t i = 0; i < 4; ++i) {
      support[i] = Subtract(original[i], Multiply(height, direction[i]));
      if (!Finite(original[i]) || !Finite(support[i])) { return false; }
    }
    Height = std::max(Height, Magnitude(height));
    MinimumW[0] = std::min(MinimumW[0], original[3].Lower);
    MinimumW[1] = std::min(MinimumW[1], support[3].Lower);
    for (size_t i = 0; i < 2; ++i) {
      const auto numerator =
          Subtract(Multiply(direction[i], original[3]), Multiply(direction[3], original[i]));
      if (!Finite(numerator)) { return false; }
      Numerator[i] = std::max(Numerator[i], Magnitude(numerator));
    }
    for (const auto &clip : {original, support}) {
      DepthInside &= clip[2].Lower >= 0 && Subtract(clip[3], clip[2]).Lower >= 0;
    }
    return true;
  }
};
}

std::expected<ImpostorPoseBounds, std::string>
ImpostorPoseBounds::From(Vec3 centre, Vec3 towardEye, double halfExtentM) {
  const auto frame = ImpostorViewFrame::From(towardEye);
  if (!frame || !std::isfinite(halfExtentM) || !(halfExtentM > 0) ||
      !std::ranges::all_of(centre, [](double value) { return std::isfinite(value); })) {
    return std::unexpected("invalid complete capture domain");
  }
  const std::array rows{Exact(frame->Right), Exact(frame->Up), Exact(towardEye)};
  const std::array columns{
      Cross(rows[1], rows[2]), Cross(rows[2], rows[0]), Cross(rows[0], rows[1])};
  const auto determinant = Dot(rows[0], columns[0]);
  if (!Finite(determinant) || (determinant.Lower <= 0 && determinant.Upper >= 0)) {
    return std::unexpected("unresolved capture domain inverse");
  }
  const std::array extents{
      Exact(halfExtentM), Exact(halfExtentM), Multiply(Exact(2), Exact(halfExtentM))};
  std::array<Point, 3> offsets;
  for (size_t column = 0; column < 3; ++column) {
    for (size_t axis = 0; axis < 3; ++axis) {
      offsets[column][axis] = Multiply(Divide(columns[column][axis], determinant), extents[column]);
    }
  }
  ImpostorPoseBounds result;
  result.Centre_ = centre;
  result.Direction_ = towardEye;
  for (size_t corner = 0; corner < result.Corners_.size(); ++corner) {
    for (size_t axis = 0; axis < 3; ++axis) {
      auto value = Exact(centre[axis]);
      for (size_t column = 0; column < 3; ++column) {
        value = Add(
            value, Multiply(offsets[column][axis], Exact((corner & (1u << column)) == 0 ? -1 : 1)));
      }
      if (!Finite(value)) { return std::unexpected("capture domain overflow"); }
      result.Corners_[corner][axis] = value;
    }
  }
  return result;
}

std::expected<ImpostorPoseBounds::Error, std::string>
ImpostorPoseBounds::Project(const CameraBasis &camera, const Lens &lens) const {
  if (!Valid(camera, lens)) { return std::unexpected("invalid geometric pose operands"); }
  const auto matrix = lens.Projection();
  if (!std::ranges::all_of(matrix, [](float value) { return std::isfinite(value); })) {
    return std::unexpected("nonfinite geometric projection");
  }
  const auto direction = Exact(Direction_);
  const auto denominator = Dot(direction, direction);
  const auto facing = Dot(Relative(Exact(camera.EyeM), Centre_), direction);
  if (!(denominator.Lower > 0) || !(facing.Lower > 0)) {
    return std::unexpected("geometric support faces away from the viewer");
  }
  const auto projectedDirection = ProjectPoint(direction, camera, matrix, true);
  ProjectionExtrema extrema;
  for (const auto &corner : Corners_) {
    const auto height = Divide(Dot(Relative(corner, Centre_), direction), denominator);
    if (!Finite(height) ||
        !extrema.Include(height,
                         ProjectPoint(Relative(corner, camera.EyeM), camera, matrix, false),
                         projectedDirection)) {
      return std::unexpected("geometric projection overflow");
    }
  }
  if (!(extrema.MinimumW[0] > 0) || !(extrema.MinimumW[1] > 0)) {
    return std::unexpected("source or support crosses the projective eye");
  }
  const auto divisor = Multiply(Exact(extrema.MinimumW[0]), Exact(extrema.MinimumW[1]));
  Error result{.AxisUpperPx = {}, .L1UpperPx = 0, .DepthInside = extrema.DepthInside};
  const std::array raster{lens.WidePx, lens.HighPx};
  for (size_t axis = 0; axis < 2; ++axis) {
    const auto numerator = Multiply(Multiply(Exact(extrema.Height), Exact(extrema.Numerator[axis])),
                                    Exact(raster[axis] * .5));
    const auto error = Divide(numerator, divisor);
    if (!Finite(error)) { return std::unexpected("geometric error overflow"); }
    result.AxisUpperPx[axis] = error.Upper;
  }
  result.L1UpperPx = Add(Exact(result.AxisUpperPx[0]), Exact(result.AxisUpperPx[1])).Upper;
  if (!std::isfinite(result.L1UpperPx)) { return std::unexpected("geometric error overflow"); }
  return result;
}
}

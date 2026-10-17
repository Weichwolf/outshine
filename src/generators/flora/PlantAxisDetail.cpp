#include "PlantAxisDetail.h"
#include "PlantAxisSurface.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <numbers>
#include <optional>
#include <string_view>

namespace outshine::Generators {
namespace {
using Limits = PlantAxisSurfaceLimits;

struct Coefficients {
  double Axial = 0, Mixed = 0, Angular = 0;
};

double Up(double value) {
  return std::nextafter(value, std::numeric_limits<double>::infinity());
}

double Add(double first, double second) {
  if (first == 0) { return second; }
  if (second == 0) { return first; }
  return Up(first + second);
}

double Multiply(double first, double second) {
  return first == 0 || second == 0 ? 0 : Up(first * second);
}

double Divide(double numerator, double denominator) {
  return numerator == 0 ? 0 : Up(numerator / denominator);
}

std::optional<Coefficients> Model(const PlantAxis &axis) {
  const std::array values{axis.LengthM, axis.Bend, axis.RadiiM[0], axis.RadiiM[1]};
  const double radius = std::max(axis.RadiiM[0], axis.RadiiM[1]);
  const double bend = std::abs(axis.Bend);
  if (!std::numeric_limits<double>::is_iec559 ||
      !std::ranges::all_of(values, [](double value) { return std::isfinite(value); }) ||
      axis.LengthM <= 0 || axis.RadiiM[0] <= 0 || axis.RadiiM[1] <= 0 ||
      axis.LengthM + radius > std::numeric_limits<float>::max() || bend > std::numbers::pi ||
      Multiply(bend, radius) >= axis.LengthM) {
    return std::nullopt;
  }
  const double slope =
      axis.RadiiM[0] == axis.RadiiM[1] ? 0 : Up(std::abs(axis.RadiiM[1] - axis.RadiiM[0]));
  const double pi = Up(std::numbers::pi);
  return Coefficients{
      .Axial = Divide(Add(Add(Multiply(axis.LengthM, bend), Multiply(Multiply(2, slope), bend)),
                          Multiply(radius, Multiply(bend, bend))),
                      8),
      .Mixed = Divide(Multiply(Add(slope, Multiply(radius, bend)), pi), 2),
      .Angular = Divide(Multiply(radius, Multiply(pi, pi)), 2)};
}

double Bound(Coefficients model, size_t segments, size_t sides) {
  return Add(Add(Divide(model.Axial, static_cast<double>(segments * segments)),
                 Divide(model.Mixed, static_cast<double>(segments * sides))),
             Divide(model.Angular, static_cast<double>(sides * sides)));
}

std::optional<size_t> Segments(Coefficients model, size_t sides, double allowed) {
  if (Bound(model, Limits::LeastSegments, sides) <= allowed) { return Limits::LeastSegments; }
  if (Bound(model, Limits::MostSegments, sides) > allowed) { return std::nullopt; }
  size_t first = Limits::LeastSegments + 1;
  size_t last = Limits::MostSegments;
  while (first < last) {
    const size_t middle = first + (last - first) / 2;
    if (Bound(model, middle, sides) <= allowed) {
      last = middle;
    } else {
      first = middle + 1;
    }
  }
  return first;
}

bool ValidDetail(size_t segments, size_t sides) {
  return segments >= Limits::LeastSegments && segments <= Limits::MostSegments &&
         sides >= Limits::LeastSides && sides <= Limits::MostSides;
}
}

std::expected<double, std::string_view>
BoundPlantAxisParametricError(const PlantAxis &axis, size_t segments, size_t sides) {
  const auto model = Model(axis);
  if (!model || !ValidDetail(segments, sides)) {
    return std::unexpected("axis error model requires regular dimensions and bounded detail");
  }
  return Bound(*model, segments, sides);
}

std::expected<PlantAxisDetail, std::string_view>
SelectPlantAxisDetail(const PlantAxis &axis, double maximumParametricErrorM) {
  const auto model = Model(axis);
  if (!model || !std::isfinite(maximumParametricErrorM) || maximumParametricErrorM <= 0) {
    return std::unexpected("axis detail requires a positive finite parametric error budget");
  }
  std::optional<PlantAxisDetail> best;
  size_t triangles = std::numeric_limits<size_t>::max();
  for (size_t sides = Limits::LeastSides; sides <= Limits::MostSides; ++sides) {
    if (2 * sides * (Limits::LeastSegments + 1) >= triangles) { break; }
    const auto segments = Segments(*model, sides, maximumParametricErrorM);
    if (!segments) { continue; }
    const size_t count = 2 * sides * (*segments + 1);
    if (count < triangles) {
      best = {.Segments = *segments,
              .Sides = sides,
              .ParametricErrorM = Bound(*model, *segments, sides)};
      triangles = count;
    }
  }
  if (!best) { return std::unexpected("axis error budget requires unavailable detail"); }
  return *best;
}
}

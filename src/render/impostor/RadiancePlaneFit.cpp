#include "RadianceSupport.h"
#include "ImpostorAtlas.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <expected>
#include <limits>
#include <string>

namespace outshine::Render {
namespace {
using Factor = std::array<std::array<double, 4>, 3>;

double AddRow(Factor &factor, std::array<double, 4> row) {
  for (size_t column = 0; column < 3; ++column) {
    auto &held = factor[column];
    const double radius = std::hypot(held[column], row[column]);
    if (radius == 0) { continue; }
    const double cosine = held[column] / radius;
    const double sine = row[column] / radius;
    held[column] = radius;
    for (size_t at = column + 1; at < 4; ++at) {
      const double next = cosine * held[at] + sine * row[at];
      row[at] = cosine * row[at] - sine * held[at];
      held[at] = next;
    }
  }
  return row[3] * row[3];
}
}

std::expected<RadiancePlaneFit, std::string> FitRadiancePlane(const ImpostorAtlas &source,
                                                              RadiancePlaneFitRequest request) {
  if (request.View >= source.Views().size()) {
    return std::unexpected("radiance fit view is unavailable");
  }
  const auto &texels = source.Views()[request.View].Texels;
  if (texels.size() > request.TexelsMost) {
    return std::unexpected("radiance plane fit exceeds source texel capacity");
  }
  Factor factor{};
  RadiancePlaneFit result;
  double residual = 0;
  const auto width = static_cast<size_t>(source.Pixels());
  const double side = source.Pixels();
  for (size_t at = 0; at < texels.size(); ++at) {
    const auto &sample = texels[at];
    if (sample.Surface == 0) { continue; }
    const double u = (static_cast<double>(at % width) + .5) / side * 2 - 1;
    const size_t y = at / width;
    const double v = 1 - (static_cast<double>(y) + .5) / side * 2;
    residual += AddRow(factor, {u, v, 1, 4.0 * sample.Depth - 2});
    ++result.SourcePoints;
  }
  if (result.SourcePoints < 3) {
    return std::unexpected("radiance fit needs three occupied source points");
  }
  const double largest = std::max({factor[0][0], factor[1][1], factor[2][2]});
  const double tolerance =
      std::numeric_limits<double>::epsilon() * static_cast<double>(result.SourcePoints) * largest;
  for (size_t row = 3; row-- > 0;) {
    if (!(factor[row][row] > tolerance)) {
      return std::unexpected("occupied source points do not identify a radiance plane");
    }
    double value = factor[row][3];
    for (size_t column = row + 1; column < 3; ++column) {
      value -= factor[row][column] * result.DepthCoefficients[column];
    }
    result.DepthCoefficients[row] = value / factor[row][row];
  }
  result.NormalizedDepthRms = std::sqrt(residual / static_cast<double>(result.SourcePoints));
  return result;
}
}

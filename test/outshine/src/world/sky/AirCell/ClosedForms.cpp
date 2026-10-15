#include "AirCell.h"
#include "Check.h"
#include <algorithm>
#include <array>
#include <cmath>

namespace {
using namespace outshine::Test;
using outshine::air_cell::airIntegrateCell;

void Homogeneous(outshine::AirCellInput cell) {
  const auto sample = airIntegrateCell(cell);
  const float extinction = cell.ExtinctionA;
  const float source = cell.SourceA;
  const float distance = cell.DistanceKm;
  const double depth = static_cast<double>(extinction) * distance;
  const double expected = extinction > 0.0f ? -std::expm1(-depth) / extinction * source
                                            : static_cast<double>(distance) * source;
  CHECK(std::isfinite(sample.RadiancePerLux) && std::isfinite(sample.Transmittance),
        "thin, opaque and zero-extinction cells remain finite");
  CHECK_NEAR(sample.RadiancePerLux,
             expected,
             std::max(1e-12, std::fabs(expected) * 3e-6),
             "radiance/lux",
             "constant coefficients retain the closed-form transport including zero extinction");
  CHECK_NEAR(sample.Transmittance,
             std::exp(-depth),
             2e-7,
             "transmittance",
             "cell opacity agrees with Beer-Lambert transport");
}

void ConstantSource(float width, float distance) {
  constexpr std::array<float, 3> extinction = {0.01f, 0.2f, 2.0f};
  constexpr float source = 0.3f;
  const auto sample = airIntegrateCell({.WidthKm = width,
                                        .DistanceKm = distance,
                                        .ExtinctionA = extinction[0],
                                        .ExtinctionB = extinction[1],
                                        .ExtinctionC = extinction[2],
                                        .SourceA = source * extinction[0],
                                        .SourceB = source * extinction[1],
                                        .SourceC = source * extinction[2]});
  const double unit = static_cast<double>(distance) / width;
  const double depth = width * (extinction[0] * (unit - unit * unit + unit * unit * unit / 3.0) +
                                extinction[1] * (unit * unit - 2.0 * unit * unit * unit / 3.0) +
                                extinction[2] * unit * unit * unit / 3.0);
  CHECK_NEAR(sample.Transmittance,
             std::exp(-depth),
             2e-7,
             "transmittance",
             "positive quadratic extinction keeps its analytic column");
  CHECK_NEAR(sample.RadiancePerLux,
             -std::expm1(-depth) * source,
             2e-6,
             "radiance/lux",
             "source-function identity is independent of optical thickness");
}
}

int main() {
  for (const float width : {0.001f, 1.0f, 10.0f}) {
    for (const float fraction : {0.0f, 0.0001f, 0.01f, 0.25f, 0.75f, 1.0f}) {
      for (const float extinction : {0.0f, 1e-12f, 1e-8f, 0.001f, 0.1f, 1.0f, 100.0f}) {
        Homogeneous({.WidthKm = width,
                     .DistanceKm = width * fraction,
                     .ExtinctionA = extinction,
                     .ExtinctionB = extinction,
                     .ExtinctionC = extinction,
                     .SourceA = 0.03f,
                     .SourceB = 0.03f,
                     .SourceC = 0.03f});
      }
      ConstantSource(width, width * fraction);
    }
  }
  return Report();
}

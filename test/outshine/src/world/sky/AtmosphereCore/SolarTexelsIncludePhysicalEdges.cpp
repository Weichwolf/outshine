#include "Atmosphere.h"
#include "Check.h"
#include <array>
#include <cmath>

namespace {
constexpr double kRadiusErrorKm = 0.001;
constexpr double kCosineError = 5e-5;
constexpr double kUvError = 1e-4;
constexpr double kZenithError = 5e-6;

void CheckTexel(outshine::MediumLutSize size, float x, float y) {
  using namespace outshine;
  using namespace outshine::medium_core;
  using namespace outshine::Test;
  const MediumUv sub = {.U = (x + 0.5f) / size.WidthPx, .V = (y + 0.5f) / size.HeightPx};
  const auto look = mediumTransmittanceTexel(kEarthAir, sub, size);
  const double u = size.WidthPx > 1 ? x / (size.WidthPx - 1) : 0;
  const double v = size.HeightPx > 1 ? y / (size.HeightPx - 1) : 0;
  const double bottom = kEarthAir.BottomRadiusKm;
  const double top = kEarthAir.TopRadiusKm;
  const double span = std::sqrt((top - bottom) * (top + bottom));
  const double rho = span * v;
  const double radius = std::sqrt(rho * rho + bottom * bottom);
  const double reach = top - radius + u * (rho + span - top + radius);
  const double cosine =
      reach == 0 ? 1 : (span * span - rho * rho - reach * reach) / (2 * radius * reach);
  CHECK_NEAR(
      look.RadiusKm, radius, kRadiusErrorKm, "km", "the solar row keeps its physical radius");
  CHECK_NEAR(
      look.CosZenith, cosine, kCosineError, "cosine", "the solar column keeps its physical ray");
  const auto sampled = mediumTransmittanceSample(kEarthAir, look, size);
  CHECK_NEAR(sampled.U, sub.U, kUvError, "UV", "a solar texel is sampled at its own column centre");
  CHECK_NEAR(sampled.V, sub.V, kUvError, "UV", "a solar texel is sampled at its own row centre");
  if (y == 0) { CHECK(look.RadiusKm == kEarthAir.BottomRadiusKm, "the first row includes ground"); }
  if (y == size.HeightPx - 1 && size.HeightPx > 1) {
    CHECK(look.RadiusKm == kEarthAir.TopRadiusKm, "the last row includes atmospheric top");
  }
  if (x == 0) {
    CHECK_NEAR(look.CosZenith, 1, kZenithError, "cosine", "the first column includes zenith");
  }
}
}

int main() {
  constexpr std::array<outshine::MediumLutSize, 4> sizes = {{{.WidthPx = 512, .HeightPx = 64},
                                                             {.WidthPx = 256, .HeightPx = 64},
                                                             {.WidthPx = 2, .HeightPx = 2},
                                                             {.WidthPx = 1, .HeightPx = 1}}};
  for (const auto size : sizes) {
    for (const float x : {0.0f, (size.WidthPx - 1) * 0.5f, size.WidthPx - 1}) {
      for (const float y : {0.0f, (size.HeightPx - 1) * 0.5f, size.HeightPx - 1}) {
        CheckTexel(size, x, y);
      }
    }
  }
  return outshine::Test::Report();
}

#include "Check.h"
#include "Earth.h"
#include "GeodeticHeight.h"
#include "TangentFrame.h"
#include "Vec2.h"
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

namespace {
using namespace outshine;
using namespace outshine::Test;

void ConstantSurface() {
  const auto frame = TangentFrame::At({});
  for (const double altitude : {0.0, 100.0, 9000.0}) {
    const auto source = [altitude](LongitudeLatitude) noexcept { return std::optional{altitude}; };
    for (const double east : {0.0, 1000.0, 10000.0, 240000.0}) {
      const double radius = kWgs84A + altitude;
      const double expected =
          altitude - east * east / (radius + std::sqrt(radius * radius - east * east));
      const auto height = HeightInFrame(frame, {{east, 0}}, 1, source);
      CHECK(height && std::abs(*height - expected) < 1e-7,
            "constant geodetic height includes the analytic Earth curvature");
    }
  }
}

void ActualForwardPoints() {
  const std::array<LongitudeLatitude, 5> anchors{{{.LongitudeDeg = 10, .LatitudeDeg = 47},
                                                  {.LongitudeDeg = 179.9, .LatitudeDeg = 30},
                                                  {.LongitudeDeg = -179.9, .LatitudeDeg = -60},
                                                  {.LongitudeDeg = 20, .LatitudeDeg = 89},
                                                  {.LongitudeDeg = 0, .LatitudeDeg = -89}}};
  for (const auto anchor : anchors) {
    const auto frame = TangentFrame::At(anchor);
    const auto source = [](LongitudeLatitude geo) noexcept {
      return std::optional{1200 + 300 * std::sin(geo.LongitudeDeg * kDeg2Rad * 20) *
                                      std::cos(geo.LatitudeDeg * kDeg2Rad * 4)};
    };
    for (const double offset : {-0.4, 0.0, 0.4}) {
      const LongitudeLatitude geo{.LongitudeDeg = anchor.LongitudeDeg + offset,
                                  .LatitudeDeg = anchor.LatitudeDeg + offset};
      const auto sampled = source(geo);
      CHECK(sampled.has_value(), "analytic source provides its height");
      if (!sampled) { continue; }
      const auto point = frame.ToLocalPosition(
          {.LongitudeDeg = geo.LongitudeDeg, .LatitudeDeg = geo.LatitudeDeg, .HeightM = *sampled});
      const auto height = HeightInFrame(frame, {{point.EastM, point.NorthM}}, 1, source);
      CHECK(height && std::abs(*height - point.UpM) < 1e-6,
            "independent forward ECEF points retain height near poles and across the dateline");
    }
  }
}

void UnknownAndUnrepresentable() {
  const auto frame = TangentFrame::At({});
  const auto absent = [](LongitudeLatitude) noexcept -> std::optional<double> {
    return std::nullopt;
  };
  const auto zero = [](LongitudeLatitude) noexcept { return std::optional{0.0}; };
  CHECK(!HeightInFrame(frame, {{1000, 0}}, 1, absent), "missing source never becomes flat terrain");
  CHECK(!HeightInFrame(frame, {{2 * kWgs84A, 0}}, 1, zero),
        "a tangent-plane point beyond the ellipsoid stays unknown");
  CHECK(!HeightInFrame(frame, {{std::numeric_limits<double>::infinity(), 0}}, 1, zero),
        "invalid positions are refused");
  CHECK(!HeightInFrame(frame, {{0, 0}}, 0, zero), "sampling requires a declared field resolution");
  const auto discontinuous = [](LongitudeLatitude geo) noexcept {
    constexpr double boundaryDeg = 0.8978;
    return std::optional{geo.LongitudeDeg > boundaryDeg ? 10000.0 : 0.0};
  };
  CHECK(!HeightInFrame(frame, {{100000, 0}}, 1, discontinuous),
        "an unresolved discontinuity cannot publish a guessed surface");
}
}

int main() {
  ConstantSurface();
  ActualForwardPoints();
  UnknownAndUnrepresentable();
  return Report();
}

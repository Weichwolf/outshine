#include "Structures.h"
#include "Check.h"
#include "Geodesy.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <vector>

namespace {
class Ground final : public outshine::Generators::HeightSampler {
public:
  std::array<std::optional<double>, 4> Heights{350.0, 350.0, 350.0, 350.0};
  mutable std::vector<outshine::LongitudeLatitudeHeight> Queries;

  std::optional<double>
  sampleHeightAslM(const outshine::LongitudeLatitudeHeight &at) const override {
    const size_t corner = Queries.size();
    Queries.push_back(at);
    return corner < Heights.size() ? Heights[corner] : std::nullopt;
  }
};

std::array<double, 2> Heights(const outshine::Geometry &geometry) {
  std::array<double, 2> range{std::numeric_limits<double>::infinity(),
                              -std::numeric_limits<double>::infinity()};
  for (int part = 0; part < geometry.parts(); ++part) {
    const auto positions = geometry.positionsOf(part);
    for (size_t at = 1; at < positions.size(); at += 3) {
      range[0] = std::min(range[0], static_cast<double>(positions[at]));
      range[1] = std::max(range[1], static_cast<double>(positions[at]));
    }
  }
  return range;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Generators::Structures producer;
  Generators::Request request;
  request.LatitudeDeg = 48.21;
  request.LongitudeDeg = 16.37;
  const double halfLat = 6.0 / kMPerDegLat;
  const double halfLon = 6.0 / (kMPerDegLon * std::cos(request.LatitudeDeg * kDeg2Rad));
  const std::array<LongitudeLatitudeHeight, 4> corners{
      {{.LongitudeDeg = request.LongitudeDeg - halfLon,
        .LatitudeDeg = request.LatitudeDeg - halfLat},
       {.LongitudeDeg = request.LongitudeDeg + halfLon,
        .LatitudeDeg = request.LatitudeDeg - halfLat},
       {.LongitudeDeg = request.LongitudeDeg + halfLon,
        .LatitudeDeg = request.LatitudeDeg + halfLat},
       {.LongitudeDeg = request.LongitudeDeg - halfLon,
        .LatitudeDeg = request.LatitudeDeg + halfLat}}};
  for (const auto detail :
       {LevelOfDetail::Fine, LevelOfDetail::Shell, LevelOfDetail::Massed, LevelOfDetail::Skyline}) {
    request.Coarseness = detail;
    request.Ground = nullptr;
    const auto flat = producer.make(request);
    Ground ground;
    request.Ground = &ground;
    const auto raised = producer.make(request);
    CHECK(flat && raised, "flat and terrain-anchored buildings are produced at every detail");
    CHECK(ground.Queries.size() == 4, "the provider supplies exactly the footprint corners");
    for (size_t at = 0; at < std::min(ground.Queries.size(), corners.size()); ++at) {
      CHECK(std::abs(ground.Queries[at].LatitudeDeg - corners[at].LatitudeDeg) < 1e-12 &&
                std::abs(ground.Queries[at].LongitudeDeg - corners[at].LongitudeDeg) < 1e-12,
            "sampling follows the actual requested footprint");
    }
    if (!flat || !raised) { continue; }
    const auto flatHeights = Heights(*flat);
    const auto raisedHeights = Heights(*raised);
    CHECK(std::abs(raisedHeights[0] - flatHeights[0] - 350.0) < 0.005 &&
              std::abs(raisedHeights[1] - flatHeights[1] - 350.0) < 0.005,
          "known constant terrain moves both foundation and roof by its ASL elevation");
    ground.Queries.clear();
    ground.Heights = {100.0, 102.0, 108.0, 106.0};
    const auto slope = producer.make(request);
    CHECK(slope.has_value(), "a sloping footprint uses the native foundation solver");
    if (slope) {
      const auto slopeHeights = Heights(*slope);
      CHECK(slopeHeights[0] < 100.0 && slopeHeights[1] >= flatHeights[1] + 108.0 - 0.005,
            "the foundation intersects the low ground and the body clears the high corner");
    }
  }
  for (const auto invalid : {std::optional<double>{},
                             std::optional<double>{std::numeric_limits<double>::quiet_NaN()},
                             std::optional<double>{std::numeric_limits<double>::infinity()}}) {
    Ground ground;
    ground.Heights[2] = invalid;
    request.Ground = &ground;
    CHECK(!producer.make(request), "unknown and nonfinite heights refuse generation");
    CHECK(ground.Queries.size() == 3, "generation stops at the unavailable corner");
  }
  return Report();
}

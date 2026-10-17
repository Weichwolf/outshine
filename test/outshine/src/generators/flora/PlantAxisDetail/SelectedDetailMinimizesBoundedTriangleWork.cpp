#include "PlantAxisDetail.h"
#include "PlantAxisSurface.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <limits>
#include <numbers>

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array<PlantAxis, 4> kProfiles{
      PlantAxis{.LengthM = 1, .RadiiM = {0.1, 0.1}},
      PlantAxis{.LengthM = 1, .RadiiM = {0.1, 0.01}},
      PlantAxis{.LengthM = 1, .Bend = -3, .RadiiM = {0.2, 0.01}},
      PlantAxis{.LengthM = 0.35, .Bend = 0.5, .RadiiM = {0.0005, 0.00012}}};
  constexpr std::array kAllowed{0.001, 0.005, 0.01};
  for (const auto &axis : kProfiles) {
    for (const double allowed : kAllowed) {
      const auto detail = SelectPlantAxisDetail(axis, allowed);
      size_t bestTriangles = std::numeric_limits<size_t>::max();
      size_t bestSegments = 0;
      size_t bestSides = 0;
      for (size_t sides = PlantAxisSurfaceLimits::LeastSides;
           sides <= PlantAxisSurfaceLimits::MostSides;
           ++sides) {
        for (size_t segments = PlantAxisSurfaceLimits::LeastSegments;
             segments <= PlantAxisSurfaceLimits::MostSegments;
             ++segments) {
          const auto bound = BoundPlantAxisParametricError(axis, segments, sides);
          if (!bound || *bound > allowed) { continue; }
          const size_t triangles = 2 * sides * (segments + 1);
          if (triangles < bestTriangles) {
            bestTriangles = triangles;
            bestSegments = segments;
            bestSides = sides;
          }
        }
      }
      CHECK(detail && detail->Segments == bestSegments && detail->Sides == bestSides &&
                detail->ParametricErrorM <= allowed,
            "bounded selector agrees with complete integer search; equal triangles prefer fewer "
            "vertices");
    }
  }
  const auto circle = BoundPlantAxisParametricError(kProfiles[0], 2, 3);
  const long double analytic = static_cast<long double>(kProfiles[0].RadiiM[0]) *
                               std::numbers::pi_v<long double> * std::numbers::pi_v<long double> /
                               18;
  CHECK(circle && static_cast<long double>(*circle) >= analytic,
        "rounded coefficient evaluation never undershoots the independent circle-cap bound");
  constexpr double kLooseErrorM = 1;
  const auto minimal = SelectPlantAxisDetail(kProfiles[0], kLooseErrorM);
  CHECK(minimal && minimal->Segments == 2 && minimal->Sides == 3,
        "loose demand emits the smallest supported closed axis without unnecessary work");
  constexpr double kUnreachableErrorM = 1e-12;
  CHECK(!SelectPlantAxisDetail(kProfiles[0], kUnreachableErrorM),
        "unavailable error budget fails rather than silently exceeding it");
  CHECK(!SelectPlantAxisDetail(kProfiles[0], 0) &&
            !SelectPlantAxisDetail(kProfiles[0], std::numeric_limits<double>::quiet_NaN()) &&
            !BoundPlantAxisParametricError(kProfiles[0], 1, 3),
        "invalid error and subdivision requests are rejected before growth");
  auto singular = kProfiles[0];
  singular.LengthM = singular.Bend = 1;
  singular.RadiiM = {1, 1};
  CHECK(!SelectPlantAxisDetail(singular, kLooseErrorM),
        "singular swept tubes cannot obtain a nominal error certificate");
  return Report();
}

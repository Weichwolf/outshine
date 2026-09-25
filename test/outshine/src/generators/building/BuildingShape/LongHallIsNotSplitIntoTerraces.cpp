#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "Check.h"

#include <array>
#include <cmath>
#include <numbers>

namespace {

[[nodiscard]] std::array<double, 8> Rectangle(double lengthM, double widthM) {
  constexpr double kMetresPerDegree = 111320.0;
  constexpr double kLatitudeDeg = 49.3274;
  constexpr double kLongitudeDeg = 8.5659;
  const double eastDeg =
      lengthM / (kMetresPerDegree * std::cos(kLatitudeDeg * std::numbers::pi / 180.0));
  const double northDeg = widthM / kMetresPerDegree;
  return {kLatitudeDeg,
          kLongitudeDeg,
          kLatitudeDeg,
          kLongitudeDeg + eastDeg,
          kLatitudeDeg + northDeg,
          kLongitudeDeg + eastDeg,
          kLatitudeDeg + northDeg,
          kLongitudeDeg};
}

}

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  BuildingScratch scratch;
  const auto hall = Rectangle(352.0, 14.0);
  const auto whole = MassOf(hall, {.HeightM = 9.0, .PitchedShare = 0.0}, {}, scratch);
  CHECK(whole && whole->size() == 1 && whole->front().Use == BuildingUse::Hall &&
            whole->front().Roof == RoofKind::Flat &&
            std::fabs(whole->front().AreaM2 - 352.0 * 14.0) < 100.0,
        "a long hall remains one sourced building rather than invented row houses");

  const auto row = Rectangle(32.0, 6.0);
  const auto terrace = MassOf(row, {.HeightM = 9.0, .PitchedShare = 0.0}, {}, scratch);
  bool allTerraces = terrace && terrace->size() > 1;
  double totalAreaM2 = 0.0;
  if (terrace) {
    for (const BuildingShape &part : *terrace) {
      allTerraces &=
          part.Use == BuildingUse::Terrace && part.Roof == RoofKind::Flat && part.TopM() > 0.0;
      totalAreaM2 += part.AreaM2;
    }
  }
  CHECK(allTerraces && std::fabs(totalAreaM2 - 32.0 * 6.0) < 10.0,
        "a moderate terrace may split while retaining its footprint and explicit flat roof");

  constexpr std::array<std::array<double, 2>, 8> footprint{{{0.0, 0.0},
                                                            {352.0, 0.0},
                                                            {352.0, 8.0},
                                                            {60.0, 8.0},
                                                            {60.0, 23.0},
                                                            {20.0, 23.0},
                                                            {20.0, 8.0},
                                                            {0.0, 8.0}}};
  std::array<double, footprint.size() * 2> winged{};
  for (size_t point = 0; point < footprint.size(); ++point) {
    winged[2 * point] = 49.3274 + footprint[point][1] / 111320.0;
    winged[2 * point + 1] =
        8.5659 + footprint[point][0] / (111320.0 * std::cos(49.3274 * std::numbers::pi / 180.0));
  }
  const auto wings = MassOf(winged, {.HeightM = 9.0, .PitchedShare = 0.0}, {}, scratch);
  bool oneUse = wings && wings->size() > 1;
  if (wings) {
    for (const BuildingShape &part : *wings) {
      oneUse &= part.Use == BuildingUse::Hall && part.Roof == RoofKind::Flat;
    }
  }
  CHECK(oneUse, "a wing cut may split geometry but cannot invent a different building use");
  return Report();
}

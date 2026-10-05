#include "BuildingScratch.h"
#include "BuildingShape.h"
#include "Check.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>

namespace {

std::array<double, 8> Rectangle(double lengthM, double widthM) {
  constexpr double latitude = 48.23;
  constexpr double longitude = 16.41;
  constexpr double metresPerDegree = 111320.0;
  const double east = lengthM / (metresPerDegree * std::cos(latitude * std::numbers::pi / 180.0));
  const double north = widthM / metresPerDegree;
  return {latitude,
          longitude,
          latitude,
          longitude + east,
          latitude + north,
          longitude + east,
          latitude + north,
          longitude};
}

}

int main() {
  using namespace outshine::Generators;
  using namespace outshine::Test;
  BuildingScratch scratch;
  for (const double height : {18.0, 20.0, 24.0, 36.0}) {
    const auto block = MassOf(Rectangle(36.0, 24.0), {.HeightM = height}, {}, scratch);
    CHECK(block && block->size() == 1 && block->front().Form == BuildingForm::Block &&
              block->front().Roof == RoofKind::Mansard,
          "a broad city block does not become a tower at an arbitrary height threshold");
    if (block && !block->empty()) {
      CHECK_NEAR(block->front().TopM(),
                 height,
                 0.000001,
                 "block height",
                 "roof and body share the supplied height instead of extending it");
    }
  }
  const auto hall = MassOf(Rectangle(80.0, 20.0), {.HeightM = 24.0}, {}, scratch);
  CHECK(hall && hall->size() == 1 && hall->front().Form == BuildingForm::Hall,
        "a high wide hall retains its footprint-based family");
  for (const double width : {4.0, 10.0}) {
    const auto shaft = MassOf(Rectangle(width, width), {.HeightM = 65.0}, {}, scratch);
    CHECK(shaft && shaft->size() == 1 && shaft->front().Form == BuildingForm::Tower &&
              shaft->front().Roof == RoofKind::Flat,
          "unknown slender and small tall forms are neutral without inventing a church roof");
    if (shaft && !shaft->empty()) {
      CHECK_NEAR(shaft->front().TopM(),
                 65.0,
                 0.000001,
                 "shaft height",
                 "neutral rooftop retains the supplied silhouette height");
    }
  }
  const auto pitched =
      MassOf(Rectangle(10.0, 10.0), {.HeightM = 65.0, .PitchedShare = 1.0}, {}, scratch);
  CHECK(pitched && pitched->size() == 1 && pitched->front().Form == BuildingForm::Spire &&
            pitched->front().Roof == RoofKind::Hip,
        "explicit pitched roof evidence remains effective for a tall slender form");
  const auto flat =
      MassOf(Rectangle(36.0, 24.0), {.HeightM = 24.0, .PitchedShare = 0.0}, {}, scratch);
  CHECK(flat && !flat->empty() &&
            std::ranges::all_of(*flat,
                                [](const BuildingShape &part) {
                                  return part.Roof == RoofKind::Flat && part.TopM() <= 24.0;
                                }),
        "supplied flat roof overrides the procedural city block roof");
  return Report();
}

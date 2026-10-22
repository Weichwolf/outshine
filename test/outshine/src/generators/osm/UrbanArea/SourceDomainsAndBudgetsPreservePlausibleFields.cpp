#include "UrbanArea.h"
#include "OsmField.h"
#include "Check.h"
#include "TileGeodesy.h"
#include "math/Vec2.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
namespace Osm = outshine::Generators::Osm;
constexpr int kZoom = 16;
constexpr Ground::TileAt kTile{.X = 32767, .Y = 32768};
constexpr double kPitchM = 32;
constexpr double kHalfLotM = 12;
constexpr std::array<double, 4> kWater{-400, -400, -250, -250};
constexpr std::array<double, 4> kPark{-500, -450, -300, -200};
constexpr std::array<double, 4> kRoad{-109, -2000, -91, 2000};
const std::array<std::string, 4> kLayers{"sites", "water_polygons", "park", "streets"};

std::vector<double> Geographic(std::span<const Vec2> points) {
  std::vector<double> result;
  for (const auto &p : points) {
    const auto geo =
        Ground::TileFracToGeo({.X = kTile.X + 1 + p[0] / (Data::kMercatorGirthM / 65536),
                               .Y = kTile.Y - p[1] / (Data::kMercatorGirthM / 65536)},
                              kZoom);
    result.push_back(geo.LatitudeDeg);
    result.push_back(geo.LongitudeDeg);
  }
  return result;
}

Osm::OsmField::Declared Area(const std::string &layer, std::array<double, 4> box) {
  const std::array<Vec2, 4> points{
      {{{box[0], box[1]}}, {{box[2], box[1]}}, {{box[2], box[3]}}, {{box[0], box[3]}}}};
  return {.Layer = layer,
          .Key = "kind",
          .Value = "residential",
          .Area = true,
          .LatLon = Geographic(points)};
}

Osm::OsmField::Declared Road(bool tunnel) {
  const double x = tunnel ? -200 : -100;
  const std::array<Vec2, 2> points{{{{x, -1200}}, {{x, 100}}}};
  return {.Layer = "streets",
          .Key = "kind",
          .Value = "primary",
          .Tunnel = tunnel,
          .LatLon = Geographic(points)};
}

bool Intersects(std::array<double, 4> box, const Vec2 &centre) {
  return centre[0] + kHalfLotM > box[0] && centre[0] - kHalfLotM < box[2] &&
         centre[1] + kHalfLotM > box[1] && centre[1] - kHalfLotM < box[3];
}

void Exclusions() {
  const auto urban = Area("sites", {-1500, -1500, 1000, 1000});
  Osm::OsmField base(kZoom, kLayers);
  base.Declare(std::span(&urban, 1), kTile, "urban-source-1");
  const std::array<const Osm::OsmField *, 1> plain{&base};
  const auto original = Osm::GenerateUrbanArea(plain);
  CHECK(original.has_value(), "native owner field cooks from declared source evidence");
  if (!original) { return; }
  CHECK(original->Occupied > 100 && original->HasUrbanEvidence,
        "urban prior is nonempty and explicit");
  CHECK((original->FirstCell == std::array<int64_t, 2>{-19, -1}),
        "negative global cells use canonical southward rows");
  const std::array rows{
      urban, Area("water_polygons", kWater), Area("park", kPark), Road(false), Road(true)};
  Osm::OsmField constrained(kZoom, kLayers);
  constrained.Declare(rows, kTile);
  const std::array<const Osm::OsmField *, 1> inputs{&constrained};
  const auto product = Osm::GenerateUrbanArea(inputs);
  CHECK(product.has_value(), "open-space constraints cook");
  if (!product) { return; }
  CHECK(product->Tiers.size() == original->Tiers.size(), "exclusions preserve grid ownership");
  if (product->Tiers.size() != original->Tiers.size()) { return; }
  size_t tunnelLots = 0;
  for (uint32_t row = 0; row < product->Rows; ++row) {
    for (uint32_t column = 0; column < product->Columns; ++column) {
      const Vec2 centre{{(static_cast<double>(product->FirstCell[0] + column) + .5) * kPitchM,
                         (static_cast<double>(product->FirstCell[1] - row) + .5) * kPitchM}};
      const bool cut =
          Intersects(kWater, centre) || Intersects(kPark, centre) || Intersects(kRoad, centre);
      const size_t at = static_cast<size_t>(row) * product->Columns + column;
      CHECK(product->Tiers[at] == (cut ? 0 : original->Tiers[at]),
            "analytic rectangular exclusions retain every other original tier");
      if (std::abs(centre[0] + 200) < kHalfLotM && product->Tiers[at] != 0) { ++tunnelLots; }
    }
  }
  CHECK(tunnelLots > 0, "underground transport does not clear surface lots");
  CHECK(
      std::ranges::any_of(product->Sources,
                          [](auto source) { return source.Role == Osm::UrbanSourceRole::Tunnel; }),
      "unused underground source retains its reason");
  base.Declare({}, kTile, "replacement");
  CHECK(original->Inputs.size() == 1 && original->Inputs[0].PayloadDigest == "urban-source-1",
        "product owns source receipts after source replacement");
  const auto repeated = Osm::GenerateUrbanArea(inputs);
  CHECK(repeated && repeated->Tiers == product->Tiers,
        "native recipe is deterministic without camera input");
}

void UnknownAndParent() {
  Osm::OsmField owner(kZoom, kLayers);
  owner.Declare({}, kTile);
  const std::array<const Osm::OsmField *, 1> absent{&owner};
  const auto unknown = Osm::GenerateUrbanArea(absent);
  CHECK(unknown && !unknown->HasUrbanEvidence && unknown->Tiers.empty(),
        "missing urban evidence is explicit rather than inferred World Empty");
  const auto urban = Area("sites", {-1500, -1500, 1000, 1000});
  Osm::OsmField parent(kZoom - 1, kLayers);
  parent.Declare(std::span(&urban, 1), Ground::TileAt{.X = kTile.X / 2, .Y = kTile.Y / 2});
  const std::array<const Osm::OsmField *, 2> inherited{&owner, &parent};
  const auto generated = Osm::GenerateUrbanArea(inherited);
  CHECK(generated && generated->HasUrbanEvidence && generated->Occupied > 100,
        "coarse parent urban evidence works without owner buildings");
  const auto covered = Area("water_polygons", {-1500, -1500, 1000, 1000});
  owner.Declare(std::span(&covered, 1), kTile);
  const auto excluded = Osm::GenerateUrbanArea(inherited);
  CHECK(excluded && excluded->HasUrbanEvidence && excluded->Tiers.empty(),
        "fully excluded urban prior retains its evidence distinction");
}

void Budgets() {
  const auto urban = Area("sites", {-1500, -1500, 1000, 1000});
  Osm::OsmField owner(kZoom, kLayers);
  owner.Declare(std::span(&urban, 1), kTile);
  const std::array<const Osm::OsmField *, 1> inputs{&owner};
  CHECK(!Osm::GenerateUrbanArea({}), "missing owner is rejected");
  Osm::UrbanAreaRecipe invalid;
  invalid.FillFraction = std::numeric_limits<double>::quiet_NaN();
  CHECK(!Osm::GenerateUrbanArea(inputs, invalid), "nonfinite recipe is rejected");
  Osm::UrbanAreaLimits limited;
  limited.CellsMost = 1;
  const auto rejected = Osm::GenerateUrbanArea(inputs, {}, limited);
  CHECK(!rejected && rejected.error().find("before geometry work") != std::string::npos,
        "cell budget rejects before GEOS/source-domain work");
  limited = {};
  limited.SourceVerticesMost = 1;
  CHECK(!Osm::GenerateUrbanArea(inputs, {}, limited), "source vertex budget is enforced");
  std::stop_source cancelled;
  cancelled.request_stop();
  limited = {};
  limited.Stop = cancelled.get_token();
  CHECK(!Osm::GenerateUrbanArea(inputs, {}, limited), "cancelled cook returns no partial field");
}
}

int main() {
  Exclusions();
  UnknownAndParent();
  Budgets();
  return outshine::Test::Report();
}

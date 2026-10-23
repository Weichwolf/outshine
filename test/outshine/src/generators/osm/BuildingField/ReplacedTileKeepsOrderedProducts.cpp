#include "BuildingField.h"
#include "Check.h"

#include <algorithm>
#include <initializer_list>
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace {
constexpr auto kWestAcrossM = 12.0;
constexpr auto kCentreAcrossM = 15.0;
constexpr auto kCentreSecondAcrossM = 15.1;
constexpr auto kEastAcrossM = 19.0;
constexpr auto kWestHeightM = 20.0f;
constexpr auto kCentreSpreadM = 5.0;
constexpr auto kCentreSecondSpreadM = 5.1;
constexpr auto kEastSpreadM = 9.0;
constexpr auto kEastHeightM = 90.0f;
using Field = ::outshine::Generators::Osm::BuildingField;

bool MeasurementsMatch(const Field &field,
                       std::initializer_list<double> spread,
                       std::initializer_list<double> across) {
  const auto matches = [&](std::span<const double> expected, auto member) {
    size_t offset = 0;
    for (size_t tile = 0; tile < field.AcceptedTiles().size(); ++tile) {
      const auto &values = field.ProductAt(tile).*member;
      if (values.size() > expected.size() - offset ||
          !std::ranges::equal(values, expected.subspan(offset, values.size()))) {
        return false;
      }
      offset += values.size();
    }
    return offset == expected.size();
  };
  return matches(std::span(spread), &Field::PhysicalProduct::SeatSpreadM) &&
         matches(std::span(across), &Field::PhysicalProduct::AcrossM);
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const ::outshine::Generators::Osm::OsmField empty(14, {});
  ::outshine::Generators::Osm::BuildingField field;
  field.AnchorAt({{0, 0, 0}});

  const auto accept = [&](uint32_t tile,
                          std::span<const ::outshine::Ground::BuildingFootprint> prints,
                          std::span<const double> spread,
                          std::span<const double> across,
                          size_t triangles) {
    const ::outshine::Generators::Osm::BuildingField::Baked baked{
        .Coordinates = {},
        .Prints = prints,
        .SeatSpreadM = spread,
        .AcrossM = across,
        .Triangles = triangles,
        .OsmHeights = static_cast<int>(prints.size())};
    field.PreparesAcceptances(1);
    field.Take(tile);
    auto pending = field.PrepareAcceptance(tile, baked);
    field.CommitAcceptance(std::move(pending), baked);
  };
  const std::array<::outshine::Ground::BuildingFootprint, 1> east{
      {{.HeightM = kEastHeightM, .Street = {}, .TerrainCeiling = {}, .TerrainContacts = {}}}};
  const std::array<::outshine::Ground::BuildingFootprint, 1> west{
      {{.HeightM = kWestHeightM, .Street = {}, .TerrainCeiling = {}, .TerrainContacts = {}}}};
  const std::array<double, 1> eastSpread{kEastSpreadM};
  const std::array<double, 1> westSpread{2.0};
  const std::array<double, 1> eastAcross{kEastAcrossM};
  const std::array<double, 1> westAcross{kWestAcrossM};
  accept(9, east, eastSpread, eastAcross, 9);
  accept(5, {}, {}, {}, 0);
  accept(2, west, westSpread, westAcross, 2);
  CHECK(field.FootprintCount() == 2 && field.OfTile(2).front().HeightM == kWestHeightM &&
            field.OfTile(9).front().HeightM == kEastHeightM && field.OfTile(5).empty() &&
            field.OfTile(9).front().HeightM == kEastHeightM,
        "out-of-order and empty accepted tiles keep correct footprint ranges");
  CHECK(MeasurementsMatch(field, {2.0, kEastSpreadM}, {kWestAcrossM, kEastAcrossM}) &&
            field.IngestedTiles() == 3 && field.Ingested(empty),
        "measurement ranges and first-ingestion watermark follow sorted tile order");

  const ::outshine::Generators::Osm::BuildingField published = field.SnapshotAccepted();
  field.BeginRefinement();
  CHECK(field.RefinementTile() == 2 && field.RefinementRemaining() == 3 &&
            !field.RefinementComplete() && field.AcceptedInputs().size() == 3,
        "refined candidate scans accepted tile IDs without resetting their products");
  field.AdvanceRefinement();
  CHECK(field.RefinementTile() == 5 && field.RefinementRemaining() == 2,
        "accepted tile scan advances in stable sorted order");
  const size_t ingested = field.IngestedTiles();
  const uint64_t firstRevision = field.Revision();
  const std::array<::outshine::Ground::BuildingFootprint, 2> centre{
      {{.HeightM = 50.0f, .Street = {}, .TerrainCeiling = {}, .TerrainContacts = {}},
       {.HeightM = 51.0f, .Street = {}, .TerrainCeiling = {}, .TerrainContacts = {}}}};
  const std::array<double, 2> centreSpread{kCentreSpreadM, kCentreSecondSpreadM};
  const std::array<double, 2> centreAcross{kCentreAcrossM, kCentreSecondAcrossM};
  const Data::TileSourceIdentity fine{.Kind = Data::DataKind::Elevation,
                                      .Tile = {.Zoom = 14, .X = 5, .Y = 0},
                                      .SourceId = "dem",
                                      .Revision = "fine-2"};
  const ::outshine::Generators::Osm::BuildingField::Baked larger{.Coordinates = {},
                                                                 .Prints = centre,
                                                                 .SeatSpreadM = centreSpread,
                                                                 .AcrossM = centreAcross,
                                                                 .Triangles = 10,
                                                                 .OsmHeights = 2};
  field.PreparesAcceptances(0);
  auto pending = field.PrepareAcceptance(5, larger, std::span(&fine, 1), true);
  CHECK(field.OfTile(5).empty() && field.Revision() == firstRevision,
        "preparing replacement leaves published tile unchanged");
  field.ReplaceAcceptance(std::move(pending), larger);
  CHECK(field.FootprintCount() == 4 && field.OfTile(2).front().HeightM == kWestHeightM &&
            field.OfTile(5).size() == 2 && field.OfTile(9).front().HeightM == kEastHeightM &&
            MeasurementsMatch(field,
                              {2.0, kCentreSpreadM, kCentreSecondSpreadM, kEastSpreadM},
                              {kWestAcrossM, kCentreAcrossM, kCentreSecondAcrossM, kEastAcrossM}),
        "larger replacement shifts every later footprint and measurement range");
  CHECK(field.InputOfTile(5) && field.InputOfTile(5)->Qualified &&
            field.InputOfTile(5)->Sources.front() == fine && field.TrianglesHanded() == 21 &&
            field.OsmHeights() == 4 && field.IngestedTiles() == ingested && field.Ingested(empty) &&
            field.Revision() > firstRevision && field.RefinementTile() == 5 &&
            field.RefinementRemaining() == 2,
        "replacement updates source, counters and revision without first-ingestion changes");
  CHECK(published.OfTile(5).empty() && published.TrianglesHanded() == 11 &&
            published.InputOfTile(5) && !published.InputOfTile(5)->Qualified,
        "published snapshot remains unchanged during candidate replacement");

  const ::outshine::Generators::Osm::BuildingField::Baked smaller{};
  auto shrink = field.PrepareAcceptance(5, smaller);
  field.ReplaceAcceptance(std::move(shrink), smaller);
  CHECK(field.OfTile(5).empty() && field.OfTile(9).front().HeightM == kEastHeightM &&
            MeasurementsMatch(field, {2.0, kEastSpreadM}, {kWestAcrossM, kEastAcrossM}) &&
            field.TrianglesHanded() == 11 && field.OsmHeights() == 2 &&
            field.IngestedTiles() == ingested,
        "smaller empty replacement removes stale data and preserves later tiles");
  field.AdvanceRefinement();
  CHECK(field.RefinementTile() == 9 && field.RefinementRemaining() == 1,
        "replacement does not skip the next accepted tile");
  field.AdvanceRefinement();
  CHECK(field.RefinementComplete() && field.Ingested(empty) && !published.RefinementComplete(),
        "only the candidate completes source certification; published state remains independent");
  return Report();
}

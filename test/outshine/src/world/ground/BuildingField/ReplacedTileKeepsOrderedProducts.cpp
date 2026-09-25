#include "BuildingField.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  OsmField empty(14, {});
  BuildingField field;
  field.AnchorAt({{0, 0, 0}});

  const auto accept = [&](uint32_t tile,
                          std::span<const BuildingField::Footprint> prints,
                          std::span<const double> spread,
                          std::span<const double> across,
                          size_t triangles) {
    const BuildingField::Baked baked{.Prints = prints,
                                     .SeatSpreadM = spread,
                                     .AcrossM = across,
                                     .Triangles = triangles,
                                     .OsmHeights = static_cast<int>(prints.size())};
    field.PreparesAcceptances(
        {.Prints = prints.size(), .Spread = spread.size(), .Across = across.size(), .Tiles = 1});
    field.Take(tile);
    auto pending = field.PrepareAcceptance(tile, baked);
    field.CommitAcceptance(std::move(pending), empty, baked);
  };
  const std::array<BuildingField::Footprint, 1> east{{{.HeightM = 90.0f}}};
  const std::array<BuildingField::Footprint, 1> west{{{.HeightM = 20.0f}}};
  const std::array<double, 1> eastSpread{9.0}, westSpread{2.0};
  const std::array<double, 1> eastAcross{19.0}, westAcross{12.0};
  accept(9, east, eastSpread, eastAcross, 9);
  accept(5, {}, {}, {}, 0);
  accept(2, west, westSpread, westAcross, 2);
  CHECK(field.Footprints().size() == 2 && field.Footprints()[0].HeightM == 20.0f &&
            field.Footprints()[1].HeightM == 90.0f && field.OfTile(5).empty() &&
            field.OfTile(9).front().HeightM == 90.0f,
        "out-of-order and empty accepted tiles keep correct footprint ranges");
  CHECK(field.SeatSpreadM() == std::vector<double>({2.0, 9.0}) &&
            field.FootprintAcrossM() == std::vector<double>({12.0, 19.0}) &&
            field.IngestedTiles() == 3 && field.Ingested(empty),
        "measurement ranges and first-ingestion watermark follow sorted tile order");

  BuildingField published = field.SnapshotAccepted();
  field.BeginRefinement();
  CHECK(field.RefinementTile() == 2 && field.RefinementRemaining() == 3 &&
            !field.RefinementComplete() && field.AcceptedInputs().size() == 3,
        "refined candidate scans accepted tile IDs without resetting their products");
  field.AdvanceRefinement();
  CHECK(field.RefinementTile() == 5 && field.RefinementRemaining() == 2,
        "accepted tile scan advances in stable sorted order");
  const size_t ingested = field.IngestedTiles();
  const uint64_t firstRevision = field.Revision();
  const std::array<BuildingField::Footprint, 2> centre{{{.HeightM = 50.0f}, {.HeightM = 51.0f}}};
  const std::array<double, 2> centreSpread{5.0, 5.1}, centreAcross{15.0, 15.1};
  const Data::TileSourceIdentity fine{.Kind = Data::DataKind::Elevation,
                                      .Tile = {.Zoom = 14, .X = 5, .Y = 0},
                                      .SourceId = "dem",
                                      .Revision = "fine-2"};
  const BuildingField::Baked larger{.Prints = centre,
                                    .SeatSpreadM = centreSpread,
                                    .AcrossM = centreAcross,
                                    .Triangles = 10,
                                    .OsmHeights = 2};
  field.PreparesAcceptances(
      {.Prints = centre.size(), .Spread = centreSpread.size(), .Across = centreAcross.size()});
  auto pending = field.PrepareAcceptance(5, larger, std::span(&fine, 1), true);
  CHECK(field.OfTile(5).empty() && field.Revision() == firstRevision,
        "preparing replacement leaves published tile unchanged");
  field.ReplaceAcceptance(std::move(pending), larger);
  CHECK(field.Footprints().size() == 4 && field.OfTile(2).front().HeightM == 20.0f &&
            field.OfTile(5).size() == 2 && field.OfTile(9).front().HeightM == 90.0f &&
            field.SeatSpreadM() == std::vector<double>({2.0, 5.0, 5.1, 9.0}) &&
            field.FootprintAcrossM() == std::vector<double>({12.0, 15.0, 15.1, 19.0}),
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

  const BuildingField::Baked smaller{};
  auto shrink = field.PrepareAcceptance(5, smaller);
  field.ReplaceAcceptance(std::move(shrink), smaller);
  CHECK(field.OfTile(5).empty() && field.OfTile(9).front().HeightM == 90.0f &&
            field.SeatSpreadM() == std::vector<double>({2.0, 9.0}) &&
            field.FootprintAcrossM() == std::vector<double>({12.0, 19.0}) &&
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

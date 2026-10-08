#include <array>
#include <string>

#include "BuildingField.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"building"};
  ::outshine::Generators::Osm::OsmField vectors(14, layers);
  const std::array<::outshine::Generators::Osm::OsmField::Declared, 1> declared{
      {{.Layer = "building",
        .Key = "kind",
        .Value = "house",
        .Area = true,
        .LatLon = {47.0, 9.0, 47.0, 9.001, 47.001, 9.001, 47.001, 9.0}}}};
  vectors.Declare(declared, TileAt{.X = 8, .Y = 9});
  ::outshine::Generators::Osm::BuildingField field;
  field.AnchorAt({{1.0, 2.0, 3.0}});
  CHECK(!field.IngestedWithin(vectors, 0),
        "required coverage waits while its source tile has no accepted product");
  const uint32_t tile = vectors.Features().front().Tile;
  field.Take(tile);
  ::outshine::Generators::Osm::BuildingField snapshot = field.SnapshotAccepted();
  CHECK(!field.Next(
            vectors, [](::outshine::Generators::Osm::FeatureRun) { return true; }, 1) &&
            snapshot.Next(
                        vectors, [](::outshine::Generators::Osm::FeatureRun) { return true; }, 1)
                .has_value(),
        "candidate snapshot retries a reserved tile while its source keeps ownership");
  CHECK(!snapshot.Ingested(vectors) && !snapshot.IngestedWithin(vectors, 0),
        "unfinished source tile is not accepted by the candidate snapshot");
  const ::outshine::Generators::Osm::BuildingField::Baked empty;
  field.PreparesAcceptances({.Tiles = 1});
  auto pending = field.PrepareAcceptance(tile, empty);
  field.CommitAcceptance(std::move(pending), empty);
  CHECK(field.IngestedWithin(vectors, 0),
        "required coverage becomes ready only after its tile product is accepted");
  CHECK(field.SnapshotAccepted().Ingested(vectors),
        "accepted tile stays complete when copied into a candidate");
  const auto priorFeatures = vectors.Features().size();
  auto expanded = std::array{declared.front(), declared.front()};
  expanded[1].Value = "second-house";
  expanded[1].LatLon = {47.0002, 9.0002, 47.0002, 9.0004, 47.0004, 9.0004, 47.0004, 9.0002};
  vectors.Declare(expanded, TileAt{.X = 8, .Y = 9});
  CHECK(vectors.Features().size() > priorFeatures && vectors.Features().back().Tile == tile,
        "replacement extends the feature run of an already accepted tile");
  pending = field.PrepareAcceptance(tile, empty);
  field.ReplaceAcceptance(std::move(pending), empty);
  size_t resolutions = 0;
  CHECK(!field.Next(
            vectors,
            [&resolutions](::outshine::Generators::Osm::FeatureRun) {
              ++resolutions;
              return true;
            },
            1) &&
            field.Ingested(vectors) && resolutions == 0,
        "scan catches up after accepted replacement without reserving or resolving the tile again");
  field.ResetDerived();
  CHECK(!field.IngestedWithin(vectors, 0),
        "reset removes accepted coverage together with derived geometry");
  return Report();
}

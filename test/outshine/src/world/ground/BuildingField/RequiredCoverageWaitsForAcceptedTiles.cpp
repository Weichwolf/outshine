#include <array>
#include <string>

#include "BuildingField.h"
#include "Check.h"

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const std::array<std::string, 1> layers{"building"};
  OsmField vectors(14, layers);
  const std::array<OsmField::Declared, 1> declared{
      {{.Layer = "building",
        .Key = "kind",
        .Value = "house",
        .Area = true,
        .LatLon = {47.0, 9.0, 47.0, 9.001, 47.001, 9.001, 47.001, 9.0}}}};
  vectors.Declare(declared, TileAt{.X = 8, .Y = 9});
  BuildingField field;
  field.AnchorAt({{1.0, 2.0, 3.0}});
  CHECK(!field.IngestedWithin(vectors, 0),
        "required coverage waits while its source tile has no accepted product");
  const uint32_t tile = vectors.Features().front().Tile;
  field.Take(tile);
  BuildingField snapshot = field.SnapshotAccepted();
  CHECK(!field.Next(
            vectors, [](FeatureRun) { return true; }, 1) &&
            snapshot.Next(
                        vectors, [](FeatureRun) { return true; }, 1)
                .has_value(),
        "candidate snapshot retries a reserved tile while its source keeps ownership");
  CHECK(!snapshot.Ingested(vectors) && !snapshot.IngestedWithin(vectors, 0),
        "unfinished source tile is not accepted by the candidate snapshot");
  const BuildingField::Baked empty;
  auto pending = field.PrepareAcceptance(tile, empty);
  field.CommitAcceptance(std::move(pending), vectors, empty);
  CHECK(field.IngestedWithin(vectors, 0),
        "required coverage becomes ready only after its tile product is accepted");
  CHECK(field.SnapshotAccepted().Ingested(vectors),
        "accepted tile stays complete when copied into a candidate");
  field.ResetDerived();
  CHECK(!field.IngestedWithin(vectors, 0),
        "reset removes accepted coverage together with derived geometry");
  return Report();
}

#include "BuildingField.h"
#include "Check.h"
#include <array>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  OsmField vectors(14, {});
  BuildingField field;
  field.AnchorAt({{1, 2, 3}});
  Data::TileSourceIdentity source{.Kind = Data::DataKind::Elevation,
                                  .Tile = {.Zoom = 14, .X = 4, .Y = 7},
                                  .SourceId = "dem",
                                  .Revision = "revision-a"};
  Data::TileSourceIdentity vector{.Kind = Data::DataKind::VectorMap,
                                  .Tile = {.Zoom = 14, .X = 4, .Y = 7},
                                  .SourceId = "osm",
                                  .Revision = "vector-a"};
  const BuildingField::Baked empty;
  field.PreparesAcceptances({.Tiles = 1});
  field.Take(7);
  auto pending = field.PrepareAcceptance(7,
                                         empty,
                                         std::span(&source, 1),
                                         true,
                                         vector,
                                         {.HeightRasterDigest = 17,
                                          .StreetDigest = 19,
                                          .FocalPx = 90,
                                          .TileSpanM = 100,
                                          .Eye = {.LongitudeDeg = 8, .LatitudeDeg = 47}});
  CHECK(field.InputOfTile(7) == nullptr,
        "preparing an empty tile does not publish its source identity");
  source.Revision = "revision-b";
  vector.Revision = "vector-b";
  field.CommitAcceptance(std::move(pending), vectors, empty);
  const auto *accepted = field.InputOfTile(7);
  CHECK(accepted && accepted->Qualified && accepted->Sources.size() == 1 &&
            accepted->Sources.front().Revision == "revision-a" && accepted->Vector &&
            accepted->Vector->Revision == "vector-a" && accepted->Bake.HeightRasterDigest == 17 &&
            accepted->Bake.StreetDigest == 19 && accepted->Bake.FocalPx == 90 &&
            accepted->Bake.TileSpanM == 100 && accepted->Bake.Eye.LongitudeDeg == 8 &&
            accepted->Bake.Eye.LatitudeDeg == 47,
        "accepted empty tile owns both source revisions captured before publication");
  BuildingField snapshot = field.SnapshotAccepted();
  field.ResetDerived();
  CHECK(field.InputOfTile(7) == nullptr && snapshot.InputOfTile(7) &&
            snapshot.InputOfTile(7)->Sources.front().Revision == "revision-a" &&
            snapshot.InputOfTile(7)->Vector &&
            snapshot.InputOfTile(7)->Vector->Revision == "vector-a" &&
            snapshot.InputOfTile(7)->Bake.HeightRasterDigest == 17 &&
            snapshot.InputOfTile(7)->Bake.StreetDigest == 19,
        "candidate snapshot retains source identity after source reset");
  CHECK(snapshot.HeapBytes() >= sizeof(BuildingField::AcceptedInput),
        "retained source records count toward the streaming heap budget");
  return Report();
}

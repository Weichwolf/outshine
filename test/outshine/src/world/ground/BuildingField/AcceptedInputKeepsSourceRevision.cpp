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
  HeightField::Request request{
      .Zoom = 15,
      .Tiles = {{.Zoom = 15, .X = 9, .Y = 14}, {.Zoom = 15, .X = 8, .Y = 14}},
      .Fallback = false};
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
                                          .Eye = {.LongitudeDeg = 8, .LatitudeDeg = 47}},
                                         {},
                                         request);
  CHECK(field.InputOfTile(7) == nullptr,
        "preparing an empty tile does not publish its source identity");
  request.Tiles.clear();
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
  CHECK(accepted && accepted->Heights.Zoom == 15 && !accepted->Heights.Fallback &&
            accepted->Heights.Tiles.size() == 2 && accepted->Heights.Tiles[0].X == 9 &&
            accepted->Heights.Tiles[1].X == 8,
        "accepted recipe owns ordered child requests, independently of resolved ancestor sources");
  const size_t accounted = field.HeapBytes();
  BuildingField snapshot = field.SnapshotAccepted();
  field.ResetDerived();
  CHECK(field.InputOfTile(7) == nullptr && snapshot.InputOfTile(7) &&
            snapshot.InputOfTile(7)->Sources.front().Revision == "revision-a" &&
            snapshot.InputOfTile(7)->Vector &&
            snapshot.InputOfTile(7)->Vector->Revision == "vector-a" &&
            snapshot.InputOfTile(7)->Bake.HeightRasterDigest == 17 &&
            snapshot.InputOfTile(7)->Bake.StreetDigest == 19,
        "candidate snapshot retains source identity after source reset");
  CHECK(snapshot.InputOfTile(7)->Heights.Tiles.size() == 2 &&
            snapshot.InputOfTile(7)->Heights.Tiles[0].X == 9,
        "snapshot owns request addresses after producer and accepted field reset");
  BuildingField noRequests = snapshot;
  auto replacement = noRequests.PrepareAcceptance(
      7, empty, std::span(&source, 1), true, vector, BuildingField::BakeInputs{});
  noRequests.ReplaceAcceptance(std::move(replacement), empty);
  CHECK(noRequests.InputOfTile(7)->Heights.Tiles.empty(),
        "legacy acceptances retain no invented requests");
  CHECK(accounted >= snapshot.HeapBytes() &&
            snapshot.HeapBytes() >= noRequests.HeapBytes() + 2 * sizeof(TileSpot),
        "request address storage counts toward retained heap bytes");
  CHECK(snapshot.HeapBytes() >= sizeof(BuildingField::AcceptedInput),
        "retained source records count toward the streaming heap budget");
  return Report();
}

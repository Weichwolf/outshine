#include "BuildingField.h"
#include "Check.h"
#include <array>
#include <cstddef>
#include <string>
#include <utility>

namespace {
constexpr auto kTileSpanM = 100;
constexpr auto kHeightDigest = 17;
constexpr auto kStreetDigest = 19;
constexpr auto kLatitudeDeg = 47;
constexpr auto kFocalPx = 90;
}

int main() {
  using namespace outshine;
  using namespace outshine::Ground;
  using namespace outshine::Test;
  const ::outshine::Generators::Osm::OsmField vectors(14, {});
  ::outshine::Generators::Osm::BuildingField field;
  field.AnchorAt({{1, 2, 3}});
  Data::TileSourceIdentity source{.Kind = Data::DataKind::Elevation,
                                  .Tile = {.Zoom = 14, .X = 4, .Y = 7},
                                  .SourceId = "dem",
                                  .Revision = "revision-a"};
  Data::TileSourceIdentity vector{.Kind = Data::DataKind::VectorMap,
                                  .Tile = {.Zoom = 14, .X = 4, .Y = 7},
                                  .SourceId = "osm",
                                  .Revision = "vector-a"};
  const ::outshine::Generators::Osm::BuildingField::Baked empty;
  HeightField::Request request{
      .Zoom = 15,
      .Tiles = {{.Zoom = 15, .X = 9, .Y = 14}, {.Zoom = 15, .X = 8, .Y = 14}},
      .Fallback = false};
  field.PreparesAcceptances(1);
  field.Take(7);
  auto pending = field.PrepareAcceptance(7,
                                         empty,
                                         std::span(&source, 1),
                                         true,
                                         vector,
                                         {.HeightRasterDigest = kHeightDigest,
                                          .StreetDigest = kStreetDigest,
                                          .Projection = {.FocalPx = kFocalPx},
                                          .TileSpanM = kTileSpanM,
                                          .Eye = {.LongitudeDeg = 8, .LatitudeDeg = kLatitudeDeg},
                                          .EyeEcef = {}},
                                         {},
                                         request);
  CHECK(field.InputOfTile(7) == nullptr,
        "preparing an empty tile does not publish its source identity");
  request.Tiles.clear();
  source.Revision = "revision-b";
  vector.Revision = "vector-b";
  field.CommitAcceptance(std::move(pending), empty);
  const auto *accepted = field.InputOfTile(7);
  CHECK(accepted && accepted->Qualified && accepted->Sources.size() == 1 &&
            accepted->Sources.front().Revision == "revision-a" && accepted->Vector &&
            accepted->Vector->Revision == "vector-a" &&
            accepted->Bake.HeightRasterDigest == kHeightDigest &&
            accepted->Bake.StreetDigest == kStreetDigest &&
            accepted->Bake.Projection.FocalPx == kFocalPx &&
            accepted->Bake.TileSpanM == kTileSpanM && accepted->Bake.Eye.LongitudeDeg == 8 &&
            accepted->Bake.Eye.LatitudeDeg == kLatitudeDeg,
        "accepted empty tile owns both source revisions captured before publication");
  CHECK(accepted && accepted->Heights.Zoom == 15 && !accepted->Heights.Fallback &&
            accepted->Heights.Tiles.size() == 2 && accepted->Heights.Tiles[0].X == 9 &&
            accepted->Heights.Tiles[1].X == 8,
        "accepted recipe owns ordered child requests, independently of resolved ancestor sources");
  const size_t accounted = field.HeapBytes();
  const ::outshine::Generators::Osm::BuildingField snapshot = field.SnapshotAccepted();
  field.ResetDerived();
  const auto *stored = snapshot.InputOfTile(7);
  CHECK(field.InputOfTile(7) == nullptr && stored &&
            stored->Sources.front().Revision == "revision-a" && stored->Vector && stored->Vector &&
            stored->Vector->Revision == "vector-a" &&
            stored->Bake.HeightRasterDigest == kHeightDigest &&
            stored->Bake.StreetDigest == kStreetDigest,
        "candidate snapshot retains source identity after source reset");
  CHECK(stored->Heights.Tiles.size() == 2 && stored->Heights.Tiles[0].X == 9,
        "snapshot owns request addresses after producer and accepted field reset");
  ::outshine::Generators::Osm::BuildingField noRequests = snapshot;
  auto replacement =
      noRequests.PrepareAcceptance(7,
                                   empty,
                                   std::span(&source, 1),
                                   true,
                                   vector,
                                   ::outshine::Generators::Osm::BuildingField::BakeInputs{});
  noRequests.ReplaceAcceptance(std::move(replacement), empty);
  CHECK(noRequests.InputOfTile(7)->Heights.Tiles.empty(),
        "legacy acceptances retain no invented requests");
  CHECK(accounted >= snapshot.HeapBytes() &&
            snapshot.HeapBytes() >= noRequests.HeapBytes() + 2 * sizeof(TileSpot),
        "request address storage counts toward retained heap bytes");
  CHECK(snapshot.HeapBytes() >= sizeof(::outshine::Generators::Osm::BuildingField::AcceptedInput),
        "retained source records count toward the streaming heap budget");
  return Report();
}

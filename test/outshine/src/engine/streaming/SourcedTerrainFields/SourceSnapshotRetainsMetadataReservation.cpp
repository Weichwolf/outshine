#include "Check.h"
#include "SourcedTerrainFields.h"

#include <array>
#include <cstdint>
#include <memory>
#include <optional>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Index = Ground::TerrainRevisionIndex;
  auto created = Index::Create(1);
  CHECK(created.has_value(), "source owns a bounded revision index");
  if (!created) { return Report(); }
  auto &index = **created;
  const Data::TileId tile{.Zoom = 4, .X = 0, .Y = 1};
  auto metadata = index.ReserveFor(std::array{tile});
  CHECK(metadata.has_value(), "source obtains a metadata reservation");
  if (!metadata) { return Report(); }
  const auto stamp = *index.IssueDeliveryStamp(tile);
  auto raster = std::make_shared<Ground::TerrainField>(2, 2);
  raster->AddSource(
      {.Kind = Data::DataKind::Elevation, .Tile = tile, .SourceId = "dem", .Revision = "r1"});
  raster->SetCertificate(Ground::TerrainCertificate::FromDelivery(tile, stamp, 1));
  std::vector<SourcedTerrainFields::Entry> fields{{tile, raster}};
  SourcedTerrainFields snapshot(fields, *metadata);
  std::optional<SourcedTerrainFields> captured(snapshot);
  snapshot = SourcedTerrainFields{};
  metadata->reset();
  fields.clear();
  raster.reset();
  for (uint32_t x = 0; x < 10; ++x) {
    CHECK(index.IssueDeliveryStamp({.Zoom = 4, .X = x, .Y = 0}).has_value(),
          "unrelated source arrivals remain bounded");
  }
  CHECK(index.AreCurrent(std::array{stamp}),
        "background snapshot retains metadata after its original world owner releases it");
  captured.reset();
  for (uint32_t x = 10; x < 14; ++x) {
    CHECK(index.IssueDeliveryStamp({.Zoom = 4, .X = x, .Y = 0}).has_value(),
          "source cache continues after snapshot retirement");
  }
  CHECK(!index.AreCurrent(std::array{stamp}),
        "last snapshot consumer permits metadata eviction again");
  return Report();
}

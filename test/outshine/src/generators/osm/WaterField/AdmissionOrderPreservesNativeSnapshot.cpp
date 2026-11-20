#include "Check.h"
#include "OsmField.h"
#include "TerrainLoader.h"
#include "WaterField.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <fstream>
#include <ios>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
using namespace outshine::Generators::Osm;

using Bytes = std::vector<uint8_t>;
constexpr size_t kEncodedBudget = size_t{128} * 1024;

Bytes ReadTile(const char *path) {
  std::ifstream input(path, std::ios::binary | std::ios::ate);
  const std::streamoff size = input ? input.tellg() : std::streampos{-1};
  CHECK(size > 0 && size <= 1024, "fixed MVT input has the expected bounded size");
  if (size <= 0 || size > 1024) { return {}; }
  Bytes bytes(static_cast<size_t>(size));
  input.seekg(0);
  input.read(reinterpret_cast<char *>(bytes.data()), size);
  CHECK(input, "fixed MVT input is read completely");
  return bytes;
}

class DeferredHeights final : public GroundQuery {
public:
  bool WestPending = false;
  bool EastPending = false;
  bool TerrainCurrent = true;
  uint64_t Scope = 0;
  Ground::TerrainCertificate Certificate;

  [[nodiscard]] GroundSample At(LongitudeLatitude at) const override {
    if (WestPending && at.LongitudeDeg < 0) { return GroundSample::Waiting(); }
    if (EastPending && at.LongitudeDeg > 0) { return GroundSample::Waiting(); }
    return GroundSample::At(100.0 + at.LongitudeDeg / 100.0 + at.LatitudeDeg / 100.0);
  }

  [[nodiscard]] HeightReading ReadHeight(LongitudeLatitude at) const override {
    return {.Sample = At(at), .Certificate = &Certificate, .ResidentRevision = 1};
  }

  [[nodiscard]] uint64_t TerrainScopeRevision() const noexcept override { return Scope; }

  [[nodiscard]] bool HeightInputsCurrent(const Ground::TerrainCertificate &) const override {
    return TerrainCurrent;
  }

  [[nodiscard]] GroundSample Resident(LongitudeLatitude at) const override { return At(at); }

  [[nodiscard]] Ground::GroundBlock BlockAt([[maybe_unused]] Ground::TileSpot tile) const override {
    return Ground::GroundBlock::Waiting();
  }

  [[nodiscard]] double PostM([[maybe_unused]] double latitudeDeg) const override { return 1; }
};

Generators::WaterAsset Snapshot(const OsmField &field, bool deferWest) {
  WaterField water;
  DeferredHeights ground;
  ground.WestPending = deferWest;
  const outshine::Generators::RegionSurfaceRecipes materials;
  if (deferWest) {
    for (unsigned step = 0; step < 32 && water.OfTile(1).empty(); ++step) {
      (void)water.Ingest(ground, field, materials);
    }
    CHECK(water.OfTile(0).empty() && !water.OfTile(1).empty(),
          "east water is admitted while the west DEM remains pending");
    const auto partial = water.Asset(field);
    CHECK(!partial.Complete() && partial.OfTile(0).empty() && !partial.OfTile(1).empty(),
          "partial native snapshots preserve per-tile readiness and ranges");
  }
  ground.WestPending = false;
  for (unsigned step = 0; step < 32 && !water.Ingested(field); ++step) {
    (void)water.Ingest(ground, field, materials);
  }
  CHECK(water.Ingested(field), "all unchanged source tiles finish after DEM readiness");
  const auto asset = water.Asset(field);
  CHECK(asset.Complete() && asset.Surfaces().size() == 2 && asset.Courses().size() == 2,
        "both polygon and flowing sources remain in the completed snapshot");
  CHECK(asset.OfTile(0).size() == 1 && asset.OfTile(1).size() == 1 && asset.OfTile(2).empty(),
        "tile ranges retain both populated tiles and the empty source tile");
  for (const auto &surface : asset.Surfaces()) {
    CHECK(asset.RingsOf(surface).size() == 2 && asset.InputOf(surface) != nullptr,
          "each retained polygon keeps its hole and exact source binding");
  }
  const auto bytes = asset.EncodeNative(kEncodedBudget);
  CHECK(bytes, "the complete native snapshot encodes within the fixture budget");
  if (bytes) {
    const auto restored = Generators::WaterAsset::DecodeNative(*bytes);
    CHECK(restored && restored->EncodeNative(kEncodedBudget) == bytes,
          "reordered ring, course and level spans remain valid through the native codec");
  }
  return asset;
}

void CheckOverlappingWindows(std::span<const std::string> layers,
                             const Bytes &tile,
                             const Bytes &empty,
                             const Generators::WaterAsset &expected) {
  OsmField exposed(2, layers);
  CHECK(exposed.Accept(2, 1, tile), "the east source is present in the initial window");
  DeferredHeights ground;
  ground.Scope = 1;
  auto revisions = Ground::TerrainRevisionIndex::Create();
  CHECK(revisions, "the fixture has an elevation revision index");
  if (!revisions) { return; }
  const Data::TileId terrain{.Zoom = 2, .X = 2, .Y = 1};
  const auto stamp = (**revisions).IssueDeliveryStamp(terrain);
  CHECK(stamp, "the fixture height has a delivery stamp");
  if (!stamp) { return; }
  ground.Certificate = Ground::TerrainCertificate::FromDelivery(terrain, *stamp, ground.Scope);
  WaterField water;
  const Generators::RegionSurfaceRecipes materials;
  for (int i = 0; i < 32 && !water.Ingested(exposed); ++i) {
    (void)water.Ingest(ground, exposed, materials);
  }
  const auto first = water.Asset(exposed);
  CHECK(first.Complete() && first.OfTile(0).size() == 1, "the first window publishes east water");
  CHECK(exposed.Accept(1, 1, tile) && exposed.Accept(3, 1, empty),
        "the overlapping window adds a western neighbour and shifts the east source index");
  CHECK(!water.Asset(exposed).Complete(), "the former geometry is not relabelled as current");
  ground.WestPending = true;
  ground.EastPending = true;
  (void)water.Ingest(ground, exposed, materials);
  CHECK(water.OfTile(1).size() == 1 && water.OfTile(0).empty(),
        "east water survives the index shift while both terrain sources are pending");
  ground.WestPending = false;
  for (int i = 0; i < 32 && !water.Ingested(exposed); ++i) {
    (void)water.Ingest(ground, exposed, materials);
  }
  const auto final = water.Asset(exposed);
  CHECK(final.Complete() && std::ranges::equal(final.Levels(), expected.Levels()) &&
            std::ranges::equal(final.Surfaces(),
                               expected.Surfaces(),
                               std::ranges::equal_to{},
                               &Generators::WaterAsset::Surface::LevelM,
                               &Generators::WaterAsset::Surface::LevelM) &&
            first.OfTile(0).size() == 1,
        "reused heights produce the same water and keep the old snapshot valid");
  ground.TerrainCurrent = false;
  CHECK(exposed.Accept(3, 1, empty), "a new source publication rechecks terrain dependencies");
  (void)water.Ingest(ground, exposed, materials);
  CHECK(water.OfTile(1).empty(), "old east water is withheld when its terrain proof is stale");
}
}

int main() {
  const std::array<std::string, 2> layers{"water_polygons", "water_lines"};
  OsmField field(2, layers);
  const auto tile = ReadTile("test/outshine/data/osm/water-order.mvt");
  const auto empty = ReadTile("test/outshine/data/osm/water-empty.mvt");
  CHECK(field.Accept(1, 1, tile) && field.Accept(2, 1, tile) && field.Accept(3, 1, empty),
        "fixed source arrays include two water tiles and an empty neighbor");
  const auto ordered = Snapshot(field, false);
  const auto delayed = Snapshot(field, true);
  CHECK(ordered.EncodeNative(kEncodedBudget) == delayed.EncodeNative(kEncodedBudget),
        "identical source values produce identical native bytes despite different DEM arrival");
  CheckOverlappingWindows(layers, tile, empty, ordered);
  return Report();
}

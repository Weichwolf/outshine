#include "UrbanAreaCodec.h"
#include "Check.h"
#include "Sha256.h"
#include "AssetGeneration.h"
#include <DataKind.h>
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <string>
#include <system_error>
#include <vector>
#include <expected>
#include <utility>

namespace {
using namespace outshine;
using namespace outshine::Test;
namespace Osm = outshine::Generators::Osm;

Osm::UrbanArea Example() {
  Osm::UrbanArea area;
  area.FirstCell = {-200, 300};
  area.ProjectedOriginM = {100, 200};
  area.MetresPerProjectedMetre = .8;
  area.Columns = 2;
  area.Rows = 2;
  area.Tiers = {1, 0, 4, 2};
  area.Occupied = 3;
  area.LotTests = 42;
  area.PatchTests = 8;
  area.HasUrbanEvidence = true;
  area.Inputs.push_back({.Identity = {.Kind = Data::DataKind::VectorMap,
                                      .Tile = {.Zoom = 11, .X = 1000, .Y = 800},
                                      .SourceId = "vector-provider",
                                      .Revision = "source-version"},
                         .PayloadDigest = "source-payload-digest"});
  area.Sources.push_back({.Input = 0,
                          .Feature = 17,
                          .Role = Osm::UrbanSourceRole::Urban,
                          .ProviderFeatureId = 7654321});
  return area;
}

void Same(const Osm::UrbanArea &area) {
  CHECK((area.FirstCell == std::array<int64_t, 2>{-200, 300}) &&
            (area.ProjectedOriginM == std::array<double, 2>{100, 200}),
        "global cell keys and metric origin survive");
  CHECK(area.Columns == 2 && area.Rows == 2 && area.Tiers == std::vector<uint8_t>({1, 0, 4, 2}),
        "ready categorical field preserves holes and exact heights");
  CHECK(area.Inputs.size() == 1 && area.Inputs[0].Identity.SourceId == "vector-provider" &&
            area.Inputs[0].Identity.Revision == "source-version" &&
            area.Inputs[0].PayloadDigest == "source-payload-digest",
        "original source identity and payload remain owned");
  CHECK(area.Sources.size() == 1 && area.Sources[0].Feature == 17 &&
            area.Sources[0].ProviderFeatureId == 7654321,
        "original feature identity survives cache storage");
  CHECK(area.Occupied == 3 && area.LotTests == 0 && area.PatchTests == 0,
        "read path reports no source coverage work while retaining occupied count");
}

void InvalidInputs() {
  auto area = Example();
  const auto encoded = Osm::EncodeUrbanArea(area);
  CHECK(encoded, "valid bounded field encodes");
  if (!encoded) { return; }
  const auto decoded = Osm::DecodeUrbanArea(*encoded);
  CHECK(decoded, "native format restores independently");
  if (decoded) {
    Same(*decoded);
    CHECK(Osm::EncodeUrbanArea(*decoded) == encoded,
          "source receipts and native values roundtrip byte exactly");
  }
  CHECK(!Osm::EncodeUrbanArea(area, 1) && !Osm::DecodeUrbanArea(*encoded, 1),
        "byte budgets apply in both directions");
  for (const size_t length : {size_t{0}, encoded->size() / 2, encoded->size() - 1}) {
    CHECK(!Osm::DecodeUrbanArea(std::span(*encoded).first(length)),
          "truncated packages are rejected");
  }
  auto damaged = *encoded;
  damaged[0] ^= 1;
  CHECK(!Osm::DecodeUrbanArea(damaged), "unknown format fails before consuming fields");
  damaged = *encoded;
  damaged.push_back(0);
  CHECK(!Osm::DecodeUrbanArea(damaged), "trailing bytes do not hide a second payload");
  area.Tiers[0] = 255;
  CHECK(!Osm::EncodeUrbanArea(area), "invalid categorical height is rejected");
  area = Example();
  area.Sources[0].Input = 1;
  CHECK(!Osm::EncodeUrbanArea(area), "source references stay inside the owned input table");
  area = Example();
  area.Inputs[0].Identity.Tile.Zoom = -1;
  CHECK(!Osm::EncodeUrbanArea(area), "negative source zoom is rejected before a shift");
  area = Example();
  area.Recipe.FillFraction = std::numeric_limits<double>::quiet_NaN();
  CHECK(!Osm::EncodeUrbanArea(area), "nonfinite recipe cannot enter the cache");
}

void CacheMissThenRead() {
  const auto path = PlantedPath("urban-area-assets.sqlite");
  const auto request = Sha256Hex("urban-area-demand-with-version-and-parameters");
  std::error_code error;
  std::filesystem::remove(path, error);
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "native asset cache opens");
    if (!cache) { return; }
    size_t cooks = 0;
    const AssetFactory produce =
        [&](size_t budget) -> std::expected<GeneratedAssetPackage, std::string> {
      ++cooks;
      auto encoded = Osm::EncodeUrbanArea(Example(), budget);
      if (!encoded) { return std::unexpected(encoded.error()); }
      const auto key = Sha256Hex(encoded->data(), encoded->size());
      return GeneratedAssetPackage{
          .Records = {{.Key = key,
                       .Kind = "urban-area",
                       .Bounds = {.Min = {{0, 0, 0}}, .Max = {{64, 48, 64}}},
                       .Package = {},
                       .ByteCount = encoded->size(),
                       .Parent = {},
                       .RequestKey = request}},
          .Bytes = std::move(*encoded)};
    };
    const auto cold = ResolveAssetRequest(**cache, request, Osm::kUrbanAreaBytesMost, produce);
    CHECK(cold && cooks == 1, "cache miss generates and publishes the complete field once");
  }
  {
    auto cache = AssetCache::Open(path);
    CHECK(cache, "fresh asset connection opens");
    if (!cache) { return; }
    const auto hit = ResolveAssetRequest(**cache, request, Osm::kUrbanAreaBytesMost, {});
    CHECK(hit, "warm request resolves with no source or generator available");
    if (hit) {
      const auto loaded = Osm::DecodeUrbanArea(hit->Bytes());
      CHECK(loaded, "cached field restores without source geometry work");
      if (loaded) { Same(*loaded); }
    }
  }
  std::filesystem::remove(path, error);
}
}

int main() {
  InvalidInputs();
  CacheMissThenRead();
  return Report();
}

#include "OsmField.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <vector>

namespace {
constexpr uint8_t kLayerVersion = 0x78;
constexpr uint8_t kLayerExtent = 0x28;
constexpr uint8_t kLayerKey = 0x1a;
constexpr uint8_t kLayerValue = 0x22;
constexpr uint8_t kFeatureType = 0x18;
constexpr uint8_t kFeatureTags = 0x12;
constexpr uint8_t kPolygonGeometry = 0x22;
constexpr uint8_t kPolygonLineTo = 26;
constexpr uint8_t kHeightM = 19;
constexpr uint8_t kEncodedSide = 20;

outshine::Test::Mvt::Bytes Tile() {
  using outshine::Test::Mvt::Append;
  using outshine::Test::Mvt::Bytes;
  Bytes layer{0x0a, 8, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g', kLayerVersion, 2, kLayerExtent, 64};
  Append(layer, kLayerKey, Bytes{'r', 'e', 'n', 'd', 'e', 'r', '_', 'h', 'e', 'i', 'g', 'h', 't'});
  Append(layer, kLayerKey, Bytes{'n', 'a', 'm', 'e'});
  Append(layer, kLayerValue, Bytes{kLayerExtent, kHeightM});
  Append(layer, kLayerValue, Bytes{0x0a, 5, 'h', 'o', 'u', 's', 'e'});
  Bytes feature{0x08, 7, kFeatureType, 3};
  Append(feature, kFeatureTags, Bytes{0, 0, 1, 1});
  Append(feature,
         kPolygonGeometry,
         Bytes{9, 0, 0, kPolygonLineTo, kEncodedSide, 0, 0, kEncodedSide, kHeightM, 0, 15});
  Append(layer, kFeatureTags, feature);
  Bytes tile;
  Append(tile, kLayerKey, layer);
  return tile;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  constexpr size_t most = size_t{1024} * 1024;
  const std::array<std::string, 2> layers{"buildings", "water_polygons"};
  OsmField cold(2, layers, MvtSchema::OpenMapTiles);
  CHECK(cold.Accept(1, 1, Tile()), "real MVT publishes one normalized native tile");
  const auto encoded = cold.EncodeNativeTile(most);
  CHECK(encoded && !encoded->empty(), "complete semantic assets have a bounded native encoding");
  if (!encoded) { return Report(); }
  const auto warm = OsmField::DecodeNativeTile(*encoded, most);
  CHECK(warm && warm->Features().size() == 1 && warm->Tiles().size() == 1,
        "native decode restores complete geometry and tile admission");
  if (!warm) { return Report(); }
  const auto &source = cold.Tiles().front();
  const auto &stored = warm->Tiles().front();
  CHECK(source.Source == stored.Source && source.InputDigest == stored.InputDigest &&
            source.FirstFeature == stored.FirstFeature &&
            source.FeatureCount == stored.FeatureCount,
        "source identities, content binding and native ranges survive");
  CHECK(std::ranges::equal(cold.Points(), warm->Points()) &&
            cold.Rings().size() == warm->Rings().size() && cold.Extent() == warm->Extent(),
        "geographic coordinates are restored without projection or MVT parsing");
  const auto &feature = warm->Features().front();
  CHECK(feature.ProviderFeatureId == 7 && feature.Layer == 0 &&
            warm->Num(feature, "height", 0) == kHeightM && warm->Str(feature, "name") == "house",
        "provider identity and normalized numeric/string tags are ready for all consumers");
  CHECK(feature.HasContentDigest && cold.Features().front().HasContentDigest &&
            feature.ContentDigest == cold.Features().front().ContentDigest,
        "the source message digest survives a native hit without hashing or projecting again");
  CHECK(warm->Schema() == cold.Schema() && warm->MissingLayers() == cold.MissingLayers() &&
            warm->MissingLayers() == 1 && warm->SettledWithin(0),
        "schema, genuine missing layers and readiness remain explicit");
  CHECK(warm->TotalBuildMetrics().ParseMs == 0 && warm->TotalBuildMetrics().PublicationMs == 0,
        "native decode does not claim source preparation");
  const auto again = warm->EncodeNativeTile(most);
  CHECK(again == encoded, "canonical native bytes roundtrip exactly");
  CHECK(!cold.EncodeNativeTile(1) && !OsmField::DecodeNativeTile(*encoded, 1),
        "encoded and decoded allocations obey caller budgets");
  for (size_t length = 0; length < encoded->size(); ++length) {
    CHECK(!OsmField::DecodeNativeTile(std::span(*encoded).first(length), most),
          "every truncated native package fails without publishing a partial product");
  }
  auto invalid = *encoded;
  invalid.push_back(0);
  CHECK(!OsmField::DecodeNativeTile(invalid, most), "unclaimed trailing bytes are rejected");
  invalid = *encoded;
  invalid.front() ^= 1;
  CHECK(!OsmField::DecodeNativeTile(invalid, most), "unknown native recipes are rejected");
  invalid = *encoded;
  std::fill(invalid.begin() + 4, invalid.begin() + 8, std::numeric_limits<uint8_t>::max());
  CHECK(!OsmField::DecodeNativeTile(invalid, most), "invalid geographic hierarchy is rejected");
  invalid = *encoded;
  constexpr size_t kFeatureWireBytes = 97;
  std::fill(invalid.end() - kFeatureWireBytes,
            invalid.end() - kFeatureWireBytes + sizeof(uint32_t),
            std::numeric_limits<uint8_t>::max());
  CHECK(!OsmField::DecodeNativeTile(invalid, most), "out-of-range feature rings are rejected");
  invalid = *encoded;
  std::fill(
      invalid.end() - 32, invalid.end() - sizeof(double) * 3, std::numeric_limits<uint8_t>::max());
  CHECK(!OsmField::DecodeNativeTile(invalid, most), "nonfinite feature bounds are rejected");
  cold.Declare({}, Ground::TileAt{.X = 1, .Y = 1});
  CHECK(!cold.EncodeNativeTile(most),
        "unbound declarations cannot masquerade as cached source assets");
  return Report();
}

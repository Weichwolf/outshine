#include "Check.h"
#include "ClippedWaterFixture.h"
#include <array>
#include <cstdint>
#include <limits>
#include <string>
#include <string_view>

namespace {
using namespace outshine::Test::Water;

Bytes Layer(bool polygon) {
  Bytes geometry;
  if (polygon) {
    Point previous{};
    Ring(geometry, Rectangle(10, 10, 15, 15), previous);
    Ring(geometry, Rectangle(20, 20, 25, 25), previous);
  } else {
    geometry = {9, 20, 20, 10, 20, 0};
  }
  Bytes feature{8};
  Varint(feature, polygon ? std::numeric_limits<uint64_t>::max() : (uint64_t{1} << 53) + 1);
  Field(feature, 0x12, Bytes{0, 0, 1, 1, 2, 2, 3, 3});
  feature.insert(feature.end(), {0x18, static_cast<uint8_t>(polygon ? 3 : 2)});
  Field(feature, 0x22, geometry);
  Bytes layer;
  Text(layer, 0x0a, polygon ? "water" : "waterway");
  Field(layer, 0x12, feature);
  for (const auto key : {"class", "brunnel", "intermittent", "tunnel"}) { Text(layer, 0x1a, key); }
  const std::array<std::string_view, 4> values{
      polygon ? "river" : "stream", polygon ? "bridge" : "tunnel", polygon ? "yes" : "no", "yes"};
  for (const auto text : values) {
    Bytes value;
    Text(value, 0x0a, text);
    Field(layer, 0x22, value);
  }
  layer.insert(layer.end(), {0x28, 64, 0x78, 2});
  return layer;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  const std::array<std::string, 2> layers{"water_polygons", "water_lines"};
  OsmField field(2, layers, MvtSchema::OpenMapTiles);
  Bytes tile;
  Field(tile, 0x1a, Layer(true));
  Field(tile, 0x1a, Layer(false));
  CHECK(field.Accept(1, 1, tile), "real vector water tile accepted");
  WaterField water;
  Finish(water, field);
  CHECK(water.Ingested(field) && water.Surfaces().size() == 2 && water.Courses().size() == 1,
        "one polygon input can supply several surfaces alongside a separate flowing input");
  const auto cold = water.Asset(field);
  const auto encoded = cold.EncodeNative(8192);
  CHECK(encoded, "source-bound water is cacheable");
  if (!encoded) { return Report(); }
  const auto warm = Generators::WaterAsset::DecodeNative(*encoded);
  CHECK(warm && warm->EncodeNative(8192) == encoded, "native metadata roundtrip is canonical");
  if (!warm) { return Report(); }
  CHECK(warm->Sources().size() == 1 && warm->Inputs().size() == 2,
        "tile provenance is shared and one source feature is not copied per polygon");
  if (warm->Sources().size() != 1 || warm->Inputs().size() != 2 || warm->Surfaces().size() != 2 ||
      warm->Courses().size() != 1) {
    return Report();
  }
  const auto *surface = warm->InputOf(warm->Surfaces().front());
  const auto *course = warm->InputOf(warm->Courses().front());
  CHECK(surface && course, "both geometry classes retain their source binding");
  if (!surface || !course) { return Report(); }
  CHECK(surface == warm->InputOf(warm->Surfaces().back()) && surface->Source == 0 &&
            surface->Feature == 0 && course->Source == 0 && course->Feature == 1,
        "multipolygon pieces share exact tile-local source indices");
  CHECK(surface->FeatureId == std::numeric_limits<uint64_t>::max() &&
            course->FeatureId == (uint64_t{1} << 53) + 1,
        "provider IDs preserve all 64 bits rather than passing through floating point");
  CHECK(surface->Layer == "water_polygons" && surface->Kind == "river" &&
            surface->Crossing == "bridge" && surface->Bridge && surface->Tunnel &&
            surface->Intermittent == true,
        "normalized classification, raw crossing and conflicting flags all survive");
  CHECK(course->Layer == "water_lines" && course->Kind == "stream" &&
            course->Crossing == "tunnel" && !course->Bridge && course->Tunnel &&
            course->Intermittent == false,
        "explicit permanent tunnel water remains distinct from unknown intermittency");
  CHECK(warm->Sources().front().Identity == field.Tiles().front().Source &&
            warm->Sources().front().InputDigest == field.Tiles().front().InputDigest &&
            warm->Sources().front().FeatureCount == 2,
        "complete source identity and input binding survive without per-feature revision copies");
  field.Declare({}, Ground::TileAt{.X = 1, .Y = 1});
  CHECK(surface->Kind == "river" && warm->Sources().front().FeatureCount == 2 &&
            warm->EncodeNative(8192) == encoded,
        "cache metadata owns its strings independently of the mutable field");
  CHECK(!warm->InputOf(Generators::WaterAsset::Surface{}) &&
            !warm->InputOf(Generators::WaterAsset::Course{}),
        "source-free procedural geometry has no invented source input");
  return Report();
}

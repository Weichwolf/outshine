#include "WaterAsset.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Generators::WaterAsset;
  WaterAsset::Data data{
      .Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = 10, .Input = 0}},
      .Rings = {{.FirstPoint = 0, .PointCount = 3}},
      .Courses = {{.FirstPoint = 0, .PointCount = 2, .FirstLevel = 0, .HalfWidthM = 1, .Input = 0}},
      .Levels = {10, 9},
      .Points = {0, 0, 0, 1, 1, 1},
      .Tiles = {{.Zoom = 0, .X = 0, .Y = 0, .FirstSurface = 0, .SurfaceCount = 1}},
      .ProcessedTiles = 1,
      .Sources = {{.Identity = {.From = Data::TileSourceIdentity::Origin::Shaped,
                                .Kind = Data::DataKind::VectorMap,
                                .Tile = {.Zoom = 0, .X = 0, .Y = 0},
                                .NativeCell = Data::CellId{.SouthDeg = -89, .WestDeg = 179},
                                .SourceId = "provider",
                                .Revision = std::string(200, 'r')},
                   .InputDigest = "opaque-versioned-input",
                   .FeatureCount = 1}},
      .Inputs = {{.Source = 0,
                  .Feature = 0,
                  .FeatureId = std::numeric_limits<uint64_t>::max(),
                  .Layer = "water",
                  .Kind = "future-water-class",
                  .Crossing = "future-crossing",
                  .Bridge = true,
                  .Tunnel = true,
                  .Intermittent = false}}};
  const auto bytes = WaterAsset(data).EncodeNative(4096);
  CHECK(bytes, "complete source identity and unknown future labels can be encoded");
  if (!bytes) { return Report(); }
  const auto warm = WaterAsset::DecodeNative(*bytes);
  CHECK(warm && warm->EncodeNative(4096) == bytes,
        "all optional and opaque metadata survives canonical native replay");
  if (!warm || warm->Sources().empty() || warm->Inputs().empty()) { return Report(); }
  CHECK(warm->Sources().front().Identity == data.Sources.front().Identity &&
            warm->Sources().front().InputDigest == data.Sources.front().InputDigest &&
            warm->Inputs().front().FeatureId == data.Inputs.front().FeatureId &&
            warm->Inputs().front().Kind == data.Inputs.front().Kind &&
            warm->Inputs().front().Crossing == data.Inputs.front().Crossing &&
            warm->Inputs().front().Bridge && warm->Inputs().front().Tunnel &&
            warm->Inputs().front().Intermittent == false,
        "provenance, exact IDs, unknown classes and contradictory flags remain explicit");
  for (size_t length = 0; length < bytes->size(); ++length) {
    CHECK(!WaterAsset::DecodeNative(std::span(*bytes).first(length)),
          "every truncated identity, string and optional field is rejected");
  }
  const auto reject = [&](auto mutate) {
    auto invalid = data;
    mutate(invalid);
    CHECK(!WaterAsset(std::move(invalid)).EncodeNative(4096),
          "dangling or invalid source metadata cannot be published");
  };
  reject([](auto &value) { value.Surfaces.front().Input = 1; });
  reject([](auto &value) { value.Courses.front().Input = 1; });
  reject([](auto &value) { value.Inputs.front().Source = 1; });
  reject([](auto &value) { value.Inputs.front().Feature = 1; });
  reject([](auto &value) { value.Sources.front().FeatureCount = 0; });
  reject([](auto &value) { value.Sources.clear(); });
  reject([](auto &value) { value.Sources.push_back(value.Sources.front()); });
  reject([](auto &value) { value.Sources.front().Identity.Tile.Zoom = 31; });
  reject([](auto &value) { value.Sources.front().Identity.Tile.X = 1; });
  reject([](auto &value) { value.Sources.front().Identity.NativeCell->SouthDeg = 90; });
  reject([](auto &value) { value.Sources.front().Identity.NativeCell->WestDeg = -181; });
  reject([](auto &value) {
    value.Sources.front().Identity.From = static_cast<Data::TileSourceIdentity::Origin>(4);
  });
  reject([](auto &value) { value.Sources.front().Identity.Kind = static_cast<Data::DataKind>(5); });
  data.Sources.front().Identity.NativeCell.reset();
  data.Sources.front().Identity.SourceId.clear();
  data.Sources.front().Identity.Revision = {};
  data.Sources.front().InputDigest.clear();
  data.Inputs.front() = {};
  const auto minimal = WaterAsset(data).EncodeNative(4096);
  CHECK(minimal, "empty metadata labels still have a complete bounded encoding");
  if (!minimal) { return Report(); }
  const auto unknown = WaterAsset::DecodeNative(*minimal);
  CHECK(unknown && !unknown->Inputs().front().FeatureId &&
            !unknown->Inputs().front().Intermittent &&
            !unknown->Sources().front().Identity.NativeCell,
        "minimum-sized metadata records retain unknown values without fabricated defaults");
  return Report();
}

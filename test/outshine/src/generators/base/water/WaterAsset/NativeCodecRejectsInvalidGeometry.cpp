#include "WaterAsset.h"
#include "Check.h"
#include <limits>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  using Generators::WaterAsset;
  const WaterAsset::Data valid{
      .Surfaces = {{.FirstRing = 0, .RingCount = 1, .LevelM = -10}},
      .Rings = {{.FirstPoint = 0, .PointCount = 4}},
      .Courses = {{.FirstPoint = 0, .PointCount = 2, .FirstLevel = 0, .HalfWidthM = 1}},
      .Levels = {-10, -11},
      .Points = {0, 0, 0, 1, 1, 1, 1, 0},
      .Tiles = {{.Zoom = 6, .X = 32, .Y = 32, .FirstSurface = 0, .SurfaceCount = 1}},
      .ProcessedTiles = 1};
  const auto bytes = WaterAsset(valid).EncodeNative(4096);
  CHECK(bytes && WaterAsset::DecodeNative(*bytes),
        "complete geometry has a source-free native roundtrip");
  const auto reject = [&](auto mutate) {
    auto data = valid;
    mutate(data);
    CHECK(!WaterAsset(std::move(data)).EncodeNative(4096),
          "invalid native geometry never becomes a ready package");
  };
  reject([](auto &data) { data.Status = WaterAsset::Coverage::Partial; });
  reject([](auto &data) { data.Points.pop_back(); });
  reject([](auto &data) { data.Points[0] = std::numeric_limits<double>::quiet_NaN(); });
  reject([](auto &data) { data.Levels[0] = std::numeric_limits<float>::infinity(); });
  reject([](auto &data) { data.Surfaces[0].LevelM = std::numeric_limits<float>::infinity(); });
  reject([](auto &data) { data.Surfaces[0].FirstRing = 1; });
  reject([](auto &data) { data.Rings[0].FirstPoint = 1; });
  reject([](auto &data) { data.Rings[0].PointCount = 2; });
  reject([](auto &data) { data.Courses[0].FirstLevel = 1; });
  reject([](auto &data) { data.Courses[0].HalfWidthM = 0; });
  reject([](auto &data) { data.Tiles[0].Zoom = -1; });
  reject([](auto &data) { data.Tiles[0].X = 64; });
  reject([](auto &data) { data.Tiles[0].SurfaceCount = 2; });
  reject([](auto &data) { data.ProcessedTiles = 2; });
  reject([](auto &data) { data.NoGround = -1; });
  const auto empty = WaterAsset{}.EncodeNative(4096);
  CHECK(empty && WaterAsset::DecodeNative(*empty),
        "a completely empty scene needs neither sources nor generator progress");
  return Report();
}

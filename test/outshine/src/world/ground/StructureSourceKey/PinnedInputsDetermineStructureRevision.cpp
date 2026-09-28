#include "StructureSourceKey.h"
#include "Check.h"

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <utility>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const Data::TileSourceIdentity vector{.From = Data::TileSourceIdentity::Origin::Provider,
                                        .Kind = Data::DataKind::VectorMap,
                                        .Tile = {.Zoom = 14, .X = 8612, .Y = 5560},
                                        .SourceId = "vector",
                                        .Revision = "one"};
  const Data::TileSourceIdentity height{.From = Data::TileSourceIdentity::Origin::Provider,
                                        .Kind = Data::DataKind::Elevation,
                                        .Tile = {.Zoom = 15, .X = 17224, .Y = 11120},
                                        .SourceId = "dem",
                                        .Revision = "one"};
  const std::array heights{height};
  const auto key = [&](std::optional<Data::TileSourceIdentity> vectorSource,
                       std::span<const Data::TileSourceIdentity> heightSources,
                       uint64_t raster,
                       uint64_t street,
                       double spanM,
                       bool fallback) {
    return StructureSourceKey({.Vector = std::move(vectorSource),
                               .HeightSources = heightSources,
                               .HeightDigest = raster,
                               .StreetDigest = street,
                               .TileSpanM = spanM,
                               .FallbackHeights = fallback});
  };
  const uint64_t baseline = key(vector, heights, 7, 11, 2400, false);
  CHECK(baseline != 0 && baseline == key(vector, heights, 7, 11, 2400, false),
        "equal pinned source snapshots produce one reusable key");
  auto secondHeight = height;
  secondHeight.Tile.X += 1;
  const std::array ordered{height, secondHeight};
  const std::array reversed{secondHeight, height};
  const std::array repeated{height, secondHeight, height};
  CHECK(key(vector, ordered, 7, 11, 2400, false) == key(vector, reversed, 7, 11, 2400, false) &&
            key(vector, ordered, 7, 11, 2400, false) == key(vector, repeated, 7, 11, 2400, false),
        "source arrival order and duplicate delivery do not change geometry identity");
  auto nextVector = vector;
  nextVector.Revision = "two";
  CHECK(key(nextVector, heights, 7, 11, 2400, false) != baseline,
        "vector revision invalidates resident variants");
  auto nextHeight = height;
  nextHeight.Revision = "two";
  CHECK(key(vector, std::array{nextHeight}, 7, 11, 2400, false) != baseline,
        "DEM source revision invalidates resident variants even with equal raster samples");
  CHECK(key(vector, heights, 8, 11, 2400, false) != baseline,
        "changed height samples invalidate resident variants");
  CHECK(key(vector, heights, 7, 12, 2400, false) != baseline,
        "changed street corridor invalidates resident variants");
  CHECK(key(vector, heights, 7, 11, 2500, false) != baseline,
        "changed spatial scale invalidates massing");
  CHECK(key(vector, heights, 7, 11, 2400, true) != baseline,
        "fallback height provenance cannot masquerade as qualified input");
  CHECK(key(std::nullopt, heights, 7, 11, 2400, false) != baseline,
        "missing vector identity cannot reuse a keyed vector product");
  return Report();
}

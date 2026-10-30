#include "MvtLayer.h"
#include "MvtWireFixture.h"
#include "Check.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <span>
#include <vector>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

Bytes Tile(Bytes header, std::span<const uint8_t> tags) {
  Bytes feature{0x18, 2, 0x22, 6, 9, 1, 0, 10, 4, 2};
  Append(feature, 0x12, tags);
  Append(header, 0x12, feature);
  Append(header, 0x1a, Bytes{'k'});
  Append(header, 0x22, Bytes{0x0a, 1, 'v'});
  Bytes tile;
  Append(tile, 0x1a, header);
  return tile;
}

void InterleavedTablesPreserveOrder() {
  using namespace outshine;
  using namespace outshine::Test;
  Bytes first{0x08, 7, 0x18, 2, 0x22, 6, 9, 2, 0, 10, 2, 0};
  Bytes second{0x08, 3, 0x18, 2, 0x22, 6, 9, 6, 0, 10, 2, 0};
  Append(first, 0x12, Bytes{0, 0});
  Append(second, 0x12, Bytes{0, 0});
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  Append(layer, 0x12, first);
  Append(layer, 0x1a, Bytes{'k'});
  layer.insert(layer.end(), {0x48, 99});
  Append(layer, 0x12, second);
  Append(layer, 0x22, Bytes{0x0a, 1, 'v'});
  Bytes input;
  Append(input, 0x1a, layer);
  Generators::Osm::MvtLayer decoded;
  CHECK(decoded.Parse(input, "x") && decoded.Features().size() == 2,
        "features on either side of a table and an unknown field decode together");
  if (decoded.Features().size() != 2) { return; }
  const auto &a = decoded.Features()[0];
  const auto &b = decoded.Features()[1];
  CHECK(a.ProviderFeatureId == uint64_t{7} && b.ProviderFeatureId == uint64_t{3},
        "source order is preserved independently of provider ID order");
  CHECK(a.ContentDigest == Sha256Digest(first.data(), first.size()) &&
            b.ContentDigest == Sha256Digest(second.data(), second.size()),
        "source digests cover the exact original feature messages");
  CHECK(decoded.StringTag(a, "k") == "v" && decoded.StringTag(b, "k") == "v",
        "both features resolve their later value table");
  CHECK(std::ranges::equal(decoded.Points(), std::array<int32_t, 8>{1, 0, 2, 0, 3, 0, 4, 0}),
        "geometry follows original message order across intervening tables");
}
}

int main() {
  InterleavedTablesPreserveOrder();
  using namespace outshine::Test;
  const Bytes valid{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  for (const auto &header :
       std::vector<Bytes>{{0x0a, 1, 'x', 0x28, 64},
                          {0x0a, 1, 'x', 0x78, 2},
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 0},
                          {0x0a, 1, 'x', 0x78, 3, 0x28, 64},
                          {0x0a, 1, 'x', 0x78, 0, 0x28, 64},
                          {0x0a, 1, 'x', 0x78, 0x82, 0x80, 0x80, 0x80, 0x10, 0x28, 64},
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 0x80, 0x80, 0x80, 0x80, 8},
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 0xc0, 0x80, 0x80, 0x80, 0x10}}) {
    const auto tile = Tile(header, Bytes{0, 0});
    ::outshine::Generators::Osm::MvtLayer decoded;
    CHECK(!decoded.Parse(tile, "x"), "invalid layer header rejected");
  }
  for (const auto &tags : std::vector<Bytes>{{0}, {0, 0, 0}, {1, 0}, {0, 1}}) {
    const auto tile = Tile(valid, tags);
    ::outshine::Generators::Osm::MvtLayer decoded;
    CHECK(!decoded.Parse(tile, "x"), "invalid dictionary pair rejected");
  }
  for (const auto &header :
       std::vector<Bytes>{valid,
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 1},
                          {0x0a, 1, 'x', 0x78, 2, 0x28, 0xff, 0xff, 0xff, 0xff, 7}}) {
    const auto tile = Tile(header, Bytes{0, 0});
    ::outshine::Generators::Osm::MvtLayer decoded;
    CHECK(decoded.Parse(tile, "x").has_value(), "valid extent and later dictionaries accepted");
    CHECK(decoded.Features().size() == 1 && decoded.Points().size() == 4 &&
              decoded.Points()[0] == -1 && decoded.Points()[2] == 1,
          "buffered coordinates remain legal outside extent");
    if (!decoded.Features().empty()) {
      CHECK(decoded.StringTag(decoded.Features().front(), "k") == "v",
            "valid tag resolves later dictionary");
    }
  }
  return Report();
}

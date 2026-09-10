#include "OsmVector.h"
#include "Check.h"
#include <array>
#include <algorithm>
#include <memory>
#include <cmath>
#include <cstdint>
#include <span>
#include <string_view>
#include <vector>

namespace {
using Bytes = std::vector<uint8_t>;

void Append(Bytes &out, uint8_t tag, std::span<const uint8_t> bytes) {
  // Fixture messages fit in a one-byte protobuf length.
  out.push_back(tag);
  out.push_back(static_cast<uint8_t>(bytes.size()));
  out.insert(out.end(), bytes.begin(), bytes.end());
}

Bytes Tile(std::span<const uint8_t> value) {
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 0x80, 0x20};
  const std::array<uint8_t, 5> key{'v', 'a', 'l', 'u', 'e'};
  Append(layer, 0x1a, key);
  const std::array<uint8_t, 14> feature{0x12, 2, 0, 0, 0x18, 2, 0x22, 6, 9, 0, 0, 10, 2, 2};
  Append(layer, 0x12, feature);
  Append(layer, 0x22, value);
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;

  struct Numeric {
    Bytes Wire;
    double Expected;
  };

  const std::array<Numeric, 9> cases{
      {{{0x15, 0, 0, 0xc0, 0xbf}, -1.5},
       {{0x19, 0, 0, 0, 0, 0, 0, 2, 0xc0}, -2.25},
       {{0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 1}, -1},
       {{0x20, 0x80, 0x80, 0x80, 0x80, 0x10}, 4294967296.0},
       {{0x28, 0x80, 0x80, 0x80, 0x80, 0x10}, 4294967296.0},
       {{0x30, 0xff, 0xff, 0xff, 0xff, 0xff, 0x3f}, -1099511627776.0},
       {{0x30, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 1}, -std::ldexp(1.0, 63)},
       {{0x38, 0}, 0},
       {{0x38, 1}, 1}}};
  for (const auto &one : cases) {
    const auto tile = Tile(one.Wire);
    OsmVector decoded;
    CHECK(decoded.Parse(tile.data(), tile.size(), "x") && decoded.Features().size() == 1,
          "typed numeric fixture parses");
    if (decoded.Features().empty()) { continue; }
    const auto tag = decoded.TagAt(decoded.Features().front(), 0);
    CHECK(tag.Key == "value" && tag.IsNum && tag.Num == one.Expected,
          "wire signedness, width and byte order match analytical value");
  }
  for (const auto &one : {cases[0], cases[1]}) {
    for (size_t length = 1; length < one.Wire.size(); ++length) {
      const auto tile = Tile(std::span(one.Wire).first(length));
      auto exact = std::make_unique<uint8_t[]>(tile.size());
      std::ranges::copy(tile, exact.get());
      OsmVector decoded;
      bool present = false;
      CHECK(!decoded.Parse(exact.get(), tile.size(), "x", &present) && present,
            "every truncated fixed-width payload refuses the present layer");
    }
  }
  for (const Bytes &bad :
       std::vector<Bytes>{{},
                          {0x40, 1},
                          {0x20, 0x80},
                          {0x20, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 2},
                          {0x38, 1, 0x0a, 1, 'x'},
                          {0x38, 1, 0x40, 0x80},
                          {0x0a, 5, 'x'}}) {
    const auto tile = Tile(bad);
    OsmVector decoded;
    CHECK(!decoded.Parse(tile.data(), tile.size(), "x"), "malformed or untyped value is refused");
  }
  const auto stringTile = Tile(Bytes{0x0a, 4, 'r', 'o', 'c', 'k', 0x40, 1});
  OsmVector decoded;
  CHECK(decoded.Parse(stringTile.data(), stringTile.size(), "x"),
        "string plus unknown extension is valid");
  if (!decoded.Features().empty()) {
    CHECK(decoded.Str(decoded.Features().front(), "value") == "rock",
          "string bytes survive unknown extension");
  }
  const auto duplicate = Tile(Bytes{0x38, 0, 0x38, 1});
  CHECK(decoded.Parse(duplicate.data(), duplicate.size(), "x"),
        "protobuf last-one-wins for repeated singular field");
  if (!decoded.Features().empty()) {
    CHECK(decoded.Num(decoded.Features().front(), "value", -1) == 1,
          "last occurrence of same value type wins");
  }
  return Report();
}

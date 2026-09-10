#include "OsmVector.h"
#include "WireFixture.h"
#include "Check.h"
#include <algorithm>
#include <array>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

Bytes Tile(char name, uint8_t x, bool broken = false) {
  Bytes layer{0x0a, 1, static_cast<uint8_t>(name), 0x78, 2, 0x28, 64};
  const Bytes feature{0x18, 2, 0x22, 6, 9, x, 0, 10, 2, 2};
  Append(layer, 0x12, feature);
  if (broken) { Append(layer, 0x12, Bytes{0x18, 2, 0x22, 1, 9}); }
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  OsmVector decoded;
  const auto initial = Tile('x', 4);
  CHECK(decoded.Parse(initial, "x").has_value(), "initial layer is valid");
  const auto *points = decoded.Points().data();
  const auto *features = decoded.Features().data();
  const auto *rings = decoded.Rings().data();
  auto trailing = Tile('x', 8);
  trailing.push_back(0x80);
  auto duplicate = Tile('x', 8);
  duplicate.insert(duplicate.end(), initial.begin(), initial.end());
  for (const auto &bad : std::vector<Bytes>{
           Bytes{}, Bytes{0x1a, 10, 0}, Tile('x', 8, true), Tile('y', 8), trailing, duplicate}) {
    CHECK(!decoded.Parse(bad, "x"), "unsuccessful replacement rejected");
    CHECK(decoded.Points().data() == points && decoded.Features().data() == features &&
              decoded.Rings().data() == rings,
          "failure preserves existing views");
    CHECK(decoded.Extent() == 64 && decoded.Features().size() == 1 && decoded.Rings().size() == 1 &&
              std::ranges::equal(decoded.Points(), std::array<int32_t, 4>{2, 0, 3, 1}),
          "failure preserves published content");
  }
  auto replacement = Tile('x', 10);
  const auto unrelated = Tile('y', 12);
  replacement.insert(replacement.end(), unrelated.begin(), unrelated.end());
  CHECK(decoded.Parse(replacement, "x") &&
            std::ranges::equal(decoded.Points(), std::array<int32_t, 4>{5, 0, 6, 1}),
        "successful replacement publishes new owned data with unrelated following layer");
  using Error = OsmVector::ParseError;

  struct Case {
    Bytes Input;
    Error Expected;
  };

  auto unknown = initial;
  unknown[6] = 3;
  auto unrelatedBroken = Tile('y', 4);
  unrelatedBroken.push_back(0x80);
  for (const auto &test : std::vector<Case>{{{}, Error::MissingLayer},
                                            {Tile('y', 4), Error::MissingLayer},
                                            {{0x1a, 10, 0}, Error::InvalidTile},
                                            {Tile('x', 4, true), Error::InvalidTile},
                                            {unrelatedBroken, Error::InvalidTile},
                                            {unknown, Error::UnsupportedVersion}}) {
    const auto result = decoded.Parse(test.Input, "x");
    CHECK(!result && result.error() == test.Expected,
          "missing layer, malformed input and unsupported version are distinct outcomes");
  }
  return Report();
}

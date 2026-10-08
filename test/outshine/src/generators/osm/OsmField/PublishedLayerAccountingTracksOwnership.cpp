#include "Check.h"
#include "MvtLayer.h"
#include "OsmField.h"
#include "test/outshine/src/generators/osm/MvtLayer/WireFixture.h"

#include <array>
#include <cstddef>
#include <string>

namespace {
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
using Mvt::Append;
using Mvt::Bytes;

Bytes Tile(size_t tagLength) {
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  Append(layer, 0x12, Bytes{0x18, 2, 0x22, 6, 9, 0, 0, 10, 2, 2});
  const Bytes key(tagLength, 'k');
  Append(layer, 0x1a, key);
  Bytes value;
  Append(value, 0x0a, Bytes(tagLength, 'v'));
  Append(layer, 0x22, value);
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}

void CheckAccounting() {
  constexpr size_t longTag = 48;
  const std::array<std::string, 1> layers{"x"};
  const auto small = Tile(1);
  const auto large = Tile(longTag);
  MvtLayer parsedSmall, parsedLarge;
  OsmField ownedSmall(2, layers), ownedLarge(2, layers);
  const bool parsed =
      parsedSmall.Parse(small, "x").has_value() && parsedLarge.Parse(large, "x").has_value();
  const bool accepted =
      ownedSmall.Accept(1, 1, small).has_value() && ownedLarge.Accept(1, 1, large).has_value();
  CHECK(parsed && accepted, "different unused string tables have identical published geometry");
  if (!parsed || !accepted) { return; }
  const auto smallBytes = ownedSmall.HeapBytes();
  const auto largeBytes = ownedLarge.HeapBytes();
  CHECK(largeBytes > smallBytes &&
            largeBytes - smallBytes == parsedLarge.HeapBytes() - parsedSmall.HeapBytes(),
        "owned parser storage keeps the original capacity charge including unused tag strings");
  CHECK(ownedSmall.SnapshotQueries()->HeapBytes() == ownedLarge.SnapshotQueries()->HeapBytes(),
        "query snapshots do not charge parser dictionaries they do not own");
  constexpr size_t queries = 1000;
  bool stable = true;
  for (size_t at = 0; at < queries; ++at) {
    stable = stable && ownedLarge.HeapBytes() == largeBytes;
  }
  CHECK(stable, "repeated memory reporting never changes the charge or parsed data");
  const Bytes broken{0x1a, 1, 0};
  CHECK(!ownedLarge.Accept(1, 1, broken) && ownedLarge.HeapBytes() == largeBytes,
        "failed replacement preserves published layer accounting");
  CHECK(ownedLarge.Accept(1, 1, small) && ownedLarge.HeapBytes() == smallBytes,
        "successful replacement releases the old layer charge exactly");
  CHECK(ownedLarge.Accept(1, 1, large) && ownedLarge.HeapBytes() == largeBytes,
        "a later larger replacement publishes its own charge");
  ownedSmall.Declare({}, {.X = 1, .Y = 1});
  ownedLarge.Declare({}, {.X = 1, .Y = 1});
  CHECK(
      ownedSmall.HeapBytes() == ownedLarge.HeapBytes() && ownedLarge.HeapBytes() < largeBytes,
      "discarding parsed sources discards their cached charge while retaining container capacity");
}

void CheckReallocation() {
  const std::array<std::string, 1> layers{"x"};
  const auto small = Tile(1);
  const auto large = Tile(48);
  MvtLayer parsedSmall, parsedLarge;
  const bool parsed =
      parsedSmall.Parse(small, "x").has_value() && parsedLarge.Parse(large, "x").has_value();
  CHECK(parsed, "independent parser charges are available before growing tile storage");
  if (!parsed) { return; }
  const auto delta = parsedLarge.HeapBytes() - parsedSmall.HeapBytes();
  OsmField ownedSmall(2, layers), ownedLarge(2, layers);
  constexpr size_t tiles = 8;
  for (size_t at = 0; at < tiles; ++at) {
    const auto x = static_cast<int>(at % 4);
    const auto y = static_cast<int>(at / 4);
    const bool accepted =
        ownedSmall.Accept(x, y, small).has_value() && ownedLarge.Accept(x, y, large).has_value();
    CHECK(accepted, "new tiles preserve the same public geometry in both dictionary sizes");
    if (!accepted) { return; }
    CHECK(ownedLarge.HeapBytes() - ownedSmall.HeapBytes() == (at + 1) * delta,
          "container growth moves each immutable layer charge exactly once");
  }
}
}

int main() {
  CheckAccounting();
  CheckReallocation();
  return outshine::Test::Report();
}

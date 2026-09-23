#include "OsmField.h"
#include "OsmVector.h"
#include "test/outshine/src/world/ground/OsmVector/WireFixture.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

void AppendVarint(Bytes &bytes, uint64_t value) {
  while (value >= 0x80u) {
    bytes.push_back(static_cast<uint8_t>(value) | 0x80u);
    value >>= 7u;
  }
  bytes.push_back(static_cast<uint8_t>(value));
}

Bytes LineFeature(std::optional<uint64_t> id) {
  Bytes feature{0x18, 2, 0x22, 6, 9, 0, 0, 10, 2, 2};
  if (id) {
    feature.push_back(0x08);
    AppendVarint(feature, *id);
  }
  return feature;
}

Bytes VectorTile(std::span<const Bytes> features) {
  Bytes layer{0x0a, 1, 'x', 0x78, 2, 0x28, 64};
  for (const Bytes &feature : features) { Append(layer, 0x12, feature); }
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  constexpr uint64_t kLargestId = std::numeric_limits<uint64_t>::max();
  const std::array features{LineFeature(std::nullopt), LineFeature(0), LineFeature(kLargestId)};
  const Bytes valid = VectorTile(features);
  OsmVector decoded;
  const auto parsed = decoded.Parse(valid, "x");
  CHECK(parsed && decoded.Features().size() == 3, "MVT features decode with optional IDs");
  if (!parsed || decoded.Features().size() != 3) { return Report(); }
  CHECK(!decoded.Features()[0].ProviderFeatureId && decoded.Features()[1].ProviderFeatureId == 0 &&
            decoded.Features()[2].ProviderFeatureId == kLargestId,
        "absence, zero and the full uint64 range stay distinct");

  const std::array<std::string, 1> layers{"x"};
  OsmField forward(2, layers);
  OsmField reverse(2, layers);
  const Bytes repeated = VectorTile(std::array{LineFeature(kLargestId)});
  const auto first = forward.Accept(1, 1, valid);
  const auto second = forward.Accept(2, 1, repeated);
  const auto otherFirst = reverse.Accept(2, 1, repeated);
  const auto otherSecond = reverse.Accept(1, 1, valid);
  CHECK(first && second && otherFirst && otherSecond,
        "both tile arrival orders publish the provider IDs");
  if (!first || !second || !otherFirst || !otherSecond) { return Report(); }
  CHECK(forward.Features().size() == 4 && reverse.Features().size() == 4,
        "all native features are published");
  if (forward.Features().size() != 4 || reverse.Features().size() != 4) { return Report(); }
  bool matching = true;
  for (size_t at = 0; at < 4; ++at) {
    matching &=
        forward.Features()[at].ProviderFeatureId == reverse.Features()[at].ProviderFeatureId;
  }
  CHECK(matching && !forward.Features()[0].ProviderFeatureId &&
            forward.Features()[1].ProviderFeatureId == 0 &&
            forward.Features()[2].ProviderFeatureId == kLargestId &&
            forward.Features()[3].ProviderFeatureId == kLargestId,
        "native IDs survive canonical tile order and repeated cross-tile identity");

  Bytes duplicated = LineFeature(7);
  duplicated.push_back(0x08);
  AppendVarint(duplicated, 8);
  const size_t accepted = forward.Features().size();
  const uint64_t generation = forward.Generation();
  const auto rejected = forward.Accept(3, 1, VectorTile(std::array{duplicated}));
  CHECK(!rejected && forward.Features().size() == accepted && forward.Generation() == generation &&
            !forward.Settled(3, 1),
        "duplicate ID rejects the whole tile without changing the native snapshot");
  Bytes wrongWire = LineFeature(7);
  wrongWire.push_back(0x0a);
  wrongWire.push_back(1);
  wrongWire.push_back(0);
  const auto refused = forward.Accept(3, 1, VectorTile(std::array{wrongWire}));
  CHECK(!refused && forward.Features().size() == accepted && forward.Generation() == generation,
        "wrong-wire ID cannot masquerade as an absent identity");
  const auto corrected = forward.Accept(3, 1, repeated);
  CHECK(corrected && forward.Settled(3, 1) && forward.Features().size() == accepted + 1 &&
            forward.Features().back().ProviderFeatureId == kLargestId,
        "corrected retry publishes the provider ID once");
  return Report();
}

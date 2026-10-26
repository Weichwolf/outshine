#include "OsmField.h"
#include "MvtLayer.h"
#include "MvtWireFixture.h"
#include "Sha256.h"
#include "Check.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <print>
#include <span>
#include <string>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

constexpr uint8_t kFeatureType = 0x18;
constexpr uint8_t kFeatureGeometry = 0x22;
constexpr uint8_t kLayerVersion = 0x78;
constexpr uint8_t kLayerExtent = 0x28;
constexpr uint8_t kLayerFeature = 0x12;
constexpr uint8_t kTileLayer = 0x1a;

void AppendVarint(Bytes &bytes, uint64_t value) {
  while (value >= 0x80u) {
    bytes.push_back(static_cast<uint8_t>(value) | 0x80u);
    value >>= 7u;
  }
  bytes.push_back(static_cast<uint8_t>(value));
}

Bytes LineFeature(std::optional<uint64_t> id) {
  Bytes feature{kFeatureType, 2, kFeatureGeometry, 6, 9, 0, 0, 10, 2, 2};
  if (id) {
    feature.push_back(0x08);
    AppendVarint(feature, *id);
  }
  return feature;
}

Bytes VectorTile(std::span<const Bytes> features) {
  Bytes layer{0x0a, 1, 'x', kLayerVersion, 2, kLayerExtent, 64};
  for (const Bytes &feature : features) { Append(layer, kLayerFeature, feature); }
  Bytes tile;
  Append(tile, kTileLayer, layer);
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using ::outshine::Generators::Osm::MvtLayer;
  using namespace outshine::Test;
  constexpr uint64_t kLargestId = std::numeric_limits<uint64_t>::max();
  const std::array features{LineFeature(std::nullopt), LineFeature(0), LineFeature(kLargestId)};
  const Bytes valid = VectorTile(features);
  ::outshine::Generators::Osm::MvtLayer decoded;
  const auto parsed = decoded.Parse(valid, "x");
  CHECK(parsed && decoded.Features().size() == 3, "MVT features decode with optional IDs");
  if (!parsed || decoded.Features().size() != 3) { return Report(); }
  CHECK(!decoded.Features()[0].ProviderFeatureId && decoded.Features()[1].ProviderFeatureId == 0 &&
            decoded.Features()[2].ProviderFeatureId == kLargestId,
        "absence, zero and the full uint64 range stay distinct");
  for (size_t at = 0; at < features.size(); ++at) {
    CHECK(decoded.Features()[at].HasContentDigest &&
              decoded.Features()[at].ContentDigest ==
                  outshine::Sha256Digest(features[at].data(), features[at].size()),
          "each source feature retains the digest of its actual encoded message");
  }

  const std::array<std::string, 1> layers{"x"};
  ::outshine::Generators::Osm::OsmField forward(2, layers);
  ::outshine::Generators::Osm::OsmField reverse(2, layers);
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
        forward.Features()[at].ProviderFeatureId == reverse.Features()[at].ProviderFeatureId &&
        forward.Features()[at].HasContentDigest && reverse.Features()[at].HasContentDigest &&
        forward.Features()[at].ContentDigest == reverse.Features()[at].ContentDigest;
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
  std::println("MVT_FEATURE_SOURCE parser_feature_bytes={} field_feature_bytes={} digest_bytes=32",
               sizeof(MvtLayer::Feature),
               sizeof(::outshine::Generators::Osm::OsmField::Feature));
  return Report();
}

#ifndef OUTSHINE_TEST_MVTTRANSPORTWIREFIXTURE_H
#define OUTSHINE_TEST_MVTTRANSPORTWIREFIXTURE_H
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Test::MvtTransportWire {
using Bytes = std::vector<uint8_t>;
constexpr uint8_t kFeatureType = 0x18;
constexpr uint8_t kFeatureGeometry = 0x22;
constexpr uint8_t kFeatureTags = 0x12;
constexpr uint8_t kLayerVersion = 0x78;
constexpr uint8_t kLayerExtent = 0x28;
constexpr uint8_t kLayerKey = 0x1a;
constexpr uint8_t kLayerValue = 0x22;
constexpr uint8_t kLineToTwo = 18;
constexpr uint8_t kMiddleX = 24;
constexpr uint8_t kEndX = 40;
constexpr uint8_t kVarintMask = 127;
constexpr int kTileX = 32768;
constexpr int kNextTileX = kTileX + 1;

inline void Varint(Bytes &out, size_t value) {
  do {
    out.push_back(static_cast<uint8_t>((value & kVarintMask) | (value > kVarintMask ? 128u : 0u)));
    value >>= 7u;
  } while (value != 0);
}

inline void Message(Bytes &out, uint8_t field, std::span<const uint8_t> body) {
  out.push_back(field);
  Varint(out, body.size());
  out.insert(out.end(), body.begin(), body.end());
}

inline void Text(Bytes &out, uint8_t field, std::string_view text) {
  Message(out, field, {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
}

struct FeatureInput {
  std::optional<uint8_t> Id;
  Bytes Tags;
  Bytes Geometry;
};

inline Bytes Feature(const FeatureInput &input) {
  Bytes out{kFeatureType, 2};
  if (input.Id) { out.insert(out.end(), {8, *input.Id}); }
  Message(out, kFeatureTags, input.Tags);
  Message(out, kFeatureGeometry, input.Geometry);
  return out;
}

inline Bytes Tile() {
  Bytes layer{kLayerVersion, 2, kLayerExtent, 64};
  Text(layer, 0x0a, "streets");
  for (const auto *key : {"kind", "layer", "bridge", "oneway", "tunnel", "access", "surface"}) {
    Text(layer, kLayerKey, key);
  }
  for (const auto *text : {"path", "0", "1", "yes", "-1", "private", "gravel"}) {
    Bytes value;
    Text(value, 0x0a, text);
    Message(layer, kLayerValue, value);
  }
  Message(layer,
          kFeatureTags,
          Feature({.Id = {},
                   .Tags = {0, 0, 1, 1, 3, 4, 5, 5, 6, 6},
                   .Geometry = {9, 16, 32, kLineToTwo, 32, 0, 32, 0}}));
  Message(layer,
          kFeatureTags,
          Feature({.Id = 0,
                   .Tags = {0, 0, 1, 2, 2, 3},
                   .Geometry = {9, 2 * kMiddleX, 0, kLineToTwo, 0, 32, 0, 32}}));
  Message(layer,
          kFeatureTags,
          Feature({.Id = 7, .Tags = {0, 0, 4, 3}, .Geometry = {9, 2 * kEndX, 0, 10, 0, 64}}));
  Bytes out;
  Message(out, kLayerKey, layer);
  return out;
}

}
#endif

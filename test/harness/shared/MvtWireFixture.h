#ifndef OUTSHINE_TEST_MVTWIREFIXTURE_H
#define OUTSHINE_TEST_MVTWIREFIXTURE_H
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace outshine::Test::Mvt {
using Bytes = std::vector<uint8_t>;

inline void Append(Bytes &out, uint8_t tag, std::span<const uint8_t> bytes) {
  assert(bytes.size() < 128);
  out.push_back(tag);
  out.push_back(static_cast<uint8_t>(bytes.size()));
  out.insert(out.end(), bytes.begin(), bytes.end());
}

[[nodiscard]] inline Bytes BuildingTile() {
  constexpr uint8_t layerVersion = 0x78;
  constexpr uint8_t layerExtent = 0x28;
  constexpr uint8_t layerKey = 0x1a;
  constexpr uint8_t layerValue = 0x22;
  constexpr uint8_t featureType = 0x18;
  constexpr uint8_t featureTags = 0x12;
  constexpr uint8_t polygonGeometry = 0x22;
  constexpr uint8_t polygonLineTo = 26;
  constexpr uint8_t buildingHeight = 19;
  constexpr uint8_t side = 20;
  Bytes layer{0x0a, 8, 'b', 'u', 'i', 'l', 'd', 'i', 'n', 'g', layerVersion, 2, layerExtent, 64};
  Append(layer, layerKey, Bytes{'r', 'e', 'n', 'd', 'e', 'r', '_', 'h', 'e', 'i', 'g', 'h', 't'});
  Append(layer, layerValue, Bytes{layerExtent, buildingHeight});
  Bytes feature{0x08, 7, featureType, 3};
  Append(feature, featureTags, Bytes{0, 0});
  Append(feature,
         polygonGeometry,
         Bytes{9, 0, 0, polygonLineTo, side, 0, 0, side, buildingHeight, 0, 15});
  Append(layer, featureTags, feature);
  Bytes tile;
  Append(tile, layerKey, layer);
  return tile;
}

}

#endif

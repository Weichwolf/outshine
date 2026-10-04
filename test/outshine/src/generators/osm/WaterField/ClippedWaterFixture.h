#pragma once

#include "OsmField.h"
#include "TerrainLoader.h"
#include "WaterField.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine::Test::Water {
using Bytes = std::vector<uint8_t>;
using Point = std::array<int32_t, 2>;

inline void Varint(Bytes &into, uint64_t value) {
  while (value >= 128) {
    into.push_back(static_cast<uint8_t>(value) | 128);
    value >>= 7;
  }
  into.push_back(static_cast<uint8_t>(value));
}

inline void Field(Bytes &into, uint8_t tag, std::span<const uint8_t> bytes) {
  into.push_back(tag);
  Varint(into, bytes.size());
  into.insert(into.end(), bytes.begin(), bytes.end());
}

inline void Text(Bytes &into, uint8_t tag, std::string_view text) {
  Field(into, tag, {reinterpret_cast<const uint8_t *>(text.data()), text.size()});
}

inline void Ring(Bytes &geometry, std::span<const Point> points, Point &previous) {
  Varint(geometry, 9);
  for (size_t at = 0; at < points.size(); ++at) {
    if (at == 1) { Varint(geometry, ((points.size() - 1) << 3) | 2); }
    for (size_t axis = 0; axis < 2; ++axis) {
      const int64_t delta = static_cast<int64_t>(points[at][axis]) - previous[axis];
      Varint(geometry,
             delta >= 0 ? static_cast<uint64_t>(delta) * 2 : static_cast<uint64_t>(-delta) * 2 - 1);
    }
    previous = points[at];
  }
  Varint(geometry, 15);
}

inline Bytes VectorTile(std::optional<uint64_t> id,
                        std::string_view kind,
                        std::span<const Point> exterior,
                        std::span<const Point> hole = {}) {
  Bytes geometry;
  Point previous{};
  Ring(geometry, exterior, previous);
  if (!hole.empty()) { Ring(geometry, hole, previous); }
  Bytes feature;
  if (id) {
    feature.push_back(8);
    Varint(feature, *id);
  }
  Field(feature, 0x12, Bytes{0, 0});
  feature.insert(feature.end(), {0x18, 3});
  Field(feature, 0x22, geometry);
  Bytes layer;
  Text(layer, 0x0a, "water_polygons");
  Field(layer, 0x12, feature);
  Text(layer, 0x1a, "kind");
  Bytes value;
  Text(value, 0x0a, kind);
  Field(layer, 0x22, value);
  layer.insert(layer.end(), {0x28, 64, 0x78, 2});
  Bytes tile;
  Field(tile, 0x1a, layer);
  return tile;
}

inline std::array<Point, 4> Rectangle(int32_t minX, int32_t minY, int32_t maxX, int32_t maxY) {
  return {Point{minX, minY}, Point{maxX, minY}, Point{maxX, maxY}, Point{minX, maxY}};
}

class Heights final : public GroundQuery {
public:
  bool AlongLatitude = false;

  GroundSample At(LongitudeLatitude at) const override {
    const bool lower = AlongLatitude ? at.LatitudeDeg > 10 : at.LongitudeDeg < -20;
    return GroundSample::At(lower ? 10.0 : 100.0);
  }

  GroundSample Resident(LongitudeLatitude at) const override { return At(at); }

  Ground::GroundBlock BlockAt(Ground::TileSpot) const override {
    return Ground::GroundBlock::Waiting();
  }

  double PostM(double) const override { return 1.0; }
};

inline void Finish(Generators::Osm::WaterField &water,
                   const Generators::Osm::OsmField &field,
                   bool alongLatitude = false) {
  Heights ground;
  ground.AlongLatitude = alongLatitude;
  Ground::VegetationTemplates materials;
  for (unsigned step = 0; step < 32 && !water.Ingested(field); ++step) {
    (void)water.Ingest(ground, field, materials);
  }
}
}

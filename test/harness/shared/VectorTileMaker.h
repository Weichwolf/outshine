#ifndef OUTSHINE_TEST_VECTORTILEMAKER_H
#define OUTSHINE_TEST_VECTORTILEMAKER_H

#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

// board:1806. The ground layer's fields all read an OsmField, and an OsmField is built from
// Mapbox Vector Tile bytes. Encoding one by hand is what makes those fields provable without a
// pool, a store or a network -- so the encoder lives here once rather than in each twin.
//
// From the MVT 2.1 specification:
//   Tile    { repeated Layer layers = 3 }
//   Layer   { string name = 1, repeated Feature features = 2, repeated string keys = 3,
//             repeated Value values = 4, uint32 extent = 5, uint32 version = 15 }
//   Feature { uint64 id = 1, packed uint32 tags = 2, GeomType type = 3, packed uint32 geometry = 4
//   } Value   { string string_value = 1, double double_value = 3 }
// Geometry is command integers (id | count << 3) followed by zig-zag encoded parameters, and
// every parameter is a delta from the cursor's previous position.
namespace outshine::Test::Mvt {

enum class Geometry : uint32_t { Point = 1, Line = 2, Polygon = 3 };

class Buffer {
public:
  void Varint(uint64_t value) {
    while (value >= 0x80) {
      Out.push_back((uint8_t)(value | 0x80));
      value >>= 7;
    }
    Out.push_back((uint8_t)value);
  }

  void Key(uint32_t field, uint32_t wire) { Varint(((uint64_t)field << 3) | wire); }

  void Uint(uint32_t field, uint64_t value) {
    Key(field, 0);
    Varint(value);
  }

  void Block(uint32_t field, const std::vector<uint8_t> &body) {
    Key(field, 2);
    Varint(body.size());
    Out.insert(Out.end(), body.begin(), body.end());
  }

  void Text(uint32_t field, const std::string &value) {
    Block(field, std::vector<uint8_t>(value.begin(), value.end()));
  }

  void Double(uint32_t field, double value) {
    Key(field, 1);
    uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof bits);
    for (int at = 0; at < 8; ++at) { Out.push_back((uint8_t)(bits >> (8 * at))); }
  }

  std::vector<uint8_t> Out;
};

[[nodiscard]] inline uint32_t ZigZag(int32_t value) {
  return (uint32_t)((value << 1) ^ (value >> 31));
}

struct Tag {
  std::string Key;
  std::string Text;
  double Number = 0.0;
  bool IsNumber = false;
};

[[nodiscard]] inline Tag Says(std::string key, std::string text) {
  return Tag{std::move(key), std::move(text), 0.0, false};
}

[[nodiscard]] inline Tag Counts(std::string key, double number) {
  return Tag{std::move(key), std::string(), number, true};
}

struct Shape {
  Geometry Form = Geometry::Line;
  std::vector<int32_t> XY;
  std::vector<Tag> Tags;
};

// one layer of N shapes, in tile-local coordinates over the declared extent.
[[nodiscard]] inline std::vector<uint8_t>
Layer(const std::string &name, const std::vector<Shape> &shapes, uint32_t extent = 4096) {
  Buffer layer;
  layer.Text(1, name);

  std::vector<std::string> keys;
  std::vector<Tag> values;
  std::vector<std::vector<uint8_t>> features;

  for (const Shape &shape : shapes) {
    Buffer feature;
    feature.Uint(1, (uint64_t)(features.size() + 1));

    Buffer tags;
    for (const Tag &tag : shape.Tags) {
      size_t keyAt = 0;
      while (keyAt < keys.size() && keys[keyAt] != tag.Key) { ++keyAt; }
      if (keyAt == keys.size()) { keys.push_back(tag.Key); }
      tags.Varint(keyAt);
      tags.Varint(values.size());
      values.push_back(tag);
    }
    if (!tags.Out.empty()) { feature.Block(2, tags.Out); }
    feature.Uint(3, (uint64_t)shape.Form);

    Buffer geometry;
    const size_t points = shape.XY.size() / 2;
    int32_t cursorX = 0, cursorY = 0;
    if (points > 0) {
      geometry.Varint(1u | (1u << 3));
      geometry.Varint(ZigZag(shape.XY[0] - cursorX));
      geometry.Varint(ZigZag(shape.XY[1] - cursorY));
      cursorX = shape.XY[0];
      cursorY = shape.XY[1];
    }
    if (points > 1) {
      geometry.Varint(2u | ((uint32_t)(points - 1) << 3));
      for (size_t at = 1; at < points; ++at) {
        geometry.Varint(ZigZag(shape.XY[2 * at] - cursorX));
        geometry.Varint(ZigZag(shape.XY[2 * at + 1] - cursorY));
        cursorX = shape.XY[2 * at];
        cursorY = shape.XY[2 * at + 1];
      }
    }
    if (shape.Form == Geometry::Polygon) { geometry.Varint(7u | (1u << 3)); }
    feature.Block(4, geometry.Out);

    features.push_back(feature.Out);
  }

  for (const std::vector<uint8_t> &one : features) { layer.Block(2, one); }
  for (const std::string &key : keys) { layer.Text(3, key); }
  for (const Tag &value : values) {
    Buffer held;
    if (value.IsNumber) {
      held.Double(3, value.Number);
    } else {
      held.Text(1, value.Text);
    }
    layer.Block(4, held.Out);
  }
  layer.Uint(5, extent);
  layer.Uint(15, 2u);
  return layer.Out;
}

[[nodiscard]] inline std::vector<uint8_t> Tile(const std::vector<std::vector<uint8_t>> &layers) {
  Buffer tile;
  for (const std::vector<uint8_t> &one : layers) { tile.Block(3, one); }
  return tile.Out;
}

} // namespace outshine::Test::Mvt
#endif

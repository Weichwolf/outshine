#include "StreetField.h"
#include "GroundMaterials.h"
#include "Check.h"
#include <array>
#include <bit>
#include <cstdint>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using Bytes = std::vector<uint8_t>;

void Append(Bytes &out, uint8_t tag, std::span<const uint8_t> bytes) {
  out.push_back(tag);
  out.push_back(static_cast<uint8_t>(bytes.size()));
  out.insert(out.end(), bytes.begin(), bytes.end());
}

Bytes Text(std::string_view value) {
  Bytes result{0x0a, static_cast<uint8_t>(value.size())};
  result.insert(result.end(), value.begin(), value.end());
  return result;
}

Bytes Number(double value) {
  Bytes result{0x19};
  auto bits = std::bit_cast<uint64_t>(value);
  for (unsigned at = 0; at < 8; ++at) {
    result.push_back(static_cast<uint8_t>(bits & 255));
    bits >>= 8;
  }
  return result;
}

Bytes Tile(const Bytes &value) {
  Bytes layer{0x0a, 7, 's', 't', 'r', 'e', 'e', 't', 's', 0x78, 2, 0x28, 0x80, 0x20};
  Append(layer, 0x1a, std::array<uint8_t, 4>{'k', 'i', 'n', 'd'});
  Append(layer, 0x22, Text("path"));
  Bytes feature{0x12, 2, 0, 0, 0x18, 2, 0x22, 6, 9, 0, 0, 10, 2, 2};
  if (!value.empty()) {
    Append(layer, 0x1a, std::array<uint8_t, 5>{'l', 'a', 'y', 'e', 'r'});
    Append(layer, 0x22, value);
    feature.insert(feature.begin() + 4, {1, 1});
    feature[1] = 4;
  }
  Append(layer, 0x12, feature);
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine::Ground;
  using namespace outshine::Test;
  GroundMaterials materials;
  VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json"), "ground catalog loads");
  CHECK(templates.Load("src/assets/world/vegetation.json", materials), "street rules load");
  if (!templates.Ready()) { return Report(); }

  struct Case {
    Bytes Wire;
    std::optional<int32_t> Layer;
  };

  const std::vector<Case> cases{{{}, 0},
                                {Number(0), 0},
                                {Number(-2), -2},
                                {Text("-3"), -3},
                                {Text("+2"), 2},
                                {Number(2147483647.0), INT32_MAX},
                                {Number(-2147483648.0), INT32_MIN},
                                {Text("2147483647"), INT32_MAX},
                                {Text("-2147483648"), INT32_MIN},
                                {Number(2147483648.0), {}},
                                {Number(-2147483649.0), {}},
                                {Number(0.5), {}},
                                {Number(std::numeric_limits<double>::max()), {}},
                                {Number(std::numeric_limits<double>::infinity()), {}},
                                {Number(std::numeric_limits<double>::quiet_NaN()), {}},
                                {Text("2147483648"), {}},
                                {Text("-2147483649"), {}},
                                {Text("1x"), {}},
                                {Text("1.5"), {}},
                                {Text(""), {}},
                                {Text("+"), {}},
                                {Text("+-1"), {}},
                                {Text(" 1"), {}},
                                {Text("1 "), {}}};
  for (const Case &one : cases) {
    const std::array<std::string, 1> layers{"streets"};
    OsmField field(1, layers);
    const auto accepted = field.Accept(0, 0, Tile(one.Wire));
    CHECK(accepted && field.Features().size() == 1, "independent wire fixture supplies one line");
    if (!accepted || field.Features().size() != 1) { continue; }
    StreetField streets;
    const auto count = streets.Ingest(field, templates);
    CHECK(count == (one.Layer ? 1u : 0u), "only valid layer values publish a way");
    CHECK(streets.InvalidLayerCount() == (one.Layer ? 0 : 1),
          "invalid values are counted explicitly");
    if (one.Layer && !streets.Ways().empty()) {
      CHECK(streets.Ways().front().Layer == *one.Layer, "signed layer survives without truncation");
      CHECK(streets.Ways().front().PointCount == 2 && streets.Ways().front().HalfWidthM > 0,
            "valid line geometry and width remain usable");
      CHECK(streets.LayerSaidCount() == (one.Wire.empty() ? 0 : 1),
            "numeric and textual declarations both count as explicit layers");
    }
    CHECK(streets.Ingest(field, templates) == count &&
              streets.InvalidLayerCount() == (one.Layer ? 0 : 1),
          "repeated ingestion neither duplicates ways nor recounts invalid features");
  }
  return Report();
}

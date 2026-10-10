#include "StreetField.h"
#include "GroundMaterials.h"
#include "Check.h"
#include "../MvtLayer/WireFixture.h"

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

namespace {
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;

void Number(Bytes &into, uint32_t value) {
  do {
    into.push_back(static_cast<uint8_t>((value & 127u) | (value > 127u ? 128u : 0u)));
    value >>= 7u;
  } while (value != 0);
}

Bytes Tile(std::string_view kind, int32_t firstX) {
  Bytes layer{0x0a, 7, 's', 't', 'r', 'e', 'e', 't', 's', 0x78, 2, 0x28, 0x80, 0x20};
  Append(layer, 0x1a, std::array<uint8_t, 4>{'k', 'i', 'n', 'd'});
  Bytes text{0x0a, static_cast<uint8_t>(kind.size())};
  text.insert(text.end(), kind.begin(), kind.end());
  Append(layer, 0x22, text);
  Bytes geometry{9};
  Number(geometry,
         firstX >= 0 ? 2u * static_cast<uint32_t>(firstX)
                     : 2u * static_cast<uint32_t>(-firstX) - 1u);
  Number(geometry, 4096);
  geometry.push_back(10);
  Number(geometry, 2u * static_cast<uint32_t>(3072 - firstX));
  Number(geometry, 0);
  Bytes feature{0x12, 2, 0, 0, 0x18, 2};
  Append(feature, 0x22, geometry);
  Append(layer, 0x12, feature);
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Generators::Osm;
  using namespace outshine::Test;
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "transport classes use the production recipes");
  if (!vegetation.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  for (const auto kind : {"residential", "footway", "rail"}) {
    for (const int32_t firstX : {-64, 0, 2048}) {
      OsmField field(1, layers);
      CHECK(field.Accept(0, 0, Tile(kind, firstX)).has_value(),
            "a supplied MVT line retains its buffered or clipped end");
      StreetField streets;
      CHECK(streets.Ingest(field, vegetation) == 1, "the supplied transport line is retained");
      if (streets.Ways().empty()) { continue; }
      const auto &way = streets.Ways().front();
      const auto traffic = std::string_view(kind) == "rail"      ? StreetField::Traffic::Rail
                           : std::string_view(kind) == "footway" ? StreetField::Traffic::Path
                                                                 : StreetField::Traffic::Road;
      CHECK(way.TrafficKind == traffic, "the supplied transport class survives normalization");
      CHECK(way.OwnsEnds[0] == (firstX > 0) && way.OwnsEnds[1],
            "a buffered or tile-border end cannot prove a physical bridge landing");
    }
  }
  return Report();
}

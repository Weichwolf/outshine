#include "StreetField.h"
#include "GroundMaterials.h"
#include "Check.h"
#include "MvtWireFixture.h"
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>

namespace {
using outshine::Generators::Osm::OsmField;
using outshine::Generators::Osm::StreetField;
using outshine::Test::Mvt::Append;
using outshine::Test::Mvt::Bytes;
constexpr uint64_t kLargeId = std::numeric_limits<uint64_t>::max();

void Varint(Bytes &out, uint64_t value) {
  do {
    out.push_back(static_cast<uint8_t>((value & 127u) | (value > 127u ? 128u : 0u)));
    value >>= 7u;
  } while (value != 0);
}

Bytes Feature(std::optional<uint64_t> id, uint8_t kind, bool twoRings) {
  Bytes feature;
  if (id) {
    feature.push_back(8);
    Varint(feature, *id);
  }
  feature.insert(feature.end(), {0x12, 2, 0, kind, 0x18, 2});
  Bytes geometry{9, 0, 0, 10, 2, 2};
  if (twoRings) { geometry.insert(geometry.end(), {9, 2, 2, 10, 2, 2}); }
  Append(feature, 0x22, geometry);
  return feature;
}

Bytes Tile() {
  Bytes layer{0x0a, 7, 's', 't', 'r', 'e', 'e', 't', 's', 0x78, 2, 0x28, 64};
  Append(layer, 0x1a, std::array<uint8_t, 4>{'k', 'i', 'n', 'd'});
  Append(layer, 0x22, Bytes{0x0a, 4, 'p', 'a', 't', 'h'});
  Append(layer, 0x22, Bytes{0x0a, 7, 'u', 'n', 'k', 'n', 'o', 'w', 'n'});
  Append(layer, 0x12, Feature({}, 1, false));
  Append(layer, 0x12, Feature(0, 0, true));
  Append(layer, 0x12, Feature(kLargeId, 0, false));
  Bytes tile;
  Append(tile, 0x1a, layer);
  return tile;
}

void CheckReferences(const StreetField &streets, const OsmField &field, size_t tiles) {
  using namespace outshine::Test;
  CHECK(streets.Ways().size() == 3 * tiles, "only accepted path rings become ways");
  if (streets.Ways().size() != 3 * tiles) { return; }
  for (size_t tile = 0; tile < tiles; ++tile) {
    for (uint32_t at = 0; at < 3; ++at) {
      const auto &way = streets.Ways()[3 * tile + at];
      CHECK(way.Source.has_value(), "every ingested way carries its actual source ordinals");
      if (!way.Source) { continue; }
      const auto bound = streets.SourceOf(field, 3 * tile + at);
      CHECK(bound.has_value(), "source lookup is bound to the actual ingested context");
      if (!bound) { continue; }
      const auto origin = *bound;
      CHECK(origin.Feature == 3 * tile + (at < 2 ? 1 : 2) && origin.Ring == 4 * tile + at + 1,
            "filtered features and multiple rings retain original ordinals");
      const auto &feature = field.Features()[origin.Feature];
      const auto &ring = field.Rings()[origin.Ring];
      CHECK(feature.Tile == tile && feature.ProviderFeatureId == (at < 2 ? 0 : kLargeId),
            "zero and full-width provider IDs resolve in their original source tile");
      CHECK(ring.First == way.FirstPoint && ring.Count == way.PointCount &&
                origin.Ring >= feature.FirstRing &&
                origin.Ring - feature.FirstRing < feature.RingCount,
            "the referenced feature owns the exact normalized ring");
    }
  }
}
}

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates vegetation;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            vegetation.Load("src/assets/world/vegetation.json", materials),
        "production transport recipes load");
  if (!vegetation.Ready()) { return Report(); }
  const std::array<std::string, 1> layers{"streets"};
  OsmField field(2, layers);
  StreetField streets;
  CHECK(field.Accept(0, 0, Tile()).has_value(), "first original tile is accepted");
  CHECK(streets.Ingest(field, vegetation) == 3, "first tile is normalized");
  CheckReferences(streets, field, 1);
  CHECK(field.Accept(1, 0, Tile()).has_value(), "another tile grows the original storage");
  CHECK(!streets.SourceOf(field, 0), "a new source generation must be normalized first");
  for (size_t tile = 0; tile < 2 && !streets.Ingested(field); ++tile) {
    (void)streets.Ingest(field, vegetation);
  }
  CHECK(streets.Ingested(field) && streets.Ways().size() == 6,
        "the complete new generation retains both tiles");
  streets.Settle();
  CheckReferences(streets, field, 2);
  CHECK(!streets.Without(0).Ways()[0].Source, "an intentionally removed way has no false source");
  CHECK(!streets.SourceOf(field, streets.Ways().size()), "an absent way has no source reference");
  OsmField declared(2, layers);
  const std::array<OsmField::Declared, 1> paths{
      {{.Layer = "streets", .Key = "kind", .Value = "path", .LatLon = {40, -45, 40.001, -45.001}}}};
  CHECK(declared.Declare(paths, {.LongitudeDeg = -45, .LatitudeDeg = 40}).has_value(),
        "a source without a provider ID is declared");
  CHECK(!streets.SourceOf(declared, 0), "equal local ordinals do not alias another source context");
  CHECK(streets.Ingest(declared, vegetation) == 1 && streets.Ways()[0].Source.has_value(),
        "a new source context replaces previous references");
  CHECK(!streets.SourceOf(field, 0), "previous source contexts cannot resolve replacement ways");
  if (!streets.Ways().empty() && streets.Ways()[0].Source) {
    const auto origin = *streets.Ways()[0].Source;
    CHECK(origin.Feature == 0 && origin.Ring == 0 &&
              !declared.Features()[origin.Feature].ProviderFeatureId,
          "missing provider identity stays missing rather than becoming zero");
  }
  const auto snapshot = declared.SnapshotQueries();
  CHECK(streets.SourceOf(*snapshot, 0).has_value(), "an immutable snapshot keeps its source scope");
  CHECK(declared.Declare(paths, {.LongitudeDeg = -45, .LatitudeDeg = 40}).has_value(),
        "identical declarations are accepted without republishing");
  CHECK(streets.SourceOf(declared, 0).has_value(), "unchanged source inputs keep their references");
  auto replacement = paths;
  replacement[0].LatLon.back() -= 0.001;
  CHECK(declared.Declare(replacement, {.LongitudeDeg = -45, .LatitudeDeg = 40}).has_value(),
        "changed coordinates publish a new generation in the same field");
  CHECK(!streets.SourceOf(declared, 0) && streets.SourceOf(*snapshot, 0).has_value(),
        "reused ordinals in a new generation cannot replace the original snapshot");
  return Report();
}

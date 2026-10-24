#include "Check.h"
#include "ContentStore.h"
#include "GroundMaterials.h"
#include "OsmField.h"
#include "PreparedStreetGraph.h"
#include "Sha256.h"
#include "SourceSet.h"
#include "StreetField.h"
#include "TerrainGrid.h"
#include "VegetationTemplates.h"
#include "Wayfinding.h"
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <system_error>

namespace {
using namespace outshine;
namespace Osm = Generators::Osm;
constexpr int kZoom = 14;
constexpr float kChangedHeightM = 40;

Ground::TerrainField Flat(float heightM) {
  Ground::TerrainField field(2, 2);
  for (uint32_t row = 0; row < 2; ++row) {
    for (uint32_t column = 0; column < 2; ++column) { field.SetM(row, column, heightM); }
  }
  return field;
}

std::expected<Osm::StreetGraphBuilder::Built, std::string>
Generate(const Osm::OsmField &vectors,
         const Osm::StreetField &ways,
         const Ground::TerrainField &terrain,
         size_t &factories,
         size_t &samples) {
  ++factories;
  auto result = Osm::StreetGraphBuilder::BuildOneShot(
      ways, vectors.Points(), [&]([[maybe_unused]] LongitudeLatitude at) -> std::optional<double> {
        ++samples;
        return terrain.PostingM({.Col = 0.5, .Row = 0.5});
      });
  if (!result.Graph || result.Elevated.Refused != 0) {
    return std::unexpected("the declared terrain fixture must produce a complete graph");
  }
  return result;
}

void CheckTerrainBinding(const std::filesystem::path &root) {
  Data::ContentStore store({.Directory = (root / "sources").string()});
  Data::SourceSet sources(store);
  Ground::GroundMaterials materials;
  Ground::VegetationTemplates templates;
  CHECK(materials.Load("src/assets/world/ground-materials.json") &&
            templates.Load("src/assets/world/vegetation.json", materials),
        "the fixture uses the production street rules");
  const std::array<std::string, 1> layers{"streets"};
  Osm::OsmField vectors(kZoom, layers);
  const std::array<Osm::OsmField::Declared, 1> declarations{
      {{.Layer = "streets", .Key = "kind", .Value = "path", .LatLon = {47, 9, 47, 9.002}}}};
  CHECK(vectors.Declare(declarations, {.LongitudeDeg = 9, .LatitudeDeg = 47}),
        "one fixed road is declared independently of its terrain elevations");
  Osm::StreetField ways;
  CHECK(ways.Ingest(vectors, templates) == 1, "the fixed source scope contains one street");
  const auto opened = Osm::PreparedStreetGraph::Open((root / "assets").string(), sources);
  CHECK(opened, "the production street graph cache opens");
  if (!opened) { return; }
  const auto &cache = *opened;
  const auto originalTerrain = Flat(0);
  const auto changedTerrain = Flat(kChangedHeightM);
  constexpr size_t kRasterBytes = 4 * sizeof(float);
  const auto originalBinding = Sha256Digest(originalTerrain.Data(), kRasterBytes);
  const auto changedBinding = Sha256Digest(changedTerrain.Data(), kRasterBytes);
  const auto originalKey = cache->Key(vectors, ways, {}, kZoom, originalBinding);
  const auto changedKey = cache->Key(vectors, ways, {}, kZoom, changedBinding);
  CHECK(originalKey && changedKey, "both captured terrain inputs can be identified");
  if (!originalKey || !changedKey) { return; }
  size_t factories = 0, samples = 0;
  const auto cold = cache->Resolve(
      *originalKey, [&] { return Generate(vectors, ways, originalTerrain, factories, samples); });
  CHECK(cold && !cold->Hit && factories == 1, "the initial native graph is generated once");
  const auto changed = cache->Resolve(
      *changedKey, [&] { return Generate(vectors, ways, changedTerrain, factories, samples); });
  const auto profile = changed ? changed->Graph.Graph->Profile({.Way = 0, .StationM = 0})
                               : std::optional<Path::Network::Station>{};
  std::cout << "STREET_TERRAIN_BINDING keys_equal=" << (*originalKey == *changedKey)
            << " factories=" << factories << " samples=" << samples
            << " changed_hit=" << (changed && changed->Hit)
            << " returned_height_m=" << (profile ? profile->HeightM : -1) << '\n';
  CHECK(*originalKey != *changedKey, "different captured terrain cannot share content identity");
  CHECK(changed && !changed->Hit && factories == 2,
        "changed terrain must run elevation rather than reuse the previous graph");
  CHECK(profile && profile->HeightM == kChangedHeightM,
        "the native road profile follows the terrain actually supplied to production");
  const auto reopened = Osm::PreparedStreetGraph::Open((root / "assets").string(), sources);
  CHECK(reopened, "a new cache owner opens both persisted terrain variants");
  if (!reopened) { return; }
  const auto samplesBefore = samples;
  const auto warm = (*reopened)->Resolve(*changedKey, {});
  const auto old = (*reopened)->Resolve(*originalKey, {});
  const auto warmProfile = warm ? warm->Graph.Graph->Profile({.Way = 0, .StationM = 0})
                                : std::optional<Path::Network::Station>{};
  const auto oldProfile = old ? old->Graph.Graph->Profile({.Way = 0, .StationM = 0})
                              : std::optional<Path::Network::Station>{};
  CHECK(warm && warm->Hit && warmProfile && warmProfile->HeightM == kChangedHeightM && old &&
            old->Hit && oldProfile && oldProfile->HeightM == 0 && factories == 2 &&
            samples == samplesBefore,
        "fresh owner hits preserve both independent profiles without a factory or elevation");
}
}

int main() {
  using namespace outshine::Test;
  const auto root = std::filesystem::temp_directory_path() /
                    ("outshine-street-terrain-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::error_code error;
  std::filesystem::create_directories(root / "assets", error);
  CHECK(!error, "the test owns a fresh asset directory");
  CheckTerrainBinding(root);
  std::filesystem::remove_all(root, error);
  CHECK(!error, "the isolated cache and source directories are removed");
  return Report();
}

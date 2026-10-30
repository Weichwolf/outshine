#ifndef OUTSHINE_TEST_SCOPEDGROUNDSOURCEFIXTURE_H
#define OUTSHINE_TEST_SCOPEDGROUNDSOURCEFIXTURE_H

#include "GroundMaterialCatalog.h"
#include "GroundPortSources.h"
#include "MvtGroundPortInputs.h"
#include "MvtTransportSnapshot.h"
#include "OsmField.h"
#include "RegionSurfaceRecipes.h"
#include "ReadTextFile.h"
#include "Sha256.h"
#include "StreetField.h"
#include "StreetGraphBuilder.h"
#include <array>
#include <cmath>
#include <cstdint>
#include <expected>
#include <format>
#include <memory>
#include <numbers>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>
#include <zlib.h>

namespace outshine::Test::ScopedGroundFixture {
struct Source {
  std::unique_ptr<Generators::Osm::OsmField> Field;
  Generators::Osm::StreetField Streets;
  Generators::Osm::MvtTransportSnapshot Snapshot;
  Generators::Osm::MvtGroundFieldCandidateInputs Inputs;
  Generators::GroundPortSources Index;
};

inline std::expected<std::string, std::string> Raw() {
  constexpr auto path = "test/experiments/transport/fixtures/bridge-source-facts/source.mvt.gz";
  gzFile file = gzopen(path, "rb");
  if (file == nullptr) { return std::unexpected("original source MVT unavailable"); }
  std::string bytes;
  std::array<char, 65536> chunk{};
  int count = 0;
  while ((count = gzread(file, chunk.data(), static_cast<unsigned>(chunk.size()))) > 0) {
    if (static_cast<size_t>(count) > 1048576 - bytes.size()) {
      (void)gzclose(file);
      return std::unexpected("original source MVT exceeds fixture budget");
    }
    bytes.append(chunk.data(), static_cast<size_t>(count));
  }
  if (gzclose(file) != Z_OK || count < 0 ||
      Sha256Hex(bytes) != "97489e625f1bdaeec27074600eafb72c7e2ce9fc57ea971286b8751cf9e44db8") {
    return std::unexpected("original source MVT pin changed");
  }
  return bytes;
}

inline std::expected<Source, std::string> Load(unsigned publications = 1) {
  using namespace Generators;
  using namespace Generators::Osm;
  auto raw = Raw();
  if (!raw) { return std::unexpected(raw.error()); }
  const auto materialBytes = ReadTextFile("data/world/ground-materials.json", 1048576);
  const auto recipeBytes = ReadTextFile("data/world/vegetation.json", 1048576);
  if (!materialBytes || !recipeBytes ||
      Sha256Hex(*materialBytes) !=
          "8d98fc709485870de6c8d61af160c1337424ef00994a512a5dda43173c88a658" ||
      Sha256Hex(*recipeBytes) !=
          "aa45e156d194479404beff997b81201331e27ec983f27952fe23c77432dbca9b") {
    return std::unexpected("original source recipe pins changed");
  }
  GroundMaterialCatalog materials;
  RegionSurfaceRecipes recipes;
  if (!materials.Load("data/world/ground-materials.json") ||
      !recipes.Load("data/world/vegetation.json", materials)) {
    return std::unexpected("actual source recipes unavailable");
  }
  constexpr std::array<int, 3> tile{14, 8937, 5678};
  const std::array<std::string, 1> layers{"streets"};
  auto field = std::make_unique<OsmField>(tile[0], layers, MvtSchema::OpenMapTiles);
  if (publications < 1 || publications > 2) {
    return std::unexpected("fixture requires one or two actual original publications");
  }
  for (unsigned publication = 0; publication < publications; ++publication) {
    if (!field->Accept(tile[1],
                       tile[2],
                       std::span(reinterpret_cast<const uint8_t *>(raw->data()), raw->size()))) {
      return std::unexpected("original MVT cannot be decoded");
    }
  }
  StreetField streets;
  for (size_t pass = 0; pass < field->Tiles().size() && !streets.Ingested(*field); ++pass) {
    (void)streets.Ingest(*field, recipes);
  }
  const auto native = StreetGraphBuilder::BuildOneShot(
      streets, field->Points(), [](LongitudeLatitude) -> std::optional<double> { return {}; });
  if (!native.Graph) { return std::unexpected("original native graph unavailable"); }
  const double cells = std::ldexp(1., tile[0]);
  const auto latitude = [cells](int y) {
    return std::atan(std::sinh(std::numbers::pi * (1 - 2 * y / cells))) * 180 / std::numbers::pi;
  };
  const std::array<GeographicBounds, 1> coverage{{{.WestDeg = tile[1] / cells * 360 - 180,
                                                   .SouthDeg = latitude(tile[2] + 1),
                                                   .EastDeg = (tile[1] + 1) / cells * 360 - 180,
                                                   .NorthDeg = latitude(tile[2])}}};
  auto snapshot = BuildMvtTransportSnapshot(
      *field,
      streets,
      *native.Graph,
      coverage,
      {},
      {.NodeGroupPayloadBytesMost = 16777216, .SourceFactsPayloadBytesMost = 67108864});
  if (!snapshot) { return std::unexpected(snapshot.error()); }
  if (!snapshot->SourceFacts) { return std::unexpected("original source facts unavailable"); }
  if (snapshot->SourceWayCount != 1557 || snapshot->SourceGeneration != publications ||
      snapshot->InputConnections != 9344 || snapshot->Network->Topology().Nodes().size() != 4433 ||
      snapshot->Network->Topology().Edges().size() != 8623) {
    return std::unexpected("original native source topology changed");
  }
  auto inputs = BindMvtGroundFieldCandidateInputs(*snapshot, *field, streets);
  if (!inputs) {
    return std::unexpected(std::format("actual MVT ground profiles unavailable: {}",
                                       static_cast<unsigned>(inputs.error())));
  }
  auto index = GroundPortSources::Make(
      inputs->Ports().Network, inputs->Ports().Profiles, inputs->Ports().UnknownNodes);
  if (!index) { return std::unexpected("actual native physical source index unavailable"); }
  return Source{.Field = std::move(field),
                .Streets = std::move(streets),
                .Snapshot = std::move(*snapshot),
                .Inputs = std::move(*inputs),
                .Index = std::move(*index)};
}
}
#endif

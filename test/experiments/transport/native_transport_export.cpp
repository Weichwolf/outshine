#include "native_transport_json.h"
#include "MvtTransportSnapshot.h"
#include "OsmField.h"
#include "StreetField.h"
#include "StreetGraphBuilder.h"
#include "GroundMaterialCatalog.h"
#include "RegionSurfaceRecipes.h"
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <ios>
#include <memory>
#include <numbers>
#include <optional>
#include <print>
#include <string>
#include <system_error>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;

bool WriteCapture(const std::filesystem::path &directory,
                  const MvtTransportSnapshot &snapshot,
                  const OsmField &field,
                  const StreetField &streets,
                  const std::array<int, 3> &tile) {
  std::error_code error;
  if (!std::filesystem::create_directory(directory, error)) { return false; }
  using File = std::unique_ptr<std::FILE, decltype(&std::fclose)>;
  File native(std::fopen((directory / "native.json").string().c_str(), "wb"), &std::fclose);
  File facts(std::fopen((directory / "source-facts.json").string().c_str(), "wb"), &std::fclose);
  if (!native || !facts) { return false; }
  if (!Test::TransportCapture::WriteSnapshot(native.get(), snapshot, field, streets, tile)) {
    return false;
  }
  Test::TransportCapture::WriteSourceFacts(facts.get(), *snapshot.SourceFacts, field, tile);
  const bool okay = std::ferror(native.get()) == 0 && std::ferror(facts.get()) == 0;
  const bool nativeClosed = std::fclose(native.release()) == 0;
  const bool factsClosed = std::fclose(facts.release()) == 0;
  return okay && nativeClosed && factsClosed;
}

std::optional<std::array<int, 3>> Tile(const char *const *argv) {
  std::array<int, 3> tile{};
  for (size_t at = 0; at < tile.size(); ++at) {
    const std::string value = argv[at + 2];
    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), tile[at]);
    if (parsed.ec != std::errc{} || parsed.ptr != value.data() + value.size()) { return {}; }
  }
  constexpr int kLargestZoom = 30;
  if (tile[0] < 0 || tile[0] > kLargestZoom) { return {}; }
  const int cells = static_cast<int>(uint32_t{1} << static_cast<uint32_t>(tile[0]));
  if (tile[1] < 0 || tile[2] < 0 || tile[1] >= cells || tile[2] >= cells) { return {}; }
  return tile;
}
}

int main(int argc, char *const *argv) {
  if (argc != 6) {
    std::println(stderr, "usage: native_transport_export INPUT_MVT Z X Y NEW_OUTPUT_DIRECTORY");
    return 2;
  }
  const auto tile = Tile(argv);
  if (!tile) { return 2; }
  std::ifstream input(argv[1], std::ios::binary);
  if (!input) { return 3; }
  const std::vector<uint8_t> raw{std::istreambuf_iterator<char>(input), {}};
  if (input.bad()) { return 3; }
  Generators::GroundMaterialCatalog materials;
  Generators::RegionSurfaceRecipes recipes;
  if (!materials.Load("data/world/ground-materials.json") ||
      !recipes.Load("data/world/vegetation.json", materials)) {
    return 4;
  }
  const std::array<std::string, 1> layers{"streets"};
  OsmField field((*tile)[0], layers, MvtSchema::OpenMapTiles);
  if (!field.Accept((*tile)[1], (*tile)[2], raw)) { return 5; }
  StreetField streets;
  for (size_t pass = 0; pass < field.Tiles().size() && !streets.Ingested(field); ++pass) {
    (void)streets.Ingest(field, recipes);
  }
  if (!streets.Ingested(field)) { return 6; }
  const auto graph = StreetGraphBuilder::BuildOneShot(
      streets, field.Points(), [](LongitudeLatitude) -> std::optional<double> { return {}; });
  if (!graph.Graph) { return 7; }
  const double cells = std::ldexp(1.0, (*tile)[0]);
  const auto latitude = [cells](int y) {
    return std::atan(std::sinh(std::numbers::pi * (1.0 - 2.0 * y / cells))) * 180.0 /
           std::numbers::pi;
  };
  const std::array<GeographicBounds, 1> coverage{
      {{.WestDeg = (*tile)[1] / cells * 360.0 - 180.0,
        .SouthDeg = latitude((*tile)[2] + 1),
        .EastDeg = ((*tile)[1] + 1) / cells * 360.0 - 180.0,
        .NorthDeg = latitude((*tile)[2])}}};
  const auto made =
      BuildMvtTransportSnapshot(field,
                                streets,
                                *graph.Graph,
                                coverage,
                                {},
                                {.NodeGroupPayloadBytesMost = size_t{16} * 1024 * 1024,
                                 .SourceFactsPayloadBytesMost = size_t{64} * 1024 * 1024});
  if (!made || !made->SourceFacts) {
    if (!made) { std::println(stderr, "{}", made.error()); }
    return 8;
  }
  if (made->SourceOrigin != field.ShareOriginToken() ||
      made->SourceGeneration != field.Generation() ||
      made->SourceWayCount != streets.Ways().size()) {
    return 9;
  }
  return WriteCapture(argv[5], *made, field, streets, *tile) ? 0 : 10;
}

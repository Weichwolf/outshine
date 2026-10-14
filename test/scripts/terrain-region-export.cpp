#include "content/AssetCache.h"
#include "GroundRegionAsset.h"
#include "BinaryValueArchive.h"
#include "TerrainPage.h"
#include "PreparedTerrainCodec.h"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <print>
#include <string>

namespace {
int ExportField(const outshine::CachedAsset &asset, const char *destination) {
  using namespace outshine;
  BinaryValueReader header(asset.Bytes());
  uint32_t format = 0;
  Data::TileId tile;
  if (!header(format, tile.Zoom, tile.X, tile.Y) || format != 0x31465450u) { return 2; }
  const auto field =
      Generators::DecodePreparedTerrain(tile, asset.Bytes(), Generators::kPreparedTerrainBytesMost);
  if (!field) { return 2; }
  std::ofstream output(destination, std::ios::binary);
  output.write(reinterpret_cast<const char *>(field->Data()),
               static_cast<std::streamsize>(field->Rows()) * field->Cols() * sizeof(float));
  output.flush();
  if (!output) { return 2; }
  std::println("{} {} {} {} {}", tile.Zoom, tile.X, tile.Y, field->Rows(), field->Cols());
  return 0;
}
}

int main(int argc, const char **argv) {
  using namespace outshine;
  if (argc != 4) { return 2; }
  const auto cache = AssetCache::Open(argv[1]);
  if (!cache) { return 2; }
  const auto loaded = (*cache)->Load(argv[2], size_t{512} * 1024 * 1024);
  if (!loaded || !*loaded) { return 2; }
  if ((**loaded).Record().Kind == "terrain-field") { return ExportField(**loaded, argv[3]); }
  if ((**loaded).Record().Kind != "ground-region") { return 2; }
  BinaryValueReader input((**loaded).Bytes());
  uint64_t version = 0;
  uint64_t size = 0;
  if (!input(version, size) || version != 0x35524e474fULL || size > input.In.Remaining()) {
    return 2;
  }
  const auto bytes = input.In.Take(static_cast<size_t>(size));
  if (!bytes) { return 2; }
  if (std::string(argv[3]) == "-") {
    BinaryValueReader header(*bytes);
    uint64_t format = 0;
    double longitude = 0;
    double latitude = 0;
    if (!header(format, longitude, latitude) || format != 0x0002314e5247ULL) { return 2; }
    std::println("{:.17g} {:.17g}", latitude, longitude);
    return 0;
  }
  const auto region = DecodeGroundRegionAsset(*bytes, size_t{512} * 1024 * 1024);
  if (!region) { return 2; }
  const std::filesystem::path output(argv[3]);
  std::error_code error;
  std::filesystem::create_directories(output, error);
  if (error) { return 2; }
  std::ofstream metadata(output / "pages.csv");
  if (!metadata) { return 2; }
  metadata << "page,zoom,x,y,side,postings,virtual,source_zoom,halo,nodes\n";
  for (size_t at = 0; at < region->Terrain.Sheets.size(); ++at) {
    const auto &page = region->Terrain.Sheets[at];
    const auto side = static_cast<size_t>(page.Side);
    const int halo = page.Nodes.size() == side * side ? 0 : 1;
    const Generators::TerrainPageLayout layout{.Side = page.Side, .Halo = halo};
    if (!layout.Valid() || page.Nodes.size() != layout.NodeCount()) { return 2; }
    std::ofstream heights(output / (std::to_string(at) + ".f32"), std::ios::binary);
    heights.write(reinterpret_cast<const char *>(page.Nodes.data()),
                  static_cast<std::streamsize>(page.Nodes.size() * sizeof(float)));
    heights.flush();
    if (!heights) { return 2; }
    std::ofstream fractions(output / (std::to_string(at) + ".fractions"));
    for (int node = 0; node < page.Side; ++node) {
      fractions << std::format("{:.17g}\n", layout.FractionAt(page, node));
    }
    fractions.flush();
    if (!fractions) { return 2; }
    metadata << std::format("{},{},{},{},{},{},{},{},{},{}\n",
                            at,
                            page.Tile.Zoom,
                            page.Tile.X,
                            page.Tile.Y,
                            page.Side,
                            page.Postings,
                            page.Virtual ? 1 : 0,
                            page.SourceZoom,
                            halo,
                            page.Nodes.size());
  }
  metadata.flush();
  if (!metadata) { return 2; }
  std::println("{:.17g} {:.17g} {}",
               region->Anchor.LatitudeDeg,
               region->Anchor.LongitudeDeg,
               region->Terrain.Sheets.size());
  return 0;
}

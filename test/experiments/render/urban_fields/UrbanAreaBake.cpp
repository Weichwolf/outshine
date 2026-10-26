#include "UrbanArea.h"
#include "UrbanAreaCodec.h"
#include "UrbanAreaAsset.h"
#include "OsmField.h"
#include "Check.h"
#include "AssetGeneration.h"
#include <array>
#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <expected>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>
using namespace outshine;
using namespace outshine::Generators::Osm;

namespace {
using Clock = std::chrono::steady_clock;

struct Work {
  size_t Cooks = 0, LotTests = 0, PatchTests = 0, ReadBytes = 0;
  double CookMs = 0;
};

int Number(const char *text) {
  int value = -1;
  const std::string_view token(text);
  const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
  CHECK(parsed.ec == std::errc{} && parsed.ptr == token.data() + token.size(),
        "tile integer parses");
  return value;
}

std::vector<uint8_t> Read(const char *path) {
  std::ifstream in(path, std::ios::binary | std::ios::ate);
  const auto length = static_cast<std::streamoff>(in.tellg());
  CHECK(length > 0 && length <= 8 * 1024 * 1024, "source fits byte budget");
  if (length <= 0 || length > 8 * 1024 * 1024) { return {}; }
  std::vector<uint8_t> bytes(static_cast<size_t>(length));
  in.seekg(0);
  in.read(reinterpret_cast<char *>(bytes.data()), length);
  CHECK(in.good(), "source reads fully");
  return bytes;
}

std::expected<UrbanArea, std::string> Cook(char **argv, Work &work) {
  const std::array<std::string, 7> layers{
      "sites", "land", "park", "water_polygons", "water_lines", "streets", "aeroway"};
  const std::array<std::string, 1> parentLayers{"sites"};
  OsmField owner(Number(argv[2]), layers, MvtSchema::OpenMapTiles);
  OsmField parent(Number(argv[6]), parentLayers, MvtSchema::OpenMapTiles);
  const auto first = owner.Accept(Number(argv[3]), Number(argv[4]), Read(argv[1]));
  const auto second = parent.Accept(Number(argv[7]), Number(argv[8]), Read(argv[5]));
  if (!first || !second) { return std::unexpected("source decode failed"); }
  const std::array<const OsmField *, 2> inputs{&owner, &parent};
  const auto began = Clock::now();
  auto area = GenerateUrbanArea(inputs);
  work.CookMs = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
  ++work.Cooks;
  if (area) {
    work.LotTests = area->LotTests;
    work.PatchTests = area->PatchTests;
  }
  return area;
}

std::expected<UrbanArea, std::string> Cached(char **argv, Work &work) {
  auto cache = AssetCache::Open(argv[10]);
  if (!cache) { return std::unexpected("asset cache open failed"); }
  const std::string request(argv[11]);
  const auto loaded =
      ResolveAssetRequest(**cache,
                          request,
                          kUrbanAreaBytesMost,
                          [&](size_t budget) -> std::expected<GeneratedAssetPackage, std::string> {
                            auto area = Cook(argv, work);
                            if (!area) { return std::unexpected(area.error()); }
                            return PackageUrbanArea(*area, request, budget);
                          });
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (loaded->Record().Kind != "urban-area") {
    return std::unexpected("unexpected native asset kind");
  }
  work.ReadBytes = loaded->Bytes().size();
  return DecodeUrbanArea(loaded->Bytes());
}
}

int main(int argc, char **argv) {
  CHECK(argc == 10 || argc == 12,
        "owner/parent tiles and output, optionally database/request key supplied");
  if (argc != 10 && argc != 12) { return 1; }
  Work work;
  const auto began = Clock::now();
  const auto area = argc == 12 ? Cached(argv, work) : Cook(argv, work);
  const auto totalMs = std::chrono::duration<double, std::milli>(Clock::now() - began).count();
  CHECK(area, "native area request completes");
  if (!area) {
    std::cerr << area.error() << '\n';
    return 1;
  }
  std::ofstream output(argv[9], std::ios::binary);
  output.write(reinterpret_cast<const char *>(area->Tiers.data()),
               static_cast<std::streamsize>(area->Tiers.size()));
  CHECK(output.good(), "canonical native tiers saved");
  std::cout << std::setprecision(17) << "AREA " << area->Columns << ' ' << area->Rows << ' '
            << area->Occupied << ' ' << work.LotTests << ' ' << work.PatchTests << ' '
            << work.CookMs << '\n';
  std::cout << "CACHE enabled=" << (argc == 12) << " cooks=" << work.Cooks
            << " read_bytes=" << work.ReadBytes << " total_ms=" << totalMs << '\n';
  const auto occupied = std::ranges::find_if(area->Tiers, [](uint8_t tier) { return tier != 0; });
  if (occupied != area->Tiers.end()) {
    const auto index = static_cast<size_t>(occupied - area->Tiers.begin());
    const auto lot = area->LotAt(static_cast<uint32_t>(index % area->Columns),
                                 static_cast<uint32_t>(index / area->Columns));
    CHECK(lot, "occupied stored cell has a geographic lot");
    if (lot) {
      std::cout << "LOT " << lot->Centre.LongitudeDeg << ' ' << lot->Centre.LatitudeDeg << ' '
                << lot->EastSpanM << ' ' << lot->NorthSpanM << ' ' << lot->HeightM << '\n';
    }
  }
  return outshine::Test::Report();
}

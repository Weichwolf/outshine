#include "UrbanArea.h"
#include "OsmField.h"
#include "Check.h"
#include <array>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <vector>
using namespace outshine::Generators::Osm;

namespace {
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
}

int main(int argc, char **argv) {
  CHECK(argc == 10, "owner and parent source tiles plus output supplied");
  if (argc != 10) { return 1; }
  const std::array<std::string, 7> layers{
      "sites", "land", "park", "water_polygons", "water_lines", "streets", "aeroway"};
  const std::array<std::string, 1> parentLayers{"sites"};
  OsmField owner(Number(argv[2]), layers, MvtSchema::OpenMapTiles);
  OsmField parent(Number(argv[6]), parentLayers, MvtSchema::OpenMapTiles);
  auto first = owner.Accept(Number(argv[3]), Number(argv[4]), Read(argv[1]));
  auto second = parent.Accept(Number(argv[7]), Number(argv[8]), Read(argv[5]));
  CHECK(first.has_value() && second.has_value(),
        "existing native source decoder accepts both inputs");
  if (!first || !second) { return 1; }
  const std::array<const OsmField *, 2> inputs{&owner, &parent};
  const auto began = std::chrono::steady_clock::now();
  auto area = GenerateUrbanArea(inputs);
  const auto ms =
      std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - began).count();
  CHECK(area.has_value(), "native coarse-area recipe completes");
  if (!area) {
    std::cerr << area.error() << '\n';
    return 1;
  }
  std::ofstream output(argv[9], std::ios::binary);
  output.write(reinterpret_cast<const char *>(area->Tiers.data()),
               static_cast<std::streamsize>(area->Tiers.size()));
  CHECK(output.good(), "canonical native tiers saved");
  std::cout << "AREA " << area->Columns << ' ' << area->Rows << ' ' << area->Occupied << ' '
            << area->LotTests << ' ' << area->PatchTests << ' ' << ms << '\n';
  return outshine::Test::Report();
}

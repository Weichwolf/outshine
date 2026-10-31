#include "OsmField.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "MvtWireFixture.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iterator>
#include <ios>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace {
using namespace outshine::Generators::Osm;
using namespace outshine::Test;

void CheckBuild(std::span<const uint8_t> input,
                std::span<const std::string> names,
                int zoom,
                TileAt at) {
  BeginCppAllocationProbe();
  const auto plan = OsmField::NativeTileBuildBytes(input, names, MvtSchema::OpenMapTiles);
  const auto scan = EndCppAllocationProbe();
  CHECK(plan && scan.Calls == 0, "native generation is sized without allocating any product");
  if (!plan) { return; }
  BeginCppAllocationProbe();
  auto field = std::make_unique<OsmField>(zoom, names, MvtSchema::OpenMapTiles);
  const auto accepted = field->Accept(at.X, at.Y, input);
  const auto build = EndCppAllocationProbe();
  CHECK(accepted && field->HeapBytes() + sizeof(OsmField) <= plan->WorkingBytesMost,
        "the actual fresh field and retained parsed/normalized storage fit the build bound");
  CHECK(build.Bytes <= plan->WorkingBytesMost,
        "even cumulative allocation requests of this complete fresh fixture fit its bound");
  if (!accepted) { return; }
  constexpr size_t packageMost = size_t{64} * 1024 * 1024;
  BeginCppAllocationProbe();
  const auto bytes = field->NativeTileEncodedBytes(packageMost);
  const auto counted = EndCppAllocationProbe();
  CHECK(bytes && counted.Calls == 0, "the real codec measures its exact output without allocation");
  if (!bytes) { return; }
  CHECK(*bytes <= plan->EncodedBytesMost + 6,
        "the exact packet fits its structural upper bound including the direct source name");
  BeginCppAllocationProbe();
  const auto tooSmall = field->EncodeNativeTile(*bytes - 1);
  const auto refusal = EndCppAllocationProbe();
  CHECK(!tooSmall && refusal.Calls == 0,
        "insufficient encoding admission is rejected before growing an output vector");
  BeginCppAllocationProbe();
  const auto encoded = field->EncodeNativeTile(*bytes);
  const auto encoding = EndCppAllocationProbe();
  CHECK(encoded && encoded->size() == *bytes && encoded->capacity() == *bytes &&
            encoding.Calls == 1 && encoding.Bytes == *bytes,
        "the measured codec allocates its exact output once without old/new growth overlap");
}

void MalformedInput() {
  const std::array<std::string, 1> names{"buildings"};
  auto bytes = Mvt::BuildingTile();
  bytes.pop_back();
  BeginCppAllocationProbe();
  const auto plan = OsmField::NativeTileBuildBytes(bytes, names, MvtSchema::OpenMapTiles);
  const auto allocations = EndCppAllocationProbe();
  CHECK(!plan && allocations.Calls == 0,
        "a truncated source fails native build planning before construction or decoding");
}
}

int main() {
  std::vector<std::string> names{"buildings"};
  for (const size_t length : std::array<size_t, 6>{0, 22, 23, 127, 128, 4096}) {
    names.emplace_back(length, 'x');
  }
  CheckBuild(Mvt::BuildingTile(), names, 2, {.X = 1, .Y = 1});
  std::ifstream source("test/outshine/data/transport/wien-junction-14-8937-5678.pbf",
                       std::ios::binary);
  const std::vector<uint8_t> real{std::istreambuf_iterator<char>(source), {}};
  CHECK(!real.empty(), "the versioned real-world MVT input is available");
  const std::array<std::string, 6> actual{
      "buildings", "streets", "water_polygons", "water_lines", "land", "sites"};
  CheckBuild(real, actual, 14, {.X = 8937, .Y = 5678});
  MalformedInput();
  return Report();
}

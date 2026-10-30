#include "OsmField.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include "MvtWireFixture.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Generators::Osm;
using namespace outshine::Test;
constexpr size_t kMost = size_t{1024} * 1024;

void RejectWithoutStorage(std::span<const uint8_t> bytes) {
  BeginCppAllocationProbe();
  const auto required = OsmField::NativeTileDecodeBytes(bytes);
  const auto scan = EndCppAllocationProbe();
  CHECK(!required && scan.Calls == 0 && scan.Bytes == 0,
        "structurally invalid packages are rejected by the allocation-free size scan");
  BeginCppAllocationProbe();
  const auto decoded = OsmField::DecodeNativeTile(bytes, kMost);
  const auto decode = EndCppAllocationProbe();
  CHECK(!decoded && decode.Calls == 0 && decode.Bytes == 0,
        "invalid structure allocates no field, shared tokens, strings or payload vectors");
}

void CheckStorage(std::span<const uint8_t> bytes) {
  BeginCppAllocationProbe();
  const auto required = OsmField::NativeTileDecodeBytes(bytes);
  const auto scan = EndCppAllocationProbe();
  CHECK(required && scan.Calls == 0 && scan.Bytes == 0,
        "size admission scans native lengths and counts without allocating product storage");
  if (!required) { return; }
  CHECK(*required > bytes.size(), "decoded admission includes native objects and string capacity");
  BeginCppAllocationProbe();
  const auto limited = OsmField::NativeTileDecodeBytes(bytes, *required - 1);
  const auto limitedScan = EndCppAllocationProbe();
  CHECK(!limited && limitedScan.Calls == 0 && limitedScan.Bytes == 0,
        "the size scan also stops without allocation when its caller's storage limit is exceeded");
  BeginCppAllocationProbe();
  const auto refused = OsmField::DecodeNativeTile(bytes, *required - 1);
  const auto refusal = EndCppAllocationProbe();
  CHECK(!refused && refusal.Calls == 0 && refusal.Bytes == 0,
        "a budget below admission is rejected before any decoder allocation");
  BeginCppAllocationProbe();
  const auto decoded = OsmField::DecodeNativeTile(bytes, *required);
  const auto allocation = EndCppAllocationProbe();
  CHECK(decoded && allocation.Bytes <= *required && allocation.Calls > 0,
        "the admitted fresh decoder's complete allocation requests fit its declared bound");
  if (!decoded) { return; }
  CHECK(decoded->HeapBytes() <= *required && decoded->Features().size() == 1,
        "the real normalized product fits admission and retains its geometry");
  const auto encoded = decoded->EncodeNativeTile(kMost);
  CHECK(encoded && std::ranges::equal(*encoded, bytes),
        "preflight preserves the exact native representation and source semantics");
}

void DamagedStructure(std::vector<uint8_t> bytes) {
  const std::array<size_t, 6> lengths{0, 3, 4, 23, bytes.size() / 2, bytes.size() - 1};
  for (const auto length : lengths) { RejectWithoutStorage(std::span(bytes).first(length)); }
  bytes.push_back(0);
  RejectWithoutStorage(bytes);
  bytes.pop_back();
  bytes.front() ^= 1;
  RejectWithoutStorage(bytes);
  bytes.front() ^= 1;
  constexpr size_t featureTailBytes = 32 + 4 * sizeof(double) + sizeof(uint8_t);
  const size_t digestPresentAt = bytes.size() - featureTailBytes;
  const uint8_t present = bytes[digestPresentAt];
  bytes[digestPresentAt] = 2;
  RejectWithoutStorage(bytes);
  bytes[digestPresentAt] = present;
  const std::string_view firstLayer = "buildings";
  const auto layer = std::ranges::search(bytes, firstLayer);
  const size_t offset = static_cast<size_t>(layer.begin() - bytes.begin());
  CHECK(!layer.empty() && offset >= sizeof(uint32_t), "the first layer's native length is located");
  if (layer.empty() || offset < sizeof(uint32_t)) { return; }
  std::fill(layer.begin() - sizeof(uint32_t), layer.begin(), std::numeric_limits<uint8_t>::max());
  RejectWithoutStorage(bytes);
}
}

int main() {
  std::vector<std::string> layers{"buildings"};
  for (const size_t length : {0, 1, 15, 16, 22, 23, 31, 32, 63, 64, 127, 128, 255, 256, 4096}) {
    layers.emplace_back(length, 'x');
  }
  OsmField cold(2, layers, MvtSchema::OpenMapTiles);
  CHECK(cold.Accept(1, 1, Mvt::BuildingTile()), "a real MVT produces a normalized native tile");
  const auto encoded = cold.EncodeNativeTile(kMost);
  CHECK(encoded && !encoded->empty(), "native encoding retains short and long layer names");
  if (!encoded) { return Report(); }
  CheckStorage(*encoded);
  DamagedStructure(*encoded);
  return Report();
}

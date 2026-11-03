#include "GroundRegionAsset.h"
#include "BinaryValueArchive.h"
#include "Check.h"
#include "CppAllocationProbe.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <print>
#include <span>
#include <utility>
#include <vector>

namespace {
using namespace outshine;
using namespace outshine::Test;
constexpr size_t kLargeBytes = size_t{512} * 1024;

GroundRegionAsset Product() {
  GroundRegionAsset region;
  auto surface = region.Surfaces.addSurface("ground", Material{});
  CHECK(surface, "the product owns a valid material");
  if (!surface) { return region; }
  region.GroundSurface = surface->index();
  const auto part = region.Surfaces.addPart("ground", *surface);
  CHECK(part, "the product owns a mesh");
  if (!part) { return region; }
  std::vector<float> positions(size_t{131072} * 3, 0);
  CHECK(region.Surfaces.adoptPositions(*part, positions) &&
            region.Surfaces.setTriangles(*part, std::array<uint32_t, 3>{0, 1, 2}),
        "large native geometry is prepared before encoding");
  auto network = Path::Network::Create({.CellM = 1}, {});
  CHECK(network, "the product owns a functional network");
  if (!network) { return region; }
  const std::array<double, 4> route{47, 9, 47.001, 9.001};
  std::string error;
  CHECK(network->Lay(route, {.HalfWidthM = 3, .SpeedMps = 10}) && network->Weave(error),
        "native routing is finalized before encoding");
  const auto elevated = network->Elevate([](LongitudeLatitude) { return std::optional(0.0); });
  CHECK(elevated.Points == 2 && elevated.Refused == 0, "functional height profiles are complete");
  region.Network = std::make_shared<const Path::Network>(std::move(*network));
  region.NetworkWays = 1;
  return region;
}
}

int main() {
  const auto region = Product();
  const auto standalone = EncodeGroundRegionAsset(region, size_t{8} * 1024 * 1024);
  CHECK(standalone, "the standalone product encodes");
  if (!standalone) { return Report(); }
  constexpr uint8_t kPrefix = 0xa5;
  BinaryValueWriter out(standalone->size() + 1);
  out.Out.Reserve(standalone->size() + 1);
  CHECK(out(kPrefix), "the caller owns the outer packet prefix");
  outshine::Test::BeginCppAllocationProbe(kLargeBytes);
  const bool encoded = EncodeGroundRegionAsset(region, out);
  const auto allocations = outshine::Test::EndCppAllocationProbe();
  CHECK(encoded && allocations.Calls == 0,
        "composed encoding uses the reserved destination without large subrecord allocations");
  CHECK(out.Out.Bytes().front() == kPrefix &&
            std::ranges::equal(out.Out.Bytes().subspan(1), *standalone),
        "composition preserves the prefix and every standalone format byte");
  auto restored = DecodeGroundRegionAsset(out.Out.Bytes().subspan(1), standalone->size());
  CHECK(restored && restored->Network && restored->Network->WayCount() == 1 &&
            EncodeGroundRegionAsset(*restored, standalone->size()) == standalone,
        "the network and geometry survive an exact-bound round trip");
  const auto networkBytes = region.Network->EncodeAsset(standalone->size());
  CHECK(networkBytes, "the functional subrecord encodes independently");
  if (networkBytes) {
    const auto range = std::ranges::search(*standalone, *networkBytes);
    CHECK(!range.empty() && range.begin() - standalone->begin() >= 4,
          "the network retains its existing length-prefixed record");
    if (!range.empty() && range.begin() - standalone->begin() >= 4) {
      auto invalid = *standalone;
      const auto first = static_cast<size_t>(range.begin() - standalone->begin());
      std::fill_n(invalid.begin() + static_cast<ptrdiff_t>(first - 4), 4, uint8_t{255});
      CHECK(!DecodeGroundRegionAsset(invalid, invalid.size()),
            "an oversized borrowed subrecord is rejected before network decoding");
    }
  }
  BinaryValueWriter shortOutput(standalone->size());
  CHECK(shortOutput(kPrefix) && !EncodeGroundRegionAsset(region, shortOutput),
        "the prefix consumes the same byte budget as every nested product");
  std::println("ground composition bytes={} large_scratch_allocations={}",
               standalone->size(),
               allocations.Calls);
  return Report();
}

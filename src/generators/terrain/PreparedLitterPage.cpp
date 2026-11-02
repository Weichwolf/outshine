#include "PreparedLitterPage.h"
#include "LitterPageNoise.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint32_t kFormat = 0x32504c52;

uint32_t Parameters(LitterPageAddress address) noexcept {
  return address.Level | (address.CoreTexels << 8u);
}

bool Header(ByteWriter &out, LitterPageAddress address) {
  return PlanLitterPage(address) && out.Number(kFormat) && out.Number(Parameters(address)) &&
         out.Number(address.Tile[0]) && out.Number(address.Tile[1]);
}
}

std::expected<std::string, std::string> LitterPageKey(LitterPageAddress address) {
  ByteWriter header(kLitterPageHeaderBytes);
  header.Reserve(kLitterPageHeaderBytes);
  if (!Header(header, address)) { return std::unexpected("invalid litter field address"); }
  return Sha256Hex(header.Bytes().data(), header.Bytes().size());
}

std::expected<GeneratedAssetPackage, std::string> PrepareLitterPage(
    LitterPageAddress address, size_t packageBytesMost, const std::stop_token &stopping) {
  const auto layout = PlanLitterPage(address);
  if (!layout || stopping.stop_requested()) {
    return std::unexpected("invalid or cancelled litter field request");
  }
  const size_t bytes = kLitterPageHeaderBytes + size_t{layout->Side} * layout->Side;
  if (bytes > packageBytesMost) {
    return std::unexpected("litter field exceeds package admission");
  }
  auto key = LitterPageKey(address);
  if (!key) { return std::unexpected(std::move(key.error())); }
  ByteWriter header(bytes);
  header.Reserve(bytes);
  if (!Header(header, address)) { return std::unexpected("litter header could not be encoded"); }
  GeneratedAssetPackage package;
  package.Bytes = std::move(header).TakeBytes();
  package.Bytes.resize(bytes);
  auto filled = GenerateLitterPage(
      address, std::span(package.Bytes).subspan(kLitterPageHeaderBytes), stopping);
  if (!filled) { return std::unexpected(std::move(filled.error())); }
  std::string parent;
  if (const auto parentAddress = LitterPageParent(address)) {
    auto above = LitterPageKey(*parentAddress);
    if (!above) { return std::unexpected(std::move(above.error())); }
    parent = std::move(*above);
  }
  const double step = static_cast<double>(uint32_t{1} << address.Level) / 16;
  const double x = (static_cast<double>(address.Tile[0]) * address.CoreTexels - 1) * step;
  const double y = (static_cast<double>(address.Tile[1]) * address.CoreTexels - 1) * step;
  const double extent = static_cast<double>(layout->Side) * step;
  package.Records.push_back({.Key = std::move(*key),
                             .Kind = "ground-litter-page-r8",
                             .Bounds = {.Min = {{x, 0, y}}, .Max = {{x + extent, 0, y + extent}}},
                             .Package = {},
                             .OffsetBytes = 0,
                             .ByteCount = bytes,
                             .Level = address.Level,
                             .Parent = std::move(parent)});
  return package;
}

std::expected<LitterPageView, std::string> DecodeLitterPage(LitterPageAddress address,
                                                            std::span<const uint8_t> package) {
  const auto layout = PlanLitterPage(address);
  if (!layout || package.size() != kLitterPageHeaderBytes + size_t{layout->Side} * layout->Side) {
    return std::unexpected("litter package has an invalid address or size");
  }
  ByteReader in(package);
  uint32_t format = 0;
  uint32_t parameters = 0;
  std::array<int32_t, 2> tile{};
  if (!in.Number(format) || format != kFormat || !in.Number(parameters) ||
      parameters != Parameters(address) || !in.Number(tile[0]) || !in.Number(tile[1]) ||
      tile != address.Tile) {
    return std::unexpected("litter package does not match its requested identity");
  }
  auto pixels = in.Take(size_t{layout->Side} * layout->Side);
  if (!pixels || in.Remaining() != 0) {
    return std::unexpected("litter pixel extent is incomplete");
  }
  return LitterPageView{.Side = layout->Side, .Pixels = *pixels};
}
}

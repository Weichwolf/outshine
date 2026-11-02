#include "PreparedLitterField.h"
#include "LitterFieldNoise.h"
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
constexpr uint32_t kFormat = 0x31464c52;

bool Header(ByteWriter &out, LitterFieldAddress address) {
  return LitterMipSide(address.Tile, address.Level) && out.Number(kFormat) &&
         out.Number(address.Level) && out.Number(address.Tile[0]) && out.Number(address.Tile[1]);
}
}

std::expected<std::string, std::string> LitterFieldKey(LitterFieldAddress address) {
  ByteWriter header(kLitterFieldHeaderBytes);
  header.Reserve(kLitterFieldHeaderBytes);
  if (!Header(header, address)) { return std::unexpected("invalid litter field address"); }
  return Sha256Hex(header.Bytes().data(), header.Bytes().size());
}

std::expected<GeneratedAssetPackage, std::string> PrepareLitterField(
    LitterFieldAddress address, size_t packageBytesMost, const std::stop_token &stopping) {
  const auto side = LitterMipSide(address.Tile, address.Level);
  if (!side || stopping.stop_requested()) {
    return std::unexpected("invalid or cancelled litter field request");
  }
  const size_t bytes = kLitterFieldHeaderBytes + size_t{*side} * *side;
  if (bytes > packageBytesMost) {
    return std::unexpected("litter field exceeds package admission");
  }
  auto key = LitterFieldKey(address);
  if (!key) { return std::unexpected(std::move(key.error())); }
  ByteWriter header(bytes);
  header.Reserve(bytes);
  if (!Header(header, address)) { return std::unexpected("litter header could not be encoded"); }
  GeneratedAssetPackage package;
  package.Bytes = std::move(header).TakeBytes();
  package.Bytes.resize(bytes);
  auto filled = GenerateLitterMip(address.Tile,
                                  address.Level,
                                  std::span(package.Bytes).subspan(kLitterFieldHeaderBytes),
                                  stopping);
  if (!filled) { return std::unexpected(std::move(filled.error())); }
  std::string parent;
  if (address.Level < kLitterMipMax) {
    auto above = LitterFieldKey({.Tile = address.Tile, .Level = address.Level + 1});
    if (!above) { return std::unexpected(std::move(above.error())); }
    parent = std::move(*above);
  }
  const double x = static_cast<double>(address.Tile[0]) * kLitterCoreCells - kLitterHaloCells;
  const double y = static_cast<double>(address.Tile[1]) * kLitterCoreCells - kLitterHaloCells;
  package.Records.push_back(
      {.Key = std::move(*key),
       .Kind = "ground-litter-lattice-r8",
       .Bounds = {.Min = {{x, 0, y}}, .Max = {{x + kLitterCellCount, 0, y + kLitterCellCount}}},
       .Package = {},
       .OffsetBytes = 0,
       .ByteCount = bytes,
       .Level = address.Level,
       .Parent = std::move(parent)});
  return package;
}

std::expected<LitterFieldMipView, std::string> DecodeLitterField(LitterFieldAddress address,
                                                                 std::span<const uint8_t> package) {
  const auto side = LitterMipSide(address.Tile, address.Level);
  if (!side || package.size() != kLitterFieldHeaderBytes + size_t{*side} * *side) {
    return std::unexpected("litter package has an invalid address or size");
  }
  ByteReader in(package);
  uint32_t format = 0;
  uint32_t level = 0;
  std::array<int32_t, 2> tile{};
  if (!in.Number(format) || format != kFormat || !in.Number(level) || level != address.Level ||
      !in.Number(tile[0]) || !in.Number(tile[1]) || tile != address.Tile) {
    return std::unexpected("litter package does not match its requested identity");
  }
  auto pixels = in.Take(size_t{*side} * *side);
  if (!pixels || in.Remaining() != 0) {
    return std::unexpected("litter pixel extent is incomplete");
  }
  return LitterFieldMipView{.Side = *side, .Pixels = *pixels};
}
}

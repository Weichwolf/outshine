#include "GroundTerrainPacks.h"
#include "BinaryValueArchive.h"
#include "GroundRegionAsset.h"
#include "Sha256.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
constexpr std::string_view kKind = "ground-terrain-pack";
constexpr uint32_t kPackWidth = 8;
constexpr uint32_t kSheetsPerPack = kPackWidth * kPackWidth;
constexpr uint32_t kSheetsMost = 65536;
constexpr size_t kKeyLength = 64;
constexpr uint32_t kPackFormat = 1;
using Address = std::tuple<int, uint32_t, uint32_t>;

Address PackAddress(const Data::TileId &tile) {
  return {tile.Zoom, tile.X / kPackWidth, tile.Y / kPackWidth};
}

bool ValidKey(std::string_view key) {
  return key.size() == kKeyLength && std::ranges::all_of(key, [](char byte) {
           return (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'f');
         });
}

std::expected<PreparedAssetWrite, AssetCacheError> PreparePack(AssetCache &cache,
                                                               std::string_view revision,
                                                               const Address &address,
                                                               std::span<const Sheet> sheets,
                                                               size_t bytesMost,
                                                               GroundTerrainManifest::Pack &pack) {
  BinaryValueWriter out(bytesMost);
  const auto [zoom, column, row] = address;
  constexpr size_t kCoverageBytes = (4u + 2u * kSheetsPerPack) * sizeof(uint32_t);
  BinaryValueWriter coverageId(kCoverageBytes);
  if (pack.Slots.size() > kSheetsPerPack ||
      !out(kPackFormat, static_cast<uint32_t>(pack.Slots.size())) ||
      !coverageId(kPackFormat, zoom, column, row)) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  const auto coordinates = [&](uint32_t slot) {
    const auto &tile = sheets[slot].Tile;
    return std::pair(tile.X, tile.Y);
  };
  std::ranges::sort(pack.Slots, {}, coordinates);
  if (std::ranges::adjacent_find(pack.Slots, {}, coordinates) != pack.Slots.end()) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  Box bounds;
  for (const auto slot : pack.Slots) {
    const auto &sheet = sheets[slot];
    if (!coverageId(sheet.Tile.X, sheet.Tile.Y) || !WriteGroundTerrainSheet(out, sheet)) {
      return std::unexpected(AssetCacheError::InvalidInput);
    }
    bounds.Cover(GroundTerrainSheetBoundsEcef(sheet));
  }
  auto bytes = std::move(out.Out).TakeBytes();
  const auto byteCount = bytes.size();
  const auto footprint = coverageId.Out.Bytes();
  auto coverage = Sha256Hex(footprint.data(), footprint.size());
  const auto digest = Sha256Hex(bytes.data(), byteCount);
  pack.Key = Sha256Hex(std::string(revision) + coverage + digest);
  return cache.PreparePublication({{.Key = pack.Key,
                                    .Kind = std::string(kKind),
                                    .Bounds = bounds,
                                    .Package = {},
                                    .ByteCount = byteCount,
                                    .Level = static_cast<uint32_t>(zoom),
                                    .Parent = std::string(revision),
                                    .Tile = AssetTile{.Revision = std::string(revision),
                                                      .Coverage = std::move(coverage),
                                                      .Coarser = {}}}},
                                  std::move(bytes));
}

bool DecodePack(std::span<const uint8_t> bytes,
                std::span<const uint32_t> slots,
                std::vector<Sheet> &sheets) {
  BinaryValueReader in(bytes);
  uint32_t format = 0;
  uint32_t count = 0;
  if (!in(format, count) || format != kPackFormat || count != slots.size()) { return false; }
  for (const auto slot : slots) {
    if (!ReadGroundTerrainSheet(in, sheets[slot])) { return false; }
  }
  return in.In.Remaining() == 0;
}
}

bool GroundTerrainManifest::Write(BinaryValueWriter &out) const {
  if (!out(Sheets, static_cast<uint32_t>(Packs.size()))) { return false; }
  return std::ranges::all_of(Packs, [&](const auto &pack) {
    return out.Text(pack.Key) && out.Array<uint32_t>(pack.Slots);
  });
}

bool GroundTerrainManifest::Read(BinaryValueReader &in) {
  uint32_t count = 0;
  if (!in(Sheets, count) || Sheets == 0 || Sheets > kSheetsMost || count == 0 || count > Sheets ||
      count > in.In.Remaining() / (kKeyLength + 3 * sizeof(uint32_t))) {
    return false;
  }
  Packs.resize(count);
  std::vector<bool> present(Sheets);
  size_t found = 0;
  for (auto &pack : Packs) {
    uint32_t length = 0;
    if (!in(length) || length != kKeyLength) { return false; }
    const auto key = in.In.Take(length);
    if (!key) { return false; }
    pack.Key.assign(reinterpret_cast<const char *>(key->data()), key->size());
    uint32_t slots = 0;
    if (!ValidKey(pack.Key) || !in(slots) || slots == 0 || slots > kSheetsPerPack ||
        slots > in.In.Remaining() / sizeof(uint32_t)) {
      return false;
    }
    pack.Slots.resize(slots);
    for (auto &slot : pack.Slots) {
      if (!in(slot) || slot >= Sheets || present[slot]) { return false; }
      present[slot] = true;
      ++found;
    }
  }
  std::ranges::sort(Packs, {}, &Pack::Key);
  return found == Sheets && std::ranges::adjacent_find(Packs, {}, &Pack::Key) == Packs.end();
}

std::expected<GroundTerrainManifest, AssetCacheError>
PrepareGroundTerrainPacks(AssetCache &cache,
                          std::string_view revision,
                          std::span<const Sheet> sheets,
                          size_t bytesMost,
                          std::vector<PreparedAssetWrite> &into) {
  if (sheets.empty() || sheets.size() > kSheetsMost) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  std::map<Address, std::vector<uint32_t>> slots;
  for (uint32_t slot = 0; slot < sheets.size(); ++slot) {
    slots[PackAddress(sheets[slot].Tile)].push_back(slot);
  }
  GroundTerrainManifest manifest{.Sheets = static_cast<uint32_t>(sheets.size()), .Packs = {}};
  manifest.Packs.reserve(slots.size());
  into.reserve(slots.size() + 1);
  for (auto &[address, indices] : slots) {
    auto &entry = manifest.Packs.emplace_back(
        GroundTerrainManifest::Pack{.Key = {}, .Slots = std::move(indices)});
    auto pack = PreparePack(cache, revision, address, sheets, bytesMost, entry);
    if (!pack) { return std::unexpected(pack.error()); }
    bytesMost -= pack->NativeBytes();
    into.push_back(std::move(*pack));
  }
  return manifest;
}

std::expected<bool, AssetCacheError>
RestoreGroundTerrainPacks(const AssetCache &cache,
                          const AssetRecord &region,
                          const GroundTerrainManifest &manifest,
                          size_t bytesMost,
                          std::vector<Sheet> &sheets,
                          size_t &readBytes,
                          const std::stop_token &stop) {
  const auto index = cache.Select({.Bounds = region.Bounds,
                                   .Kind = kKind,
                                   .Level = {},
                                   .Radius = {},
                                   .Planes = {},
                                   .Parent = region.Key,
                                   .TileRevision = region.Key});
  if (!index) { return std::unexpected(index.error()); }
  using Entry = GroundTerrainManifest::Pack;
  std::vector<std::pair<const AssetRecord *, const Entry *>> selected;
  selected.reserve(manifest.Packs.size());
  for (const auto &pack : *index) {
    const auto entry = std::ranges::lower_bound(manifest.Packs, pack.Key, {}, &Entry::Key);
    if (entry != manifest.Packs.end() && entry->Key == pack.Key) {
      selected.emplace_back(&pack, &*entry);
    }
  }
  if (selected.size() != manifest.Packs.size()) { return false; }
  sheets.resize(manifest.Sheets);
  for (const auto &[record, entry] : selected) {
    if (stop.stop_requested() || record->ByteCount > bytesMost) { return false; }
    const auto pack = cache.Load(record->Key, bytesMost);
    if (!pack) { return std::unexpected(pack.error()); }
    if (!*pack || (**pack).Record().Package != record->Package ||
        (**pack).Record().ByteCount != record->ByteCount ||
        (**pack).Record().OffsetBytes != record->OffsetBytes) {
      return false;
    }
    const auto bytes = (**pack).Bytes();
    readBytes += bytes.size();
    bytesMost -= bytes.size();
    if (!DecodePack(bytes, entry->Slots, sheets)) { return false; }
  }
  return true;
}
}

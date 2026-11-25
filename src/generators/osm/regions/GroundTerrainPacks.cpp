#include "GroundTerrainPacks.h"
#include "GroundTerrainPrecision.h"
#include "BinaryValueArchive.h"
#include "GroundRegionAsset.h"
#include "Sha256.h"
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
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
constexpr std::string_view kBaseKind = "ground-terrain-base";
constexpr std::string_view kDetailKind = "ground-terrain-detail";
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

std::string PackKey(std::string_view revision) {
  std::string key("ground-terrain-pack:");
  key += revision;
  return Sha256Hex(key);
}

std::string BaseKey(std::string_view revision) {
  std::string key("ground-terrain-base:");
  key += revision;
  return Sha256Hex(key);
}

std::expected<bool, AssetCacheError> PreparePrecision(AssetCache &cache,
                                                      const AssetRecord &record,
                                                      GroundTerrainPrecision precision,
                                                      size_t exactStoredBytes,
                                                      std::vector<PreparedAssetWrite> &into) {
  if (!record.Tile) { return std::unexpected(AssetCacheError::InvalidInput); }
  const auto &tile = *record.Tile;
  AssetRecord base = record;
  base.Key = BaseKey(tile.Revision);
  base.Kind = kBaseKind;
  base.ByteCount = precision.Base.size();
  base.Tile = AssetTile{.Revision = tile.Revision,
                        .Coverage = tile.Coverage,
                        .Coarser = {},
                        .ErrorM = precision.ErrorM};
  const auto baseKey = base.Key;
  auto compact = cache.PreparePublication({std::move(base)}, std::move(precision.Base));
  if (!compact) { return std::unexpected(compact.error()); }
  if (compact->StoredBytes() >= exactStoredBytes) { return false; }
  AssetRecord detail = record;
  detail.Kind = kDetailKind;
  detail.ByteCount = precision.Residual.size();
  detail.Tile = AssetTile{
      .Revision = tile.Revision, .Coverage = tile.Coverage, .Coarser = baseKey, .ErrorM = 0};
  auto residual = cache.PreparePublication({std::move(detail)}, std::move(precision.Residual));
  if (!residual) { return std::unexpected(residual.error()); }
  if (compact->StoredBytes() + residual->StoredBytes() > exactStoredBytes) { return false; }
  into.push_back(std::move(*compact));
  into.push_back(std::move(*residual));
  return true;
}

std::expected<void, AssetCacheError> PreparePack(AssetCache &cache,
                                                 const Address &address,
                                                 std::span<const Sheet> sheets,
                                                 size_t bytesMost,
                                                 GroundTerrainManifest::Pack &pack,
                                                 std::vector<PreparedAssetWrite> &into) {
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
  auto revision = Sha256Hex(bytes.data(), byteCount);
  pack.Key = PackKey(revision);
  const AssetRecord record{.Key = pack.Key,
                           .Kind = std::string(kKind),
                           .Bounds = bounds,
                           .Package = {},
                           .ByteCount = byteCount,
                           .Level = static_cast<uint32_t>(zoom),
                           .Parent = {},
                           .Tile = AssetTile{.Revision = std::move(revision),
                                             .Coverage = std::move(coverage),
                                             .Coarser = {}}};
  auto exact = cache.PreparePublication({record}, std::move(bytes));
  if (!exact) { return std::unexpected(exact.error()); }
  if (auto split = EncodeGroundTerrainPrecision(sheets, pack.Slots, bytesMost); split) {
    auto prepared = PreparePrecision(cache, record, std::move(*split), exact->StoredBytes(), into);
    if (!prepared) { return std::unexpected(prepared.error()); }
    if (*prepared) { return {}; }
  }
  into.push_back(std::move(*exact));
  return {};
}

std::shared_ptr<const std::vector<Sheet>> DecodePack(std::span<const uint8_t> bytes,
                                                     size_t expected) {
  BinaryValueReader in(bytes);
  uint32_t format = 0;
  uint32_t count = 0;
  if (!in(format, count) || format != kPackFormat || count != expected) { return {}; }
  auto sheets = std::make_shared<std::vector<Sheet>>(count);
  for (auto &sheet : *sheets) {
    if (!ReadGroundTerrainSheet(in, sheet)) { return {}; }
  }
  return in.In.Remaining() == 0 ? std::shared_ptr<const std::vector<Sheet>>(std::move(sheets))
                                : nullptr;
}

bool IsCanonical(const AssetRecord &pack, const AssetRecord &region) {
  if (!pack.Tile || pack.OffsetBytes != 0) { return false; }
  const auto &revision = pack.Tile->Revision;
  if (pack.Kind == kDetailKind) {
    return pack.Parent.empty() && ValidKey(revision) && PackKey(revision) == pack.Key &&
           BaseKey(revision) == pack.Tile->Coarser;
  }
  if (pack.Kind != kKind) { return false; }
  return pack.Parent.empty() ? revision == pack.Package && PackKey(revision) == pack.Key
                             : pack.Parent == region.Key && revision == region.Key;
}

std::optional<double> BaseError(const AssetRecord &base, const AssetRecord &detail) {
  if (base.Kind != kBaseKind || base.OffsetBytes != 0 || !base.Parent.empty() ||
      base.Bounds != detail.Bounds || base.Level != detail.Level || !base.Tile || !detail.Tile) {
    return std::nullopt;
  }
  const auto &source = *base.Tile;
  const auto &exact = *detail.Tile;
  const double error = source.ErrorM.value_or(-1);
  if (source.Revision != exact.Revision || source.Coverage != exact.Coverage ||
      !source.Coarser.empty() || !std::isfinite(error) || error < 0) {
    return std::nullopt;
  }
  return error;
}

struct PlannedPack {
  const AssetRecord *Payload;
  const AssetRecord *Base;
  const GroundTerrainManifest::Pack *Entry;
};

std::optional<PlannedPack> PlanPack(const AssetRecord &record,
                                    const GroundTerrainManifest &manifest,
                                    std::span<const AssetRecord> bases,
                                    const GroundTerrainErrorLimit &limit,
                                    size_t &bytesMost) {
  const auto entry =
      std::ranges::lower_bound(manifest.Packs, record.Key, {}, &GroundTerrainManifest::Pack::Key);
  if (entry == manifest.Packs.end() || entry->Key != record.Key) { return std::nullopt; }
  PlannedPack plan{.Payload = &record, .Base = nullptr, .Entry = &*entry};
  if (record.Kind == kDetailKind) {
    if (!record.Tile) { return std::nullopt; }
    const auto &key = record.Tile->Coarser;
    const auto base = std::ranges::lower_bound(bases, key, {}, &AssetRecord::Key);
    if (base == bases.end() || base->Key != key) { return std::nullopt; }
    const auto error = BaseError(*base, record);
    if (!error) { return std::nullopt; }
    if (limit && *error <= limit(base->Bounds)) {
      plan.Payload = &*base;
    } else {
      if (base->ByteCount > bytesMost) { return std::nullopt; }
      bytesMost -= static_cast<size_t>(base->ByteCount);
      plan.Base = &*base;
    }
  }
  if (plan.Payload->ByteCount > bytesMost) { return std::nullopt; }
  bytesMost -= static_cast<size_t>(plan.Payload->ByteCount);
  return plan;
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
  into.reserve(2 * slots.size() + 1);
  for (auto &[address, indices] : slots) {
    auto &entry = manifest.Packs.emplace_back(
        GroundTerrainManifest::Pack{.Key = {}, .Slots = std::move(indices)});
    const size_t first = into.size();
    auto pack = PreparePack(cache, address, sheets, bytesMost, entry, into);
    if (!pack) { return std::unexpected(pack.error()); }
    for (const auto &part : std::span(into).subspan(first)) { bytesMost -= part.NativeBytes(); }
  }
  return manifest;
}

std::expected<GroundTerrainPackReader::Pages, AssetCacheError>
GroundTerrainPackReader::Read(const AssetCache &cache,
                              const AssetRecord &record,
                              const AssetRecord *base,
                              size_t count,
                              size_t &readBytes) {
  if (record.OffsetBytes != 0) { return Pages{}; }
  Pages resident;
  {
    const std::scoped_lock lock(Lock_);
    const auto found = Resident_.find(record.Key);
    if (found != Resident_.end() && found->second.Package == record.Package &&
        found->second.NativeBytes == record.ByteCount) {
      resident = found->second.Pages.lock();
    }
  }
  if (resident) {
    cache.UsePackage(record.Package);
    if (base != nullptr) { cache.UsePackage(base->Package); }
    return resident->size() == count ? std::move(resident) : nullptr;
  }
  const auto pack = cache.Load(record.Key, static_cast<size_t>(record.ByteCount));
  if (!pack) { return std::unexpected(pack.error()); }
  if (!*pack || (**pack).Record().Package != record.Package ||
      (**pack).Record().ByteCount != record.ByteCount ||
      (**pack).Record().OffsetBytes != record.OffsetBytes) {
    return Pages{};
  }
  const auto bytes = (**pack).Bytes();
  readBytes += bytes.size();
  auto decoded = Materialize(cache, record, base, bytes, count, readBytes);
  if (!decoded) { return std::unexpected(decoded.error()); }
  if (*decoded) {
    const std::scoped_lock lock(Lock_);
    auto &entry = Resident_[record.Key];
    if (auto published = entry.Pages.lock(); published && entry.Package == record.Package &&
                                             entry.NativeBytes == record.ByteCount &&
                                             published->size() == count) {
      return published;
    }
    entry = {.Pages = *decoded, .Package = record.Package, .NativeBytes = record.ByteCount};
  }
  return decoded;
}

std::expected<GroundTerrainPackReader::Pages, AssetCacheError>
GroundTerrainPackReader::Materialize(const AssetCache &cache,
                                     const AssetRecord &record,
                                     const AssetRecord *base,
                                     std::span<const uint8_t> bytes,
                                     size_t count,
                                     size_t &readBytes) {
  if (base != nullptr) {
    auto coarse = Read(cache, *base, nullptr, count, readBytes);
    if (!coarse) { return std::unexpected(coarse.error()); }
    if (!*coarse) { return Pages{}; }
    auto exact = std::make_shared<std::vector<Sheet>>(**coarse);
    if (!ApplyGroundTerrainResidual(bytes, *exact)) { return Pages{}; }
    return exact;
  }
  if (record.Kind != kBaseKind) { return DecodePack(bytes, count); }
  auto compact = DecodeGroundTerrainBase(bytes, count);
  return compact ? std::make_shared<const std::vector<Sheet>>(std::move(*compact)) : nullptr;
}

std::expected<bool, AssetCacheError>
GroundTerrainPackReader::Restore(const AssetCache &cache,
                                 const AssetRecord &region,
                                 const GroundTerrainManifest &manifest,
                                 size_t bytesMost,
                                 std::vector<Sheet> &sheets,
                                 GroundTerrainRead &read,
                                 const std::stop_token &stop,
                                 const GroundTerrainErrorLimit &limit) {
  if (stop.stop_requested()) { return false; }
  std::vector<std::string_view> keys;
  keys.reserve(manifest.Packs.size());
  for (const auto &pack : manifest.Packs) { keys.push_back(pack.Key); }
  const auto index = cache.FindTiles(keys);
  if (!index) { return std::unexpected(index.error()); }
  if (index->size() != manifest.Packs.size()) { return false; }
  keys.clear();
  for (const auto &pack : *index) {
    if (!IsCanonical(pack, region)) { return false; }
    if (pack.Kind == kDetailKind && pack.Tile) { keys.push_back(pack.Tile->Coarser); }
  }
  auto bases = cache.FindTiles(keys);
  if (!bases) { return std::unexpected(bases.error()); }
  if (bases->size() != keys.size()) { return false; }
  std::ranges::sort(*bases, {}, &AssetRecord::Key);
  std::vector<PlannedPack> selected;
  selected.reserve(manifest.Packs.size());
  for (const auto &record : *index) {
    auto plan = PlanPack(record, manifest, *bases, limit, bytesMost);
    if (!plan) { return false; }
    selected.push_back(*plan);
  }
  {
    const std::scoped_lock lock(Lock_);
    std::erase_if(Resident_, [](const auto &entry) { return entry.second.Pages.expired(); });
  }
  sheets.resize(manifest.Sheets);
  for (const auto &[record, base, entry] : selected) {
    if (stop.stop_requested()) { return false; }
    const auto pages = Read(cache, *record, base, entry->Slots.size(), read.Bytes);
    if (!pages) { return std::unexpected(pages.error()); }
    if (!*pages) { return false; }
    read.CompactPacks += static_cast<size_t>(record->Kind == kBaseKind);
    for (size_t pageIndex = 0; pageIndex < entry->Slots.size(); ++pageIndex) {
      const auto &page = (**pages)[pageIndex];
      sheets[entry->Slots[pageIndex]] = {
          .Tile = page.Tile,
          .Nodes = {},
          .Side = page.Side,
          .Postings = page.Postings,
          .Virtual = page.Virtual,
          .SourceZoom = page.SourceZoom,
          .SharedSamples = std::shared_ptr<const std::vector<float>>(*pages, &page.Nodes)};
    }
  }
  return true;
}
}

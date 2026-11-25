#include "BuildingRoofAssets.h"
#include "ByteArchive.h"
#include "PreparedBuildingAssets.h"
#include "PreparedStructureTile.h"
#include "Sha256.h"
#include "StructureArtifact.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint64_t kIndexFormat = 0x32584449464f4f52ULL;
constexpr uint64_t kBlockFormat = 0x324b4c42464f4f52ULL;
constexpr std::string_view kIndexKind = "building-roof-index";
constexpr std::string_view kBlockKind = "building-roof-block";
constexpr std::string_view kCohortIndexKind = "building-roof-cohorts";
constexpr std::string_view kCohortKind = "building-roof-cohort";
constexpr uint64_t kCohortFormat = 0x3154524843464f52ULL;
constexpr size_t kCohortBytes = 2 * sizeof(uint32_t) + sizeof(double);
constexpr size_t kCohortIndexBytesMost =
    sizeof(uint64_t) + sizeof(uint32_t) + kCohortBytes * kStructureCellsPerTile;
constexpr size_t kRowBytes = sizeof(uint32_t) + (6 + 28 + 3) * sizeof(float);
constexpr size_t kBlockPrefix = sizeof(uint64_t) + sizeof(uint32_t);
constexpr size_t kPackedRowsMost = (kStructureArtifactBytesMost - kBlockPrefix) / kRowBytes;
constexpr size_t kIndexPrefix = sizeof(uint64_t) + 3 * sizeof(double) + 2 * sizeof(uint32_t);
constexpr size_t kChunkBytes = sizeof(uint32_t) + sizeof(double);
constexpr size_t kIndexBytesMost = 4u << 20;

std::string IndexKey(std::string_view baseKey) {
  return Sha256Hex("building-roof-index-2/" + std::string(baseKey));
}

std::string BlockKey(const std::string &index, size_t block) {
  return Sha256Hex("building-roof-block-2/" + index + "/" + std::to_string(block));
}

std::string CohortIndexKey(const std::string &index, size_t prefix) {
  return Sha256Hex("building-roof-cohorts-1/" + index + "/" + std::to_string(prefix));
}

std::string CohortKey(const std::string &index, uint32_t cell) {
  return Sha256Hex("building-roof-cohort-1/" + index + "/" + std::to_string(cell));
}

template <class Archive, class T> bool Fields(Archive &io, T &row) {
  if (!io.Number(row.Source)) { return false; }
  for (size_t axis = 0; axis < 3; ++axis) {
    if (!io.Number(row.Bounds.Min[axis]) || !io.Number(row.Bounds.Max[axis])) { return false; }
  }
  for (auto &plane : row.Planes) {
    for (auto &coefficient : plane) {
      if (!io.Number(coefficient)) { return false; }
    }
  }
  for (auto &value : row.WallTint) {
    if (!io.Number(value)) { return false; }
  }
  return true;
}

[[nodiscard]] std::expected<PreparedAssetWrite, AssetCacheError>
Package(AssetCache &cache,
        std::string key,
        std::string_view kind,
        std::string parent,
        const Box &bounds,
        std::vector<uint8_t> bytes) {
  std::vector<AssetRecord> records{{.Key = std::move(key),
                                    .Kind = std::string(kind),
                                    .Bounds = bounds,
                                    .Package = {},
                                    .ByteCount = bytes.size(),
                                    .Level = 1,
                                    .Parent = std::move(parent)}};
  return cache.PreparePublication(std::move(records), std::move(bytes));
}

[[nodiscard]] std::expected<std::vector<BuildingRoofProxy>, AssetCacheError>
Prepare(const PreparedStructureTile &base, const std::stop_token &stop) {
  std::vector<BuildingRoofProxy> records;
  for (uint32_t at = 0; at < base.Surfaces.size(); ++at) {
    if (stop.stop_requested()) { return std::unexpected(AssetCacheError::Busy); }
    const auto &surface = base.Surfaces[at];
    const auto ready = surface.RequireShapes();
    if (!ready) {
      return std::unexpected(ready.error() == StructureMeshError::PreparedSurfaceBusy
                                 ? AssetCacheError::Busy
                                 : AssetCacheError::Storage);
    }
    auto roof = BuildingRoofProxy::Of(surface, at);
    if (roof) { records.push_back(*roof); }
  }
  std::ranges::sort(records, [](const BuildingRoofProxy &a, const BuildingRoofProxy &b) {
    const double left = a.DiameterM();
    const double right = b.DiameterM();
    return left == right ? a.Source < b.Source : left < right;
  });
  return records;
}

Box WorldBounds(const Vec3 &anchor, std::span<const BuildingRoofProxy> rows) {
  Box bounds;
  for (const auto &row : rows) { bounds.Cover(row.WiderBounds()); }
  if (bounds.Empty()) {
    bounds.Cover(anchor);
    return bounds;
  }
  for (size_t i = 0; i < 3; ++i) {
    constexpr double infinity = std::numeric_limits<double>::infinity();
    bounds.Min[i] = std::nextafter(bounds.Min[i] + anchor[i], -infinity);
    bounds.Max[i] = std::nextafter(bounds.Max[i] + anchor[i], infinity);
  }
  return bounds;
}

std::expected<PreparedAssetWrite, AssetCacheError>
PackRows(AssetCache &cache,
         const std::string &parent,
         std::string key,
         std::string_view kind,
         const Vec3 &anchor,
         std::span<const BuildingRoofProxy> rows) {
  if (rows.size() > kPackedRowsMost) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  ByteWriter bytes(kBlockPrefix + kRowBytes * rows.size());
  if (!bytes.Number(kBlockFormat) || !bytes.Number(static_cast<uint32_t>(rows.size()))) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  for (const auto &row : rows) {
    if (!Fields(bytes, row)) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  }
  return Package(
      cache, std::move(key), kind, parent, WorldBounds(anchor, rows), std::move(bytes).TakeBytes());
}

std::expected<std::optional<std::vector<BuildingRoofProxy>>, AssetCacheError>
ReadRows(const AssetCache &cache,
         const std::string &key,
         std::string_view kind,
         const std::string &parent,
         BuildingRoofAssets::Block block,
         uint32_t sources) {
  const auto rows = static_cast<size_t>(block.Rows);
  if (rows > kPackedRowsMost) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  const auto loaded = cache.Load(key, kBlockPrefix + rows * kRowBytes);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return std::nullopt; }
  const auto &entry = (**loaded).Record();
  ByteReader bytes((**loaded).Bytes());
  uint64_t format = 0;
  uint32_t count = 0;
  if (entry.Kind != kind || entry.Parent != parent || entry.Level != 1 || !bytes.Number(format) ||
      format != kBlockFormat || !bytes.Number(count) || count != rows ||
      bytes.Remaining() != rows * kRowBytes) {
    return std::unexpected(AssetCacheError::Storage);
  }
  std::vector<BuildingRoofProxy> result(count);
  for (auto &row : result) {
    if (!Fields(bytes, row) || !row.Valid() || row.Source >= sources ||
        row.DiameterM() > block.MaximumDiameterM) {
      return std::unexpected(AssetCacheError::Storage);
    }
  }
  return result;
}

bool ValidCohort(const BuildingRoofAssets::Cohort &cohort, uint32_t sources) {
  return cohort.Cell != 0 && cohort.Cell <= kStructureCellsPerTile && cohort.Rows != 0 &&
         cohort.Rows <= sources && std::isfinite(cohort.ShellErrorM) && cohort.ShellErrorM >= 0;
}
}

std::expected<std::optional<BuildingRoofAssets>, AssetCacheError>
BuildingRoofAssets::Open(const AssetCache &cache, std::string_view baseKey) {
  BuildingRoofAssets product;
  product.Key_ = IndexKey(baseKey);
  const auto loaded = cache.Load(product.Key_, kIndexBytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return std::nullopt; }
  const auto &entry = (**loaded).Record();
  ByteReader bytes((**loaded).Bytes());
  uint64_t format = 0;
  uint32_t blocks = 0;
  if (entry.Kind != kIndexKind || entry.Parent != baseKey || entry.Level != 1 ||
      !bytes.Number(format) || format != kIndexFormat || !bytes.Number(product.AnchorEcef_[0]) ||
      !bytes.Number(product.AnchorEcef_[1]) || !bytes.Number(product.AnchorEcef_[2]) ||
      !bytes.Number(product.SourceCount_) || !bytes.Number(blocks) ||
      blocks > bytes.Remaining() / kChunkBytes || bytes.Remaining() != blocks * kChunkBytes ||
      !std::ranges::all_of(product.AnchorEcef_, [](double x) { return std::isfinite(x); })) {
    return std::unexpected(AssetCacheError::Storage);
  }
  product.Bounds_ = entry.Bounds;
  product.Blocks_.resize(blocks);
  uint64_t count = 0;
  double last = 0;
  for (auto &block : product.Blocks_) {
    if (!bytes.Number(block.Rows) || !bytes.Number(block.MaximumDiameterM) || block.Rows == 0 ||
        block.Rows > kRowsPerBlock || !std::isfinite(block.MaximumDiameterM) ||
        block.MaximumDiameterM <= 0 || block.MaximumDiameterM < last) {
      return std::unexpected(AssetCacheError::Storage);
    }
    last = block.MaximumDiameterM;
    count += block.Rows;
  }
  if (count > product.SourceCount_) { return std::unexpected(AssetCacheError::Storage); }
  return product;
}

std::expected<BuildingRoofAssets, AssetCacheError>
BuildingRoofAssets::Resolve(AssetCache &cache,
                            PreparedBuildingAssets *inputs,
                            const std::string &baseKey,
                            const std::stop_token &stop) {
  if (stop.stop_requested() || baseKey.empty()) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  auto opened = Open(cache, baseKey);
  if (!opened) { return std::unexpected(opened.error()); }
  if (*opened) { return std::move(**opened); }
  if (inputs == nullptr) { return std::unexpected(AssetCacheError::InvalidInput); }
  const auto base = inputs->Load(baseKey);
  if (!base || !*base || !(*base)->HeightQualified ||
      (*base)->Surfaces.size() != (*base)->Structures.size() ||
      (*base)->Surfaces.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  const auto prepared = Prepare(**base, stop);
  if (!prepared) { return std::unexpected(prepared.error()); }
  BuildingRoofAssets product;
  product.Key_ = IndexKey(baseKey);
  product.AnchorEcef_ = (*base)->AnchorEcef;
  product.Bounds_ = WorldBounds(product.AnchorEcef_, *prepared);
  product.SourceCount_ = static_cast<uint32_t>((*base)->Surfaces.size());
  const auto rows = std::span(*prepared);
  std::vector<PreparedAssetWrite> packages;
  packages.reserve(rows.size() / kRowsPerBlock + 2);
  for (size_t begin = 0; begin < rows.size(); begin += kRowsPerBlock) {
    if (stop.stop_requested()) { return std::unexpected(AssetCacheError::Busy); }
    const auto group = rows.subspan(begin, std::min<size_t>(kRowsPerBlock, rows.size() - begin));
    auto package = PackRows(cache,
                            product.Key_,
                            BlockKey(product.Key_, product.Blocks_.size()),
                            kBlockKind,
                            product.AnchorEcef_,
                            group);
    if (!package) { return std::unexpected(package.error()); }
    packages.push_back(std::move(*package));
    product.Blocks_.push_back({.MaximumDiameterM = group.back().DiameterM(),
                               .Rows = static_cast<uint32_t>(group.size())});
  }
  ByteWriter metadata(kIndexBytesMost);
  metadata.Reserve(kIndexPrefix + kChunkBytes * product.Blocks_.size());
  if (!metadata.Number(kIndexFormat) || !metadata.Number(product.AnchorEcef_[0]) ||
      !metadata.Number(product.AnchorEcef_[1]) || !metadata.Number(product.AnchorEcef_[2]) ||
      !metadata.Number(product.SourceCount_) ||
      !metadata.Number(static_cast<uint32_t>(product.Blocks_.size()))) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  for (const auto &block : product.Blocks_) {
    if (!metadata.Number(block.Rows) || !metadata.Number(block.MaximumDiameterM)) {
      return std::unexpected(AssetCacheError::CapacityExceeded);
    }
  }
  if (stop.stop_requested()) { return std::unexpected(AssetCacheError::Busy); }
  auto directory = Package(
      cache, product.Key_, kIndexKind, baseKey, product.Bounds_, std::move(metadata).TakeBytes());
  if (!directory) { return std::unexpected(directory.error()); }
  packages.push_back(std::move(*directory));
  const auto stored = cache.PublishBatch(packages);
  if (!stored) { return std::unexpected(stored.error()); }
  return product;
}

size_t BuildingRoofAssets::Prefix(double diameterM) const noexcept {
  if (!(diameterM >= 0)) { return 0; }
  const auto end = std::ranges::upper_bound(Blocks_, diameterM, {}, &Block::MaximumDiameterM);
  return static_cast<size_t>(end - Blocks_.begin());
}

std::expected<std::optional<std::vector<BuildingRoofProxy>>, AssetCacheError>
BuildingRoofAssets::LoadBlock(const AssetCache &cache, size_t index) const {
  if (index >= Blocks_.size()) { return std::unexpected(AssetCacheError::InvalidInput); }
  const auto &block = Blocks_[index];
  return ReadRows(cache, BlockKey(Key_, index), kBlockKind, Key_, block, SourceCount_);
}

std::optional<std::vector<BuildingRoofAssets::Cohort>>
BuildingRoofAssets::ReadCohorts(const AssetCache &cache, size_t prefix) const {
  if (prefix == 0 || prefix > Blocks_.size()) { return std::nullopt; }
  const auto loaded = cache.Load(CohortIndexKey(Key_, prefix), kCohortIndexBytesMost);
  if (!loaded || !*loaded) { return std::nullopt; }
  const auto &record = (**loaded).Record();
  ByteReader bytes((**loaded).Bytes());
  uint64_t format = 0;
  uint32_t count = 0;
  if (record.Kind != kCohortIndexKind || record.Parent != Key_ || !bytes.Number(format) ||
      format != kCohortFormat || !bytes.Number(count) || count > kStructureCellsPerTile ||
      bytes.Remaining() != count * kCohortBytes) {
    return std::nullopt;
  }
  std::vector<Cohort> result(count);
  uint32_t previous = 0;
  for (auto &entry : result) {
    if (!bytes.Number(entry.Cell) || !bytes.Number(entry.Rows) ||
        !bytes.Number(entry.ShellErrorM) || entry.Cell <= previous ||
        !ValidCohort(entry, SourceCount_)) {
      return std::nullopt;
    }
    previous = entry.Cell;
  }
  return result;
}

std::optional<std::vector<BuildingRoofProxy>>
BuildingRoofAssets::ReadCohort(const AssetCache &cache, size_t prefix, const Cohort &cohort) const {
  if (prefix == 0 || prefix > Blocks_.size() || !ValidCohort(cohort, SourceCount_)) {
    return std::nullopt;
  }
  const auto index = CohortIndexKey(Key_, prefix);
  auto rows =
      ReadRows(cache,
               CohortKey(index, cohort.Cell),
               kCohortKind,
               index,
               {.MaximumDiameterM = Blocks_[prefix - 1].MaximumDiameterM, .Rows = cohort.Rows},
               SourceCount_);
  return rows ? std::move(*rows) : std::nullopt;
}

std::expected<void, AssetCacheError>
BuildingRoofAssets::PublishCohort(AssetCache &cache,
                                  size_t prefix,
                                  std::span<const Cohort> directory,
                                  const Cohort &selected,
                                  std::span<const BuildingRoofProxy> rows) const {
  if (prefix == 0 || prefix > Blocks_.size() || selected.Rows != rows.size() ||
      directory.size() > kStructureCellsPerTile || !ValidCohort(selected, SourceCount_)) {
    return std::unexpected(AssetCacheError::InvalidInput);
  }
  ByteWriter bytes(kCohortIndexBytesMost);
  if (!bytes.Number(kCohortFormat) || !bytes.Number(static_cast<uint32_t>(directory.size()))) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  for (const auto &entry : directory) {
    if (!ValidCohort(entry, SourceCount_) || !bytes.Number(entry.Cell) ||
        !bytes.Number(entry.Rows) || !bytes.Number(entry.ShellErrorM)) {
      return std::unexpected(AssetCacheError::InvalidInput);
    }
  }
  const auto index = CohortIndexKey(Key_, prefix);
  auto catalog =
      Package(cache, index, kCohortIndexKind, Key_, Bounds_, std::move(bytes).TakeBytes());
  if (!catalog) { return std::unexpected(catalog.error()); }
  auto payload =
      PackRows(cache, index, CohortKey(index, selected.Cell), kCohortKind, AnchorEcef_, rows);
  if (!payload) { return std::unexpected(payload.error()); }
  const std::array packages{std::move(*catalog), std::move(*payload)};
  return cache.PublishBatch(packages);
}
}

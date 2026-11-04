#include "PreparedBuildingAssets.h"
#include "PageIndexAsset.h"
#include "ByteArchive.h"
#include "Sha256.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <optional>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr uint64_t kGroupFormat = 0x3150524752435342ULL;
using Members = std::vector<PreparedBuildingAssets::SourceMember>;

std::string GroupKey(const std::string &base) {
  const auto identity = "building-source-groups-1/" + base;
  return Sha256Hex(identity.data(), identity.size());
}

std::expected<std::optional<Members>, AssetCacheError>
LoadMembers(const AssetCache &cache, const std::string &key, size_t bytesMost) {
  const auto loaded = cache.Load(GroupKey(key), bytesMost);
  if (!loaded) { return std::unexpected(loaded.error()); }
  if (!*loaded) { return std::nullopt; }
  const auto &record = (**loaded).Record();
  ByteReader bytes((**loaded).Bytes());
  uint64_t format = 0;
  uint32_t count = 0;
  if (record.Kind != "building-source-groups" || record.Parent != key || !bytes.Number(format) ||
      format != kGroupFormat || !bytes.Number(count) || count == 0 ||
      count > bytes.Remaining() / (sizeof(uint32_t) + sizeof(uint8_t))) {
    return std::unexpected(AssetCacheError::Storage);
  }
  Members members(count);
  for (auto &member : members) {
    uint8_t supported = 0;
    if (!bytes.Number(member.Cell) || member.Cell == 0 || member.Cell > 64 ||
        !bytes.Number(supported) || supported > 1) {
      return std::unexpected(AssetCacheError::Storage);
    }
    member.FieldSupported = supported != 0;
  }
  if (bytes.Remaining() != 0) { return std::unexpected(AssetCacheError::Storage); }
  return members;
}

std::expected<void, AssetCacheError>
StoreMembers(AssetCache &cache,
             const std::string &key,
             const Box &bounds,
             std::span<const PreparedBuildingAssets::SourceMember> members,
             size_t bytesMost) {
  ByteWriter bytes(bytesMost);
  if (!bytes.Number(kGroupFormat) || !bytes.Number(static_cast<uint32_t>(members.size()))) {
    return std::unexpected(AssetCacheError::CapacityExceeded);
  }
  for (const auto &member : members) {
    if (!bytes.Number(member.Cell) || !bytes.Number(static_cast<uint8_t>(member.FieldSupported))) {
      return std::unexpected(AssetCacheError::CapacityExceeded);
    }
  }
  const AssetRecord record{.Key = GroupKey(key),
                           .Kind = "building-source-groups",
                           .Bounds = bounds,
                           .Package = {},
                           .ByteCount = bytes.Bytes().size(),
                           .Parent = key};
  return cache.Publish(std::span(&record, 1), bytes.Bytes());
}
}

std::expected<std::optional<PreparedBuildingAssets::SourceGroups>, StructureBakeError>
PreparedBuildingAssets::DescribeSourceGroups(const std::string &key, SourceGroupQuery query) {
  auto members = LoadMembers(*Cache_, key, query.IndexBytesMost);
  if (!members) { return std::unexpected(CacheFailure(members.error())); }
  Base base;
  if (!*members) {
    auto loaded = Load(key);
    if (!loaded) { return std::unexpected(loaded.error()); }
    if (!*loaded) { return std::nullopt; }
    base = std::move(*loaded);
    if (base->Structures.size() != base->Surfaces.size() ||
        !PageIndexAsset::EncodedBytes(base->Structures.size())) {
      return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
    }
    members->emplace();
    (**members).reserve(base->Structures.size());
    for (size_t at = 0; at < base->Structures.size(); ++at) {
      const auto cell = base->Structures[at].Layout.Cell.Index;
      if (cell == 0 || cell > 64) {
        return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
      }
      const auto &surface = base->Surfaces[at];
      (**members).push_back(
          {.Cell = cell, .FieldSupported = surface.FaceCount() != 0 && !surface.Bounds().Empty()});
    }
  }
  auto index = SourceIndex(key, query.IndexBytesMost, base);
  if (!index) { return std::unexpected(index.error()); }
  if (!*index) { return std::nullopt; }
  if ((**members).size() != (*index)->Hierarchy.SourceOrder().size()) {
    return std::unexpected(StructureBakeErrorKind::ArtifactInvalidProduct);
  }
  if (base) {
    auto stored = StoreMembers(
        *Cache_, key, (*index)->Hierarchy.Nodes().front().Bounds, **members, query.IndexBytesMost);
    if (!stored) { return std::unexpected(CacheFailure(stored.error())); }
  }
  auto cells = DescribeSourceCells(key, query.Occupied);
  if (!cells) { return std::unexpected(cells.error()); }
  return SourceGroups{
      .Index = std::move(*index), .Members = std::move(**members), .Cells = std::move(*cells)};
}
}

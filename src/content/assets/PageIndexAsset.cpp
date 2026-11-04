#include "PageIndexAsset.h"
#include "AssetPublication.h"
#include "BinaryValueArchive.h"
#include "Sha256.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine {
namespace {
constexpr uint64_t kFormat = 0x3158444945474150ULL;
constexpr size_t kNodeBytes = 6 * sizeof(double) + 4 * sizeof(uint32_t);

std::optional<size_t> EncodedBytes(size_t pages) {
  constexpr size_t header = sizeof(kFormat) + 2 * sizeof(uint32_t);
  constexpr size_t perPage = 2 * kNodeBytes + sizeof(uint32_t);
  constexpr size_t pagesMost =
      std::min<size_t>(std::numeric_limits<uint32_t>::max() / 2,
                       (std::numeric_limits<size_t>::max() - header) / perPage);
  if (pages > pagesMost) { return std::nullopt; }
  return header + pages * perPage;
}

std::optional<std::vector<uint8_t>> Encode(const PlanHierarchy &hierarchy, size_t bytesMost) {
  BinaryValueWriter out(bytesMost);
  if (!out(kFormat,
           static_cast<uint32_t>(hierarchy.Nodes().size()),
           static_cast<uint32_t>(hierarchy.SourceOrder().size()))) {
    return std::nullopt;
  }
  for (const auto &node : hierarchy.Nodes()) {
    if (!out(node.Bounds.Min.Axis,
             node.Bounds.Max.Axis,
             node.First,
             node.Count,
             node.Left,
             node.Right)) {
      return std::nullopt;
    }
  }
  for (const auto page : hierarchy.SourceOrder()) {
    if (!out(page)) { return std::nullopt; }
  }
  return std::move(out.Out).TakeBytes();
}

std::optional<PlanHierarchy> Decode(std::span<const uint8_t> bytes, std::span<const Box> bounds) {
  BinaryValueReader in(bytes);
  uint64_t format = 0;
  uint32_t nodes = 0;
  uint32_t pages = 0;
  if (!in(format, nodes, pages) || format != kFormat || pages != bounds.size() || pages == 0 ||
      nodes != size_t{2} * pages - 1 || nodes > in.In.Remaining() / kNodeBytes) {
    return std::nullopt;
  }
  std::vector<PlanHierarchy::Node> tree(nodes);
  for (auto &node : tree) {
    if (!in(node.Bounds.Min.Axis,
            node.Bounds.Max.Axis,
            node.First,
            node.Count,
            node.Left,
            node.Right)) {
      return std::nullopt;
    }
  }
  if (pages > in.In.Remaining() / sizeof(uint32_t)) { return std::nullopt; }
  std::vector<uint32_t> order(pages);
  for (auto &page : order) {
    if (!in(page)) { return std::nullopt; }
  }
  if (in.In.Remaining() != 0) { return std::nullopt; }
  auto restored = PlanHierarchy::Restore(std::move(tree), std::move(order), bounds);
  if (!restored) { return std::nullopt; }
  return std::move(*restored);
}
}

std::expected<PageIndexAsset, AssetCacheError>
ResolvePageIndex(AssetCache &cache, std::string_view parent, std::span<const Box> pageBounds) {
  if (!AssetProductKey::Parse(parent)) { return std::unexpected(AssetCacheError::InvalidInput); }
  const auto bytesMost = EncodedBytes(pageBounds.size());
  if (!bytesMost) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  const auto recipe = "spatial-page-index-1/" + std::string(parent);
  PageIndexAsset result{.Key = Sha256Hex(recipe.data(), recipe.size()), .Hierarchy = {}};
  if (pageBounds.empty()) { return result; }
  auto stored = cache.Load(result.Key, *bytesMost);
  if (!stored) { return std::unexpected(stored.error()); }
  if (*stored) {
    const auto &record = (**stored).Record();
    if (record.Kind != "spatial-page-index" || record.Parent != parent) {
      return std::unexpected(AssetCacheError::Storage);
    }
    auto restored = Decode((**stored).Bytes(), pageBounds);
    if (!restored || restored->Nodes().front().Bounds.Min != record.Bounds.Min ||
        restored->Nodes().front().Bounds.Max != record.Bounds.Max) {
      return std::unexpected(AssetCacheError::Storage);
    }
    result.Hierarchy = std::move(*restored);
    result.ByteCount = (**stored).Bytes().size();
    result.From = PageIndexAsset::Origin::Stored;
    return result;
  }
  auto built = PlanHierarchy::Build(pageBounds);
  if (!built) { return std::unexpected(AssetCacheError::InvalidInput); }
  auto bytes = Encode(*built, *bytesMost);
  if (!bytes) { return std::unexpected(AssetCacheError::CapacityExceeded); }
  const AssetRecord record{.Key = result.Key,
                           .Kind = "spatial-page-index",
                           .Bounds = built->Nodes().front().Bounds,
                           .Package = Sha256Hex(bytes->data(), bytes->size()),
                           .ByteCount = bytes->size(),
                           .Parent = std::string(parent)};
  const auto published = cache.Publish(std::span(&record, 1), *bytes);
  if (!published) { return std::unexpected(published.error()); }
  result.Hierarchy = std::move(*built);
  result.ByteCount = bytes->size();
  result.From = PageIndexAsset::Origin::Created;
  return result;
}
}

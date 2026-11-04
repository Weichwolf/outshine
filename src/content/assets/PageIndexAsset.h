#ifndef OUTSHINE_CONTENT_ASSETS_PAGEINDEXASSET_H
#define OUTSHINE_CONTENT_ASSETS_PAGEINDEXASSET_H

#include "AssetCache.h"
#include "PlanHierarchy.h"
#include <cstddef>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string>
#include <string_view>

namespace outshine {
struct PageIndexAsset {
  enum class Origin { Empty, Stored, Created };
  std::string Key;
  PlanHierarchy Hierarchy;
  size_t ByteCount = 0;
  Origin From = Origin::Empty;
  [[nodiscard]] static std::optional<size_t> EncodedBytes(size_t pages) noexcept;
};

// Page order is the immutable manifest's order; Bounds and Parent use its coordinate frame.
[[nodiscard]] std::expected<std::optional<PageIndexAsset>, AssetCacheError>
LoadPageIndex(const AssetCache &cache, std::string_view parent, size_t bytesMost);

[[nodiscard]] std::expected<PageIndexAsset, AssetCacheError>
ResolvePageIndex(AssetCache &cache,
                 std::string_view parent,
                 std::span<const Box> pageBounds,
                 size_t bytesMost = std::numeric_limits<size_t>::max());
}
#endif

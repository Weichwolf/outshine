#ifndef OUTSHINE_CONTENT_ASSETS_PAGEINDEXASSET_H
#define OUTSHINE_CONTENT_ASSETS_PAGEINDEXASSET_H

#include "AssetCache.h"
#include "PlanHierarchy.h"
#include <cstddef>
#include <expected>
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
};

// Page order is the immutable manifest's order; Bounds and Parent use its coordinate frame.
[[nodiscard]] std::expected<PageIndexAsset, AssetCacheError>
ResolvePageIndex(AssetCache &cache, std::string_view parent, std::span<const Box> pageBounds);
}
#endif

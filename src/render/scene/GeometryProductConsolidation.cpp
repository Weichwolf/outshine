#include "GeometryPageResidency.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Render {
bool GeometryPageResidency::ConsolidateProducts(uint32_t tile,
                                                uint64_t sourceKey,
                                                const GeometryPageView &product,
                                                std::span<const std::string_view> keys,
                                                std::string &error) {
  if (Renderer_ == nullptr || sourceKey == 0 || product.AssetKey.empty() || !product.Coverage ||
      !product.Parts.empty() || keys.empty() ||
      std::ranges::adjacent_find(keys, [](auto a, auto b) { return a >= b; }) != keys.end()) {
    error = "geometry consolidation needs ordered distinct products and exact source coverage";
    return false;
  }
  std::vector<ResidentGeometryPage *> targets;
  std::vector<const SourceCoverage *> coverage;
  targets.reserve(keys.size());
  coverage.reserve(keys.size());
  size_t parts = 0;
  const auto first = std::ranges::lower_bound(Pages_, tile, {}, &ResidentGeometryPage::Tile);
  for (auto at = first; at != Pages_.end() && at->Tile == tile; ++at) {
    if (at->SourceKey != sourceKey) { continue; }
    if (at->AssetKey == product.AssetKey) {
      error = "consolidated geometry product is already resident";
      return false;
    }
    if (!std::ranges::binary_search(keys, std::string_view(at->AssetKey))) { continue; }
    if (at->Visible || at->Mode != Publication::Product || !at->Coverage ||
        at->Cell != product.RequestedCell.value_or(0) || at->Detail != product.RequestedDetail ||
        at->OccupiedCells != product.OccupiedCells ||
        (!targets.empty() && at->Row != targets.front()->Row) ||
        at->Parts.size() > std::numeric_limits<size_t>::max() - parts) {
      error = "geometry products have incompatible residency or coordinate frames";
      return false;
    }
    targets.push_back(&*at);
    coverage.push_back(&*at->Coverage);
    parts += at->Parts.size();
  }
  if (targets.size() != keys.size() || !SourceCoverage::Partition(*product.Coverage, coverage)) {
    error = "geometry products do not partition the consolidated source";
    return false;
  }
  ResidentGeometryPage combined{.Tile = tile,
                                .Cell = product.RequestedCell.value_or(0),
                                .Digest = product.Digest,
                                .SourceKey = sourceKey,
                                .AssetKey = std::string(product.AssetKey),
                                .Mode = Publication::Product,
                                .OccupiedCells = product.OccupiedCells,
                                .FallbackHeights = product.FallbackHeights,
                                .Detail = product.RequestedDetail,
                                .SelectedView = product.SelectedView,
                                .Coverage = product.Coverage,
                                .Row = targets.front()->Row,
                                .Visible = false,
                                .Parts = {}};
  combined.Parts.reserve(parts);
  for (auto *page : targets) {
    std::ranges::move(page->Parts, std::back_inserter(combined.Parts));
    page->Parts.clear();
  }
  std::erase_if(Pages_, [&](const ResidentGeometryPage &page) {
    return page.Tile == tile && page.SourceKey == sourceKey &&
           std::ranges::binary_search(keys, std::string_view(page.AssetKey));
  });
  Pages_.insert(std::ranges::lower_bound(Pages_, std::pair{tile, combined.Cell}, {}, AddressOf),
                std::move(combined));
  RefreshDigest();
  return true;
}

void GeometryPageResidency::DiscardProduct(uint32_t tile,
                                           uint64_t sourceKey,
                                           std::string_view key) {
  std::erase_if(Pages_, [&](const ResidentGeometryPage &page) {
    if (page.Tile != tile || page.SourceKey != sourceKey || page.AssetKey != key || page.Visible ||
        page.Mode != Publication::Product) {
      return false;
    }
    Releases(page);
    return true;
  });
  RefreshDigest();
}
}

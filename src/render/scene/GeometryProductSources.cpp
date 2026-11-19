#include "GeometryPageResidency.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

namespace outshine::Render {
std::expected<const SourceCoverage *, std::string>
GeometryPageResidency::BindCoverage(ProductAddress address, const SourceCoverage &coverage) {
  if (address.SourceKey == 0 || address.AssetKey.empty()) {
    return std::unexpected("geometry source binding needs an immutable product identity");
  }
  ResidentGeometryPage *target = nullptr;
  const auto first =
      std::ranges::lower_bound(Pages_, address.Tile, {}, &ResidentGeometryPage::Tile);
  for (auto at = first; at != Pages_.end() && at->Tile == address.Tile; ++at) {
    if (at->SourceKey != address.SourceKey || at->AssetKey != address.AssetKey) { continue; }
    if (target != nullptr) { return std::unexpected("geometry product identity is ambiguous"); }
    target = &*at;
  }
  if (target == nullptr) {
    return std::unexpected("geometry source binding has no resident product");
  }
  if (target->Coverage && *target->Coverage != coverage) {
    return std::unexpected("resident geometry product source coverage cannot change");
  }
  if (!target->Coverage) {
    target->Coverage = coverage;
    target->CoverageFrom = CoverageSource::Metadata;
  }
  return &*target->Coverage;
}

std::expected<std::vector<uint32_t>, std::string>
GeometryPageResidency::RebindProducts(ProductRevision revision,
                                      std::span<ProductBinding> bindings) {
  const uint32_t tile = revision.Tile;
  if (Renderer_ == nullptr || revision.SourceKey == 0 ||
      bindings.size() > std::numeric_limits<uint32_t>::max()) {
    return std::unexpected("geometry product rebind needs a live renderer and source revision");
  }
  std::unordered_map<std::string_view, uint32_t> requested;
  requested.reserve(bindings.size());
  for (uint32_t index = 0; index < bindings.size(); ++index) {
    const auto key = bindings[index].AssetKey;
    if (key.empty() || !requested.emplace(key, index).second) {
      return std::unexpected("geometry product rebind needs unique immutable asset keys");
    }
  }
  std::vector<uint8_t> retained(bindings.size());
  for (auto &held : Pages_) {
    if (held.Tile != tile || held.Mode != Publication::Product) { continue; }
    const auto found = requested.find(held.AssetKey);
    if (found != requested.end() && held.Visible &&
        held.DrawOrderGroup == revision.DrawOrderGroup && held.Row == revision.Placement &&
        retained[found->second] == 0) {
      auto &target = bindings[found->second].Coverage;
      if (held.SourceKey != revision.SourceKey || held.Coverage == target) {
        held.SourceKey = revision.SourceKey;
        held.Coverage = std::move(target);
        retained[found->second] = 1;
        continue;
      }
    }
    Releases(held);
    held.AssetKey.clear();
  }
  std::erase_if(Pages_, [tile](const ResidentGeometryPage &held) {
    return held.Tile == tile && held.Mode == Publication::Product && held.AssetKey.empty();
  });
  RefreshDigest();
  std::vector<uint32_t> missing;
  for (uint32_t index = 0; index < retained.size(); ++index) {
    if (retained[index] == 0) { missing.push_back(index); }
  }
  return missing;
}
}

#include "GeometryPageResidency.h"
#include <algorithm>
#include <cstdint>
#include <expected>
#include <string>
#include <string_view>

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
}

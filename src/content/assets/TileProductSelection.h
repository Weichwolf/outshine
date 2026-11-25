#ifndef OUTSHINE_CONTENT_ASSETS_TILEPRODUCTSELECTION_H
#define OUTSHINE_CONTENT_ASSETS_TILEPRODUCTSELECTION_H

#include "AssetRecord.h"
#include <algorithm>
#include <cmath>
#include <optional>
#include <span>
#include <string_view>

namespace outshine {
struct TileProductDomain {
  std::string_view Revision;
  std::string_view Coverage;
};

[[nodiscard]] inline const AssetRecord *FindTileProduct(std::span<const AssetRecord> byKey,
                                                        std::span<const std::string_view> preferred,
                                                        TileProductDomain domain) {
  for (const auto key : preferred) {
    const auto found = std::ranges::lower_bound(byKey, key, {}, &AssetRecord::Key);
    if (found != byKey.end() && found->Key == key && found->Tile &&
        found->Tile->Revision == domain.Revision && found->Tile->Coverage == domain.Coverage) {
      return &*found;
    }
  }
  return nullptr;
}

[[nodiscard]] inline std::optional<double> CertifiedTileError(const AssetRecord &product) {
  if (!product.Tile) { return std::nullopt; }
  const auto error = product.Tile->ErrorM;
  return error && std::isfinite(*error) && *error >= 0 ? error : std::nullopt;
}

[[nodiscard]] inline bool FitsTileError(const AssetRecord &product, const auto &allows) {
  const auto error = CertifiedTileError(product);
  return error && allows(product.Bounds, *error);
}
}

#endif

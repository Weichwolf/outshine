#ifndef OUTSHINE_CONTENT_ASSETS_SURFACETILEASSET_H
#define OUTSHINE_CONTENT_ASSETS_SURFACETILEASSET_H

#include "SurfaceTile.h"
#include "Box.h"
#include <cstddef>
#include <cstdint>
#include <expected>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace outshine {

class AssetCache;

[[nodiscard]] std::expected<void, std::string>
PublishSurfaceTile(AssetCache &cache, std::string_view revision, std::span<const uint8_t> bytes);

struct SurfaceTileLookup {
  std::optional<SurfaceTile> Product;
  size_t RequestedBytes = 0;
};

[[nodiscard]] std::expected<SurfaceTileLookup, std::string>
LoadSurfaceTileAt(AssetCache &cache, std::string_view revision, LongitudeLatitude location);

struct SurfaceCutDemand {
  Box SweptEyeEcef;
  double RadiusM = 0;
  double FocalPixels = 0;
};

struct SurfaceCutLookup {
  std::vector<SurfaceTile> Products;
  size_t RequestedBytes = 0;
};

[[nodiscard]] std::expected<SurfaceCutLookup, std::string>
LoadSurfaceCut(AssetCache &cache, std::string_view revision, const SurfaceCutDemand &demand);

}

#endif

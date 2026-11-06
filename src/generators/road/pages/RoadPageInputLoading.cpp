#include "PreparedRoadPages.h"
#include <algorithm>
#include <cstddef>
#include <expected>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
bool MatchesParts(const RoadPageRecipe &recipe, const RoadPageAddress &address) {
  std::vector<RoadPagePart> parts;
  parts.reserve(recipe.Intervals.size() + recipe.Junctions.size());
  for (const auto &interval : recipe.Intervals) {
    parts.push_back({.Kind = RoadPagePartKind::Interval, .Index = interval.Edge});
  }
  for (const auto &junction : recipe.Junctions) {
    parts.push_back({.Kind = RoadPagePartKind::Junction, .Index = junction.Index});
  }
  std::ranges::sort(parts, [](const auto &a, const auto &b) {
    return a.Kind != b.Kind ? a.Kind < b.Kind : a.Index < b.Index;
  });
  return parts == address.Parts;
}
}

std::expected<std::optional<AssetCache::ReadPlan>, PreparedRoadPages::Failure>
PreparedRoadPages::PlanRecipeRead(const RoadPageAddress &address, size_t bytesMost) const {
  const auto key = RoadPageRecipeKey(address);
  if (key.empty()) { return std::unexpected(std::string("invalid road recipe address")); }
  auto plan = Cache_->PlanRead(key, bytesMost);
  if (!plan) { return std::unexpected(plan.error()); }
  return *plan;
}

std::expected<std::optional<RoadPageRecipe>, PreparedRoadPages::Failure>
PreparedRoadPages::LoadRecipe(const RoadPageAddress &address,
                              const AssetCache::ReadPlan &read,
                              size_t capacityMost) const {
  const auto key = RoadPageRecipeKey(address);
  if (key.empty()) { return std::unexpected(std::string("invalid road recipe address")); }
  std::optional<RoadPageRecipe> recipe;
  auto loaded = Cache_->LoadValidated(
      key,
      "road-page-recipe",
      read,
      [&](const CachedAsset &asset) -> std::expected<bool, AssetCacheError> {
        auto decoded = DecodeRoadPageRecipe(
            key, asset.Bytes(), {.BytesMost = read.PackageBytesMost, .CapacityMost = capacityMost});
        if (!decoded) {
          if (decoded.error() == RoadPageRecipeCodecError::CapacityExceeded) {
            return std::unexpected(AssetCacheError::CapacityExceeded);
          }
          return false;
        }
        if (!MatchesParts(*decoded, address)) { return false; }
        recipe = std::move(*decoded);
        return true;
      });
  if (!loaded) { return std::unexpected(loaded.error()); }
  return recipe;
}
}

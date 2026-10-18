#include "PlantRecipeLayout.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <numbers>
#include <optional>
#include <span>
#include <string_view>
#include <vector>
#include <utility>

namespace outshine::Generators {
namespace {
constexpr size_t kMostBranches = kPlantRecipeMostBranches, kMostWhorlMembers = 16, kMostOrders = 8;
constexpr size_t kLeafWhorlMembers = 3;

bool Valid(const PlantLevel &level) {
  const std::array values{level.LengthM,
                          level.BaseRadiusM,
                          level.TipRadiusM,
                          level.TipScale,
                          level.AttachmentJitter,
                          level.AzimuthVariationRad,
                          level.UpwardBias,
                          level.First,
                          level.Last,
                          level.AngleRad,
                          level.AngleVariationRad,
                          level.BendRad,
                          level.BendVariationRad,
                          level.LeafFirst,
                          level.LeafLast,
                          level.LeafAngleRad,
                          level.LeafSkyAlignment,
                          level.LeafInternodeM.value_or(1),
                          level.TipAngleRad.value_or(level.AngleRad)};
  if (!std::ranges::all_of(values, [](double value) { return std::isfinite(value); })) {
    return false;
  }
  const double curvature = std::abs(level.BendRad) + level.BendVariationRad;
  const double tip = level.TipAngleRad.value_or(level.AngleRad);
  constexpr double kLeastTipScale = 0.05;
  return level.Branches > 0 && level.Branches <= kMostBranches && level.WhorlSize > 0 &&
         level.WhorlSize <= kMostWhorlMembers && level.BaseRadiusM > 0 && level.TipRadiusM > 0 &&
         level.TipRadiusM <= level.BaseRadiusM && level.TipScale >= kLeastTipScale &&
         level.TipScale <= 1 &&
         (level.Shape == PlantBranchShape::Taper || level.Shape == PlantBranchShape::Rounded) &&
         level.AttachmentJitter >= 0 && level.AttachmentJitter < 0.5 && level.UpwardBias >= 0 &&
         level.UpwardBias < 0.5 && level.AzimuthVariationRad >= 0 &&
         level.AzimuthVariationRad <= std::numbers::pi && level.BendVariationRad >= 0 &&
         curvature <= std::numbers::pi &&
         level.LengthM * level.TipScale > curvature * level.BaseRadiusM && level.First >= 0 &&
         level.First <= level.Last && level.Last <= 1 && level.AngleVariationRad >= 0 &&
         level.AngleRad - level.AngleVariationRad > 0 &&
         level.AngleRad + level.AngleVariationRad < std::numbers::pi &&
         tip - level.AngleVariationRad > 0 && tip + level.AngleVariationRad < std::numbers::pi &&
         level.LeafFirst >= 0 && level.LeafFirst <= level.LeafLast && level.LeafLast <= 1 &&
         level.LeafAngleRad > 0 && level.LeafAngleRad < std::numbers::pi &&
         level.LeafSkyAlignment >= 0 && level.LeafSkyAlignment < 1 &&
         level.LeafInternodeM.value_or(1) > 0 &&
         static_cast<uint8_t>(level.Arrangement) <= static_cast<uint8_t>(PlantArrangement::Whorled);
}

}

bool ValidPlantRecipe(const PlantRecipe &recipe) {
  if (recipe.Levels.empty() || recipe.Levels.size() > kMostOrders ||
      recipe.Blades.size() > kMostBranches || recipe.Levels.front().Branches != 1) {
    return false;
  }
  for (size_t at = 0; at < recipe.Levels.size(); ++at) {
    const auto &level = recipe.Levels[at];
    if (!Valid(level) || (recipe.Blades.empty() && level.LeafInternodeM)) { return false; }
    if (at > 0) {
      const auto &parent = recipe.Levels[at - 1];
      const double radius =
          parent.BaseRadiusM + (parent.TipRadiusM - parent.BaseRadiusM) * level.Last;
      if (level.BaseRadiusM > radius) { return false; }
    }
  }
  return std::ranges::all_of(recipe.Blades, [&](const PlantBlade &blade) {
    return blade.PetioleM == recipe.Blades.front().PetioleM &&
           MeasurePlantBladeSurface(blade,
                                    {.Segments = 2,
                                     .Across = 1,
                                     .MaximumVertices = std::numeric_limits<size_t>::max(),
                                     .MaximumTriangles = std::numeric_limits<size_t>::max()})
               .has_value();
  });
}

namespace {
std::optional<uint64_t> LeafCount(const PlantLevel &level, size_t slot) {
  if (!level.LeafInternodeM) { return uint64_t{0}; }
  const double intervals = (level.LeafLast - level.LeafFirst) * PlantLevelLength(level, slot) /
                           level.LeafInternodeM.value_or(1);
  const size_t per = PlantLeavesPerGroup(level.Arrangement);
  if (!std::isfinite(intervals) ||
      intervals >=
          static_cast<double>(std::numeric_limits<int32_t>::max()) / static_cast<double>(per)) {
    return std::nullopt;
  }
  return level.LeafInternodeM ? (static_cast<uint64_t>(std::floor(intervals)) + 1) * per : 0;
}

bool Topology(const PlantRecipe &recipe, PlantRecipeLayout &layout) {
  uint64_t axes = 0;
  uint64_t leaves = 0;
  uint64_t parents = 1;
  constexpr uint64_t kMost = std::numeric_limits<uint64_t>::max();
  for (size_t at = 0; at < recipe.Levels.size(); ++at) {
    const auto &level = recipe.Levels[at];
    auto &row = layout.Levels[at];
    row.AxisOffset = axes;
    row.LeafOffset = leaves;
    if (parents > kMost / level.Branches) { return false; }
    row.AxisCount = parents * level.Branches;
    if (row.AxisCount > kMost - axes) { return false; }
    axes += row.AxisCount;
    for (size_t slot = 0; slot < level.Branches; ++slot) {
      const auto count = LeafCount(level, slot);
      if (!count) { return false; }
      row.LeafPrefix[slot + 1] = row.LeafPrefix[slot] + *count;
    }
    const uint64_t per = row.LeafPrefix[level.Branches];
    if (per != 0 && parents > (kMost - leaves) / per) { return false; }
    leaves += parents * per;
    parents = row.AxisCount;
  }
  return true;
}

bool Select(const PlantRecipe &recipe, PlantRecipeDemand demand, PlantRecipeLayout &layout) {
  const size_t depth =
      static_cast<size_t>(std::ranges::find(demand.Path, int16_t{-1}) - demand.Path.begin());
  if (depth >= recipe.Levels.size() ||
      !std::ranges::all_of(std::span(demand.Path).subspan(depth),
                           [](int16_t slot) { return slot == -1; })) {
    return false;
  }
  uint64_t ordinal = 0;
  for (size_t at = 0; at <= depth; ++at) {
    if (at > 0) {
      const int16_t slot = demand.Path[at - 1];
      if (slot < 0 || std::cmp_greater_equal(slot, recipe.Levels[at].Branches)) { return false; }
      ordinal = ordinal * recipe.Levels[at].Branches + static_cast<size_t>(slot);
    }
    layout.Levels[at].FirstOrdinal = ordinal;
  }
  layout.Depth = depth;
  layout.Size.Root = layout.Levels[depth].AxisOffset + ordinal;
  for (size_t at = 0; at < recipe.Levels.size(); ++at) {
    auto &row = layout.Levels[at];
    row.Count = at < depth ? 1 : row.AxisCount / layout.Levels[depth].AxisCount;
    if (at >= depth) { row.FirstOrdinal = ordinal * row.Count; }
    row.FirstRow = layout.Size.Axes;
    if (row.Count > demand.MaximumAxes - layout.Size.Axes) { return false; }
    layout.Size.Axes += static_cast<size_t>(row.Count);
    if (at < depth) { continue; }
    const uint64_t first = PlantLeafPrefix(recipe.Levels[at], row, row.FirstOrdinal);
    const uint64_t last = PlantLeafPrefix(recipe.Levels[at], row, row.FirstOrdinal + row.Count);
    if (last - first > demand.MaximumLeaves - layout.Size.Leaves) { return false; }
    layout.Size.Leaves += static_cast<size_t>(last - first);
  }
  return layout.Size.Axes <= std::vector<PlantAxis>{}.max_size() &&
         layout.Size.Leaves <= std::vector<PlantLeaf>{}.max_size();
}
}

double PlantLevelFraction(size_t slot, const PlantLevel &level, double single) noexcept {
  const size_t groups = (level.Branches - 1) / level.WhorlSize + 1;
  const size_t group = slot / level.WhorlSize;
  return groups > 1 ? static_cast<double>(group) / static_cast<double>(groups - 1) : single;
}

double PlantLevelLength(const PlantLevel &level, size_t slot) noexcept {
  const double fraction = PlantLevelFraction(slot, level, 0);
  return level.LengthM *
         (level.Shape == PlantBranchShape::Rounded
              ? level.TipScale + (1 - level.TipScale) * std::sin(std::numbers::pi * fraction)
              : 1 + (level.TipScale - 1) * fraction);
}

size_t PlantLeavesPerGroup(PlantArrangement arrangement) noexcept {
  if (arrangement == PlantArrangement::Whorled) { return kLeafWhorlMembers; }
  return arrangement == PlantArrangement::Opposite || arrangement == PlantArrangement::Decussate
             ? 2
             : 1;
}

uint64_t PlantLeafPrefix(const PlantLevel &level,
                         const PlantRecipeLevelLayout &layout,
                         uint64_t ordinal) noexcept {
  return ordinal / level.Branches * layout.LeafPrefix[level.Branches] +
         layout.LeafPrefix[ordinal % level.Branches];
}

std::expected<PlantRecipeLayout, std::string_view> LayoutPlantRecipe(const PlantRecipe &recipe,
                                                                     PlantRecipeDemand demand) {
  PlantRecipeLayout result;
  if (!ValidPlantRecipe(recipe) || !Topology(recipe, result)) {
    return std::unexpected(
        "plant recipe requires finite regular levels, matching blades and bounded identities");
  }
  if (!Select(recipe, demand, result)) {
    return std::unexpected("plant recipe branch or detail budget is invalid");
  }
  return result;
}
}

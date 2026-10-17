#include "PlantRecipe.h"
#include "PlantRecipeLayout.h"
#include "PlantRandom.h"
#include "PlantArc.h"
#include "PlantFrame.h"
#include "PlantShoot.h"
#include "math/Vec3.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <numbers>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
constexpr Vec3 kUp{{0, 0, 1}};
constexpr uint64_t kShootSeedMultiplier = 0x9e3779b97f4a7c15ULL;

double GoldenAngle() noexcept {
  return std::numbers::pi * (3 - std::sqrt(5));
}

double Azimuth(const PlantLevel &level, size_t slot) noexcept {
  if (level.WhorlSize == 1) { return static_cast<double>(slot) * GoldenAngle(); }
  const size_t group = slot / level.WhorlSize;
  return static_cast<double>(group) * GoldenAngle() +
         static_cast<double>(slot % level.WhorlSize) *
             (2 * std::numbers::pi / static_cast<double>(level.WhorlSize));
}

void AxisPath(const PlantRecipe &recipe, size_t depth, PlantAxis &axis, uint64_t ordinal) noexcept {
  for (size_t at = depth; at > 0; --at) {
    axis.Path[at - 1] = static_cast<int16_t>(ordinal % recipe.Levels[at].Branches);
    ordinal /= recipe.Levels[at].Branches;
  }
}

bool PositionAxis(PlantAxis &axis,
                  const PlantAxis &parent,
                  const PlantLevel &level,
                  double parentPhase,
                  PlantRandom &random,
                  size_t slot) {
  const double jitter = level.AttachmentJitter > 0 ? random.Uniform(-1, 1) : 0;
  const size_t groups = (level.Branches - 1) / level.WhorlSize + 1;
  const double fraction = std::clamp(PlantLevelFraction(slot, level, 0.5) +
                                         jitter * level.AttachmentJitter /
                                             static_cast<double>(std::max(size_t{1}, groups - 1)),
                                     0.0,
                                     1.0);
  axis.ParentParameter = level.First + (level.Last - level.First) * fraction;
  const auto arc =
      PlantArc{.LengthM = parent.LengthM, .BendRad = parent.Bend}.FrameAt(axis.ParentParameter);
  const double angle = parentPhase + Azimuth(level, slot) +
                       random.Uniform(-level.AzimuthVariationRad, level.AzimuthVariationRad);
  const Vec3 radial = arc.Lateral * std::cos(angle) + arc.Normal * std::sin(angle);
  const double inclination = level.AngleRad +
                             (level.TipAngleRad.value_or(level.AngleRad) - level.AngleRad) *
                                 PlantLevelFraction(slot, level, 0.5) +
                             random.Uniform(-level.AngleVariationRad, level.AngleVariationRad);
  Vec3 direction = PlantRotate(
      parent.Frame, arc.Tangent * std::cos(inclination) + radial * std::sin(inclination));
  if (level.UpwardBias > 0) {
    direction = direction * (1 - level.UpwardBias) + kUp * level.UpwardBias;
    direction = direction * (1.0 / Length(direction));
  }
  const auto frame = PlantNormalFrame(direction, kUp);
  if (!frame) { return false; }
  axis.Frame = *frame;
  axis.OriginM = (Vec3{parent.OriginM} + PlantRotate(parent.Frame, arc.Centre)).Axis;
  axis.Parent = static_cast<int64_t>(parent.Id);
  return true;
}

bool BuildAxes(const PlantRecipe &recipe,
               const PlantRecipeLayout &layout,
               std::vector<PlantAxis> &axes) {
  std::vector<double> phases(layout.Size.Axes);
  axes.reserve(layout.Size.Axes);
  for (size_t depth = 0; depth < recipe.Levels.size(); ++depth) {
    const auto &level = recipe.Levels[depth];
    const auto &range = layout.Levels[depth];
    for (uint64_t at = 0; at < range.Count; ++at) {
      const uint64_t ordinal = range.FirstOrdinal + at;
      PlantAxis axis;
      axis.Id = range.AxisOffset + ordinal;
      AxisPath(recipe, depth, axis, ordinal);
      const size_t slot = depth == 0 ? 0 : static_cast<size_t>(ordinal % level.Branches);
      axis.LengthM = PlantLevelLength(level, slot);
      axis.RadiiM = {level.BaseRadiusM, level.TipRadiusM};
      PlantRandom random(recipe.Seed, axis.Id);
      const double phase = random.Uniform(-std::numbers::pi, std::numbers::pi);
      axis.Bend = level.BendRad + random.Uniform(-level.BendVariationRad, level.BendVariationRad);
      if (depth == 0) {
        const auto frame = PlantNormalFrame(kUp, {{std::cos(phase), std::sin(phase), 0}});
        if (!frame) { return false; }
        axis.Frame = *frame;
      } else {
        const auto &previous = layout.Levels[depth - 1];
        const size_t parent = previous.FirstRow +
                              static_cast<size_t>(ordinal / level.Branches - previous.FirstOrdinal);
        if (!PositionAxis(axis, axes[parent], level, phases[parent], random, slot)) {
          return false;
        }
      }
      phases[axes.size()] = phase;
      axes.push_back(axis);
    }
  }
  return true;
}

double LeafIncrement(PlantArrangement arrangement) noexcept {
  if (arrangement == PlantArrangement::Distichous) { return std::numbers::pi; }
  if (arrangement == PlantArrangement::Opposite) { return 0; }
  if (arrangement == PlantArrangement::Decussate) { return std::numbers::pi / 2; }
  return GoldenAngle();
}

bool LeafAt(PlantLeaf &leaf,
            const PlantAxis &axis,
            const PlantLevel &level,
            const PlantBlade &blade,
            double angle) {
  const auto local = PlantShoot{.LengthM = axis.LengthM,
                                .BendRad = axis.Bend,
                                .PetioleM = blade.PetioleM,
                                .LeafAngleRad = level.LeafAngleRad,
                                .NormalAlignment = level.LeafSkyAlignment,
                                .NormalHint = {axis.Frame[6], axis.Frame[7], axis.Frame[8]}}
                         .LeafAt({.Parameter = leaf.StemParameter, .AzimuthRad = angle});
  if (!local) { return false; }
  leaf.Frame = PlantComposeFrames(axis.Frame, local->Frame);
  leaf.NodeM = (PlantRotate(axis.Frame, Vec3{local->NodeM}) + Vec3{axis.OriginM}).Axis;
  leaf.OriginM = (PlantRotate(axis.Frame, Vec3{local->OriginM}) + Vec3{axis.OriginM}).Axis;
  return true;
}

bool BuildLeaves(const PlantRecipe &recipe,
                 const PlantRecipeLayout &layout,
                 const std::vector<PlantAxis> &axes,
                 std::vector<PlantLeaf> &leaves) {
  leaves.reserve(layout.Size.Leaves);
  for (size_t depth = layout.Depth; depth < recipe.Levels.size(); ++depth) {
    const auto &level = recipe.Levels[depth];
    const auto &range = layout.Levels[depth];
    if (!level.LeafInternodeM) { continue; }
    const double internode = *level.LeafInternodeM;
    const size_t per = PlantLeavesPerGroup(level.Arrangement);
    for (uint64_t at = 0; at < range.Count; ++at) {
      const uint64_t ordinal = range.FirstOrdinal + at;
      const auto slot = static_cast<size_t>(ordinal % level.Branches);
      const uint64_t count = range.LeafPrefix[slot + 1] - range.LeafPrefix[slot];
      if (count == 0) { continue; }
      const auto &axis = axes[range.FirstRow + static_cast<size_t>(at)];
      const uint64_t first = range.LeafOffset + PlantLeafPrefix(level, range, ordinal);
      PlantRandom random(recipe.Seed ^ ((axis.Id + 1) * kShootSeedMultiplier));
      const double phase = random.Uniform(-std::numbers::pi, std::numbers::pi);
      for (uint64_t leafAt = 0; leafAt < count; ++leafAt) {
        PlantLeaf leaf;
        leaf.Id = first + leafAt;
        leaf.Axis = axis.Id;
        leaf.Variant = static_cast<uint8_t>(
            (leaf.Id % recipe.Blades.size() + recipe.Seed % recipe.Blades.size()) %
            recipe.Blades.size());
        const uint64_t group = leafAt / per;
        leaf.StemParameter = level.LeafLast - static_cast<double>(group) * internode / axis.LengthM;
        const double angle =
            phase + static_cast<double>(group) * LeafIncrement(level.Arrangement) +
            static_cast<double>(leafAt % per) * 2 * std::numbers::pi / static_cast<double>(per);
        if (!LeafAt(leaf, axis, level, recipe.Blades.front(), angle)) { return false; }
        leaves.push_back(leaf);
      }
    }
  }
  return true;
}
}

std::expected<PlantRecipeSize, std::string_view> MeasurePlantRecipe(const PlantRecipe &recipe,
                                                                    PlantRecipeDemand demand) {
  const auto layout = LayoutPlantRecipe(recipe, demand);
  if (!layout) { return std::unexpected(layout.error()); }
  return layout->Size;
}

std::expected<PlantPlan, std::string_view> BuildPlantPlan(const PlantRecipe &recipe,
                                                          PlantRecipeDemand demand) {
  const auto layout = LayoutPlantRecipe(recipe, demand);
  if (!layout) { return std::unexpected(layout.error()); }
  std::vector<PlantAxis> axes;
  std::vector<PlantLeaf> leaves;
  if (!BuildAxes(recipe, *layout, axes) || !BuildLeaves(recipe, *layout, axes, leaves)) {
    return std::unexpected("plant recipe frame is not representable");
  }
  return PlantPlan::Create(std::move(axes), std::move(leaves), recipe.Blades.size());
}
}

#include "UrbanAreaGroups.h"
#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <string_view>
#include <vector>

namespace outshine::Generators::Osm {
std::expected<UrbanAreaGroups, std::string_view> UrbanAreaGroups::Make(const UrbanArea &area,
                                                                       Layout layout) {
  const uint64_t side = uint64_t{area.Recipe.PatchSide} * layout.PatchesPerSide;
  if (!area.Recipe.Valid() || layout.PatchesPerSide == 0 ||
      side > std::numeric_limits<uint32_t>::max() || ((area.Columns == 0) != (area.Rows == 0))) {
    return std::unexpected("invalid intrinsic urban group layout");
  }
  UrbanAreaGroups result;
  result.Columns_ = area.Columns;
  result.Rows_ = area.Rows;
  result.PatchSide_ = area.Recipe.PatchSide;
  result.GroupSide_ = static_cast<uint32_t>(side);
  result.Across_ = area.Columns == 0 ? 0 : (area.Columns - 1) / result.GroupSide_ + 1;
  result.Down_ = area.Rows == 0 ? 0 : (area.Rows - 1) / result.GroupSide_ + 1;
  if (uint64_t{result.Across_} * result.Down_ > layout.GroupsMost) {
    return std::unexpected("urban group metadata exceeds demand budget");
  }
  return result;
}

size_t UrbanAreaGroups::Size() const noexcept {
  return static_cast<size_t>(Across_) * Down_;
}

std::optional<UrbanPatchRange> UrbanAreaGroups::At(size_t index) const noexcept {
  if (index >= Size()) { return std::nullopt; }
  const auto column = static_cast<uint32_t>(index % Across_) * GroupSide_;
  const auto row = static_cast<uint32_t>(index / Across_) * GroupSide_;
  return UrbanPatchRange{.Column = column,
                         .Row = row,
                         .Columns = std::min(GroupSide_, Columns_ - column),
                         .Rows = std::min(GroupSide_, Rows_ - row)};
}

std::expected<std::vector<size_t>, std::string_view>
UrbanAreaGroups::Occupied(const UrbanArea &area) const {
  if (!area.Recipe.Valid() || area.Columns != Columns_ || area.Rows != Rows_ ||
      area.Recipe.PatchSide != PatchSide_ || area.Tiers.size() != uint64_t{Columns_} * Rows_) {
    return std::unexpected("urban occupancy requires its complete matching field");
  }
  std::vector<size_t> occupied;
  const std::span tiers{area.Tiers};
  for (size_t index = 0; index < Size(); ++index) {
    const auto range = At(index);
    if (!range) { return std::unexpected("urban group has no intrinsic range"); }
    for (size_t row = range->Row; row < range->Row + range->Rows; ++row) {
      const auto cells = tiers.subspan(row * Columns_ + range->Column, range->Columns);
      if (std::ranges::any_of(cells, [](uint8_t tier) { return tier != 0; })) {
        occupied.push_back(index);
        break;
      }
    }
  }
  return occupied;
}

std::expected<std::vector<UrbanPatchRange>, std::string_view>
UrbanAreaGroups::Patches(Selection selection) const {
  const auto group = At(selection.Index);
  if (!group) { return std::unexpected("urban group is outside its intrinsic layout"); }
  const uint64_t across = (group->Columns - 1) / PatchSide_ + 1;
  const uint64_t down = (group->Rows - 1) / PatchSide_ + 1;
  if (across * down > selection.PatchesMost) {
    return std::unexpected("selected urban group exceeds patch budget");
  }
  std::vector<UrbanPatchRange> patches;
  patches.reserve(static_cast<size_t>(across * down));
  for (uint64_t row = 0; row < group->Rows; row += PatchSide_) {
    for (uint64_t column = 0; column < group->Columns; column += PatchSide_) {
      const auto x = static_cast<uint32_t>(column);
      const auto y = static_cast<uint32_t>(row);
      patches.push_back({.Column = group->Column + x,
                         .Row = group->Row + y,
                         .Columns = std::min(PatchSide_, group->Columns - x),
                         .Rows = std::min(PatchSide_, group->Rows - y)});
    }
  }
  return patches;
}
}

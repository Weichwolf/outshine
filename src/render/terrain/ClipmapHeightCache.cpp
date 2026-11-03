#include "ClipmapHeightCache.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <optional>
#include <span>
#include <vector>

namespace outshine::Render {
namespace {
std::array<ClipmapHeightRegion, 2>
Bands(std::optional<ClipmapNode> previous, ClipmapNode low, uint32_t side, uint32_t level) {
  if (!previous || low[0] >= (*previous)[0] + side || (*previous)[0] >= low[0] + side ||
      low[1] >= (*previous)[1] + side || (*previous)[1] >= low[1] + side) {
    return {{{.Low = low, .Level = level, .Width = side, .Height = side}, {}}};
  }
  const int64_t dx = low[0] - (*previous)[0];
  const int64_t dy = low[1] - (*previous)[1];
  const auto width = static_cast<uint32_t>(std::abs(dx));
  const auto height = static_cast<uint32_t>(std::abs(dy));
  return {{{.Low = {low[0] + (dx > 0 ? side - dx : 0), low[1]},
            .Level = level,
            .Width = width,
            .Height = side},
           {.Low = {low[0] + std::max(-dx, int64_t{0}), low[1] + (dy > 0 ? side - dy : 0)},
            .Level = level,
            .Width = side - width,
            .Height = height}}};
}

size_t Samples(const ClipmapHeightRegion &region) {
  return static_cast<size_t>(region.Width) * region.Height;
}
}

std::expected<ClipmapHeightCache, ClipmapHeightError> ClipmapHeightCache::Make(ClipmapGrid grid,
                                                                               size_t bytesMost) {
  const uint64_t side = 2 * uint64_t{grid.HalfCells} + 3;
  if (grid.HalfCells < 8 || (grid.HalfCells & 1u) != 0 || grid.Levels == 0 ||
      grid.Levels > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      !std::isfinite(grid.CellM) || grid.CellM <= 0 ||
      side > std::numeric_limits<uint32_t>::max() ||
      !std::isfinite(std::ldexp(grid.CellM, static_cast<int>(grid.Levels - 1)) *
                     static_cast<double>(side))) {
    return std::unexpected(ClipmapHeightError::InvalidGrid);
  }
  const size_t levelBudget = bytesMost / grid.Levels;
  if (levelBudget < sizeof(Level) || side > (levelBudget - sizeof(Level)) / sizeof(float) / side) {
    return std::unexpected(ClipmapHeightError::OverBudget);
  }
  ClipmapHeightCache cache;
  cache.Grid_ = grid;
  cache.Side_ = static_cast<uint32_t>(side);
  cache.Levels_.resize(grid.Levels);
  for (uint32_t i = 0; i < grid.Levels; ++i) {
    cache.Levels_[i].Step = std::ldexp(grid.CellM, static_cast<int>(i));
    cache.Levels_[i].Values.resize(static_cast<size_t>(side * side),
                                   std::numeric_limits<float>::quiet_NaN());
  }
  return cache;
}

std::expected<ClipmapNode, ClipmapHeightError> ClipmapHeightCache::Low(Vec2 camera,
                                                                       double step) const {
  ClipmapNode low{};
  for (size_t axis = 0; axis < low.size(); ++axis) {
    const double units = std::floor((camera[axis] / step) * 0.5);
    constexpr int64_t halfDomain = std::numeric_limits<int64_t>::max() / 2;
    const auto limit = static_cast<double>(halfDomain - Side_);
    if (!std::isfinite(units) || std::abs(units) >= limit) {
      return std::unexpected(ClipmapHeightError::InvalidCoordinate);
    }
    low[axis] = static_cast<int64_t>(units) * 2 - Grid_.HalfCells - 1;
    if (!std::isfinite(static_cast<double>(low[axis] + Side_) * step) ||
        !std::isfinite(static_cast<double>(low[axis]) * step)) {
      return std::unexpected(ClipmapHeightError::InvalidCoordinate);
    }
  }
  return low;
}

std::expected<ClipmapHeightCache::WorkSize, ClipmapHeightError>
ClipmapHeightCache::Measure(Vec2 camera, size_t bytesMost) const {
  if (Levels_.empty()) { return std::unexpected(ClipmapHeightError::InvalidGrid); }
  size_t count = 0;
  size_t regions = 0;
  for (uint32_t i = 0; i < Levels_.size(); ++i) {
    const auto low = Low(camera, Levels_[i].Step);
    if (!low) { return std::unexpected(low.error()); }
    for (const auto &region : Bands(Levels_[i].Low, *low, Side_, i)) {
      const size_t samples = Samples(region);
      if (samples == 0) { continue; }
      if (regions == bytesMost / sizeof(ClipmapHeightRegion)) {
        return std::unexpected(ClipmapHeightError::OverBudget);
      }
      ++regions;
      const size_t remainder = bytesMost - regions * sizeof(ClipmapHeightRegion);
      if (count > remainder / (sizeof(Vec2) + sizeof(float)) ||
          samples > remainder / (sizeof(Vec2) + sizeof(float)) - count) {
        return std::unexpected(ClipmapHeightError::OverBudget);
      }
      count += samples;
    }
  }
  return WorkSize{.Samples = count, .Regions = regions};
}

std::expected<ClipmapHeightUpdate, ClipmapHeightError>
ClipmapHeightCache::Plan(Vec2 camera, size_t bytesMost) const {
  const auto size = Measure(camera, bytesMost);
  if (!size) { return std::unexpected(size.error()); }
  ClipmapHeightUpdate update;
  update.Owner_ = this;
  update.Generation_ = Generation_;
  update.Camera_ = camera;
  update.Positions_.reserve(size->Samples);
  update.Samples_.resize(size->Samples, std::numeric_limits<float>::quiet_NaN());
  update.Regions_.reserve(size->Regions);
  for (uint32_t i = 0; i < Levels_.size(); ++i) {
    const auto low = *Low(camera, Levels_[i].Step);
    for (const auto &region : Bands(Levels_[i].Low, low, Side_, i)) {
      if (Samples(region) == 0) { continue; }
      update.Regions_.push_back(region);
      for (uint32_t y = 0; y < region.Height; ++y) {
        for (uint32_t x = 0; x < region.Width; ++x) {
          update.Positions_.push_back({{static_cast<double>(region.Low[0] + x) * Levels_[i].Step,
                                        static_cast<double>(region.Low[1] + y) * Levels_[i].Step}});
        }
      }
    }
  }
  return update;
}

size_t ClipmapHeightCache::Slot(ClipmapNode node) const noexcept {
  std::array<size_t, 2> wrapped{};
  for (size_t i = 0; i < wrapped.size(); ++i) {
    const int64_t part = node[i] % Side_;
    wrapped[i] = static_cast<size_t>(part < 0 ? part + Side_ : part);
  }
  return wrapped[1] * Side_ + wrapped[0];
}

std::expected<void, ClipmapHeightError>
ClipmapHeightCache::Validate(const ClipmapHeightUpdate &update) const noexcept {
  if (Levels_.empty() || update.Owner_ != this || update.Generation_ != Generation_) {
    return std::unexpected(ClipmapHeightError::StaleUpdate);
  }
  if (std::ranges::any_of(update.Samples_, [](float height) { return std::isinf(height); })) {
    return std::unexpected(ClipmapHeightError::InvalidSamples);
  }
  return {};
}

std::expected<void, ClipmapHeightError>
ClipmapHeightCache::Commit(const ClipmapHeightUpdate &update) noexcept {
  const auto valid = Validate(update);
  if (!valid) { return valid; }
  size_t at = 0;
  for (const auto &region : update.Regions_) {
    for (uint32_t y = 0; y < region.Height; ++y) {
      for (uint32_t x = 0; x < region.Width; ++x) {
        Levels_[region.Level].Values[Slot({region.Low[0] + x, region.Low[1] + y})] =
            update.Samples_[at++];
      }
    }
  }
  for (auto &level : Levels_) { level.Low = *Low(update.Camera_, level.Step); }
  if (!update.Samples_.empty()) { ++Generation_; }
  return {};
}

std::optional<float> ClipmapHeightCache::Height(uint32_t level, ClipmapNode node) const noexcept {
  if (level >= Levels_.size()) { return std::nullopt; }
  const auto &held = Levels_[level];
  if (!held.Low) { return std::nullopt; }
  const auto low = *held.Low;
  if (node[0] < low[0] || node[0] >= low[0] + Side_ || node[1] < low[1] ||
      node[1] >= low[1] + Side_) {
    return std::nullopt;
  }
  const float height = Levels_[level].Values[Slot(node)];
  return std::isfinite(height) ? std::optional{height} : std::nullopt;
}

std::span<const float> ClipmapHeightCache::Values(uint32_t level) const noexcept {
  return level < Levels_.size() ? std::span(Levels_[level].Values) : std::span<const float>{};
}

std::optional<ClipmapNode> ClipmapHeightCache::Origin(uint32_t level) const noexcept {
  return level < Levels_.size() ? Levels_[level].Low : std::nullopt;
}

size_t ClipmapHeightCache::HeapBytes() const noexcept {
  size_t bytes = Levels_.capacity() * sizeof(Level);
  for (const auto &level : Levels_) { bytes += level.Values.capacity() * sizeof(float); }
  return bytes;
}
}

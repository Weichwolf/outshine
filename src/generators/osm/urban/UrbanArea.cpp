#include "UrbanAreaDomain.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <span>
#include <string>
#include <utility>
#include <vector>

namespace outshine::Generators::Osm {
namespace {
enum class NoiseStream : uint64_t { Vacancy = 1105, Height = 708 };
constexpr uint64_t kEastMultiplier = 0x9e3779b97f4a7c15;
constexpr uint64_t kNorthMultiplier = 0xd1b54a32d192ed03;
constexpr uint64_t kMixFirst = 0xbf58476d1ce4e5b9;
constexpr uint64_t kMixSecond = 0x94d049bb133111eb;
constexpr unsigned kShiftFirst = 30, kShiftSecond = 27, kShiftFinal = 31;
constexpr uint64_t kPercentageScale = 100;
constexpr uint64_t kVacancyPercent = 15;
constexpr unsigned kUnitIntervalShift = 11;
constexpr std::array kHeightQuantiles{.62, .90, .98};

uint64_t CellHash(std::array<int64_t, 2> cell, NoiseStream stream) {
  uint64_t bits = static_cast<uint64_t>(cell[0]) * kEastMultiplier;
  bits ^= static_cast<uint64_t>(cell[1]) * kNorthMultiplier;
  bits ^= static_cast<uint64_t>(stream);
  bits = (bits ^ (bits >> kShiftFirst)) * kMixFirst;
  bits = (bits ^ (bits >> kShiftSecond)) * kMixSecond;
  return bits ^ (bits >> kShiftFinal);
}

int64_t FloorDiv(int64_t value, int64_t divisor) {
  return value / divisor - (value % divisor < 0 ? 1 : 0);
}

struct CoverageBlocks {
  int64_t X = 0, Y = 0, Columns = 0, Rows = 0, Side = 0;
  std::vector<bool> Covered;
};

std::expected<CoverageBlocks, std::string> CoveredBlocks(const UrbanDetail::Domain &domain,
                                                         const UrbanArea &area,
                                                         const UrbanAreaLimits &limits) {
  constexpr int64_t coverageSideMost = 4;
  const int64_t side = std::min<int64_t>(area.Recipe.PatchSide, coverageSideMost);
  const int64_t x0 = area.FirstCell[0];
  const int64_t y1 = area.FirstCell[1];
  CoverageBlocks grid;
  grid.Side = side;
  grid.X = FloorDiv(x0, side);
  grid.Y = FloorDiv(y1 - area.Rows + 1, side);
  grid.Columns = FloorDiv(x0 + area.Columns - 1, side) - grid.X + 1;
  grid.Rows = FloorDiv(y1, side) - grid.Y + 1;
  grid.Covered.resize(static_cast<size_t>(grid.Columns * grid.Rows));
  const double span = static_cast<double>(side) * area.Recipe.ProjectedPitchM;
  const double padding = area.Recipe.ProjectedPitchM * (1 - area.Recipe.FillFraction) / 2;
  for (int64_t row = 0; row < grid.Rows; ++row) {
    if (UrbanDetail::Expired(limits)) {
      return std::unexpected("urban generation cancelled or expired");
    }
    for (int64_t column = 0; column < grid.Columns; ++column) {
      const double left = static_cast<double>(grid.X + column) * span;
      const double bottom = static_cast<double>(grid.Y + row) * span;
      const double margin = 64 * std::numeric_limits<double>::epsilon() *
                            std::max({1.,
                                      std::abs(left),
                                      std::abs(bottom),
                                      std::abs(left + span),
                                      std::abs(bottom + span)});
      const auto covered = domain.Covers({left + padding - margin,
                                          bottom + padding - margin,
                                          left + span - padding + margin,
                                          bottom + span - padding + margin});
      if (!covered) { return std::unexpected(covered.error()); }
      grid.Covered[static_cast<size_t>(row * grid.Columns + column)] = *covered;
    }
  }
  return grid;
}

std::expected<bool, std::string> CoveredCell(const UrbanDetail::Domain &domain,
                                             UrbanArea &area,
                                             const CoverageBlocks &blocks,
                                             std::array<int64_t, 2> cell) {
  const auto px = FloorDiv(cell[0], blocks.Side) - blocks.X;
  const auto py = FloorDiv(cell[1], blocks.Side) - blocks.Y;
  if (blocks.Covered[static_cast<size_t>(py * blocks.Columns + px)]) { return true; }
  ++area.LotTests;
  const double pitch = area.Recipe.ProjectedPitchM;
  const double half = pitch * area.Recipe.FillFraction / 2;
  const double x = (static_cast<double>(cell[0]) + .5) * pitch;
  const double y = (static_cast<double>(cell[1]) + .5) * pitch;
  return domain.Covers({x - half, y - half, x + half, y + half});
}

std::expected<void, std::string>
Fill(UrbanArea &area, const UrbanDetail::Domain &domain, const UrbanAreaLimits &limits) {
  const auto blocks = CoveredBlocks(domain, area, limits);
  if (!blocks) { return std::unexpected(blocks.error()); }
  area.PatchTests = blocks->Covered.size();
  for (uint32_t row = 0; row < area.Rows; ++row) {
    if (UrbanDetail::Expired(limits)) {
      return std::unexpected("urban generation cancelled or expired");
    }
    const int64_t y = area.FirstCell[1] - row;
    for (uint32_t column = 0; column < area.Columns; ++column) {
      const std::array<int64_t, 2> cell{area.FirstCell[0] + column, y};
      if (CellHash(cell, NoiseStream::Vacancy) % kPercentageScale < kVacancyPercent) { continue; }
      const auto covered = CoveredCell(domain, area, *blocks, cell);
      if (!covered) { return std::unexpected(covered.error()); }
      if (!*covered) { continue; }
      const double random =
          static_cast<double>(CellHash(cell, NoiseStream::Height) >> kUnitIntervalShift) * 0x1p-53;
      const auto tier = static_cast<uint8_t>(std::ranges::lower_bound(kHeightQuantiles, random) -
                                             kHeightQuantiles.begin() + 1);
      area.Tiers[static_cast<size_t>(row) * area.Columns + column] = tier;
      ++area.Occupied;
    }
  }
  return {};
}

bool Valid(UrbanAreaRecipe recipe, const UrbanAreaLimits &limits) {
  return recipe.Valid() && limits.CellsMost > 0 && limits.CellsMost <= 4194304;
}
}

std::expected<UrbanArea, std::string> GenerateUrbanArea(std::span<const OsmField *const> inputs,
                                                        UrbanAreaRecipe recipe,
                                                        const UrbanAreaLimits &limits) {
  if (!Valid(recipe, limits)) { return std::unexpected("invalid bounded urban recipe"); }
  if (inputs.empty() || inputs.front() == nullptr || inputs.front()->Tiles().size() != 1) {
    return std::unexpected("urban generation requires one owner tile");
  }
  const auto frame = UrbanDetail::FrameFor(inputs.front()->Tiles().front());
  const double pitch = recipe.ProjectedPitchM;
  const auto x0 = static_cast<int64_t>(std::ceil(frame.Bounds[0] / pitch - .5));
  const auto x1 = static_cast<int64_t>(std::ceil(frame.Bounds[2] / pitch - .5));
  const auto y0 = static_cast<int64_t>(std::ceil(frame.Bounds[1] / pitch - .5));
  const auto y1 = static_cast<int64_t>(std::ceil(frame.Bounds[3] / pitch - .5));
  const auto columns = static_cast<uint64_t>(x1 - x0);
  const auto rows = static_cast<uint64_t>(y1 - y0);
  if (rows > limits.CellsMost || columns > limits.CellsMost ||
      (rows != 0 && columns > limits.CellsMost / rows)) {
    return std::unexpected("urban cell work budget exceeded before geometry work");
  }
  const auto built = UrbanDetail::BuildDomain(inputs, frame, limits);
  if (!built) { return std::unexpected(built.error()); }
  const auto &domain = **built;
  UrbanArea result;
  result.Recipe = recipe;
  result.MetresPerProjectedMetre = frame.Scale;
  result.ProjectedOriginM = {frame.Bounds[0], frame.Bounds[3]};
  result.Sources = domain.Sources;
  result.HasUrbanEvidence = domain.HasUrbanEvidence;
  for (const auto *input : inputs) {
    const auto &tile = input->Tiles().front();
    result.Inputs.push_back({.Identity = tile.Source, .PayloadDigest = tile.InputDigest});
  }
  const char empty = GEOSisEmpty_r(domain.Context, domain.Allowed.get());
  if (empty == 2) { return std::unexpected("urban empty-domain query failed"); }
  if (empty == 1) { return result; }
  result.Columns = static_cast<uint32_t>(columns);
  result.Rows = static_cast<uint32_t>(rows);
  result.FirstCell = {x0, y1 - 1};
  result.Tiers.resize(static_cast<size_t>(rows * columns));
  if (rows != 0 && columns != 0) {
    const auto filled = Fill(result, domain, limits);
    if (!filled) { return std::unexpected(filled.error()); }
  }
  return result;
}
}

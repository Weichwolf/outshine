#include "Check.h"
#include "PrismViewFixture.h"
#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <random>
#include <span>
#include <vector>

namespace {
using namespace outshine;

bool Matches(const PrismGrid &source,
             const PrismGridViews &views,
             std::span<const PrismCellRect> cuts) {
  const auto &original = source.Patches.front();
  std::vector<uint8_t> visits(static_cast<size_t>(original.Columns) * original.Rows);
  for (size_t index = 0; index < views.Placements.size(); ++index) {
    const auto &where = views.Placements[index];
    const auto &patch = views.Geometry.Patches[index];
    if (where.Patch != 0 || where.Column > original.Columns || where.Row > original.Rows ||
        patch.Columns > original.Columns - where.Column || patch.Rows > original.Rows - where.Row) {
      return false;
    }
    for (uint32_t row = 0; row < patch.Rows; ++row) {
      for (uint32_t column = 0; column < patch.Columns; ++column) {
        const size_t cell =
            static_cast<size_t>(row + where.Row) * original.Columns + column + where.Column;
        if (patch.FirstSample + static_cast<size_t>(row) * patch.Columns + column !=
            original.FirstSample + cell) {
          return false;
        }
        ++visits[cell];
      }
    }
  }
  for (uint32_t row = 0; row < original.Rows; ++row) {
    for (uint32_t column = 0; column < original.Columns; ++column) {
      bool covered = false;
      for (const auto &cut : cuts) {
        covered = covered ||
                  (column >= cut.Left && column < cut.Right && row >= cut.Top && row < cut.Bottom);
      }
      if (visits[static_cast<size_t>(row) * original.Columns + column] !=
          static_cast<uint8_t>(!covered)) {
        return false;
      }
    }
  }
  return true;
}
}

int main() {
  using namespace outshine::Test;
  auto source = PrismViewFixture::Grid(17, 13);
  std::minstd_rand random(8181);
  std::uniform_int_distribution<uint32_t> x(0, 22), y(0, 17);
  for (size_t example = 0; example < 256; ++example) {
    std::array<PrismCellRect, 16> cuts;
    for (auto &cut : cuts) {
      const auto left = x(random), right = x(random), top = y(random), bottom = y(random);
      cut = {.Left = std::min(left, right),
             .Top = std::min(top, bottom),
             .Right = std::max(left, right),
             .Bottom = std::max(top, bottom)};
    }
    const auto result = ExcludePrismCells(
        source, cuts, {.InputRecordsMost = 17, .ViewsMost = 1000, .IntersectionTestsMost = 1000});
    CHECK(result && result->Geometry.Samples == source.Samples && Matches(source, *result, cuts),
          "deterministic overlapping, empty, touching and outside rectangle unions match direct "
          "cell membership");
  }
  const std::array invalid{PrismCellRect{.Patch = 1, .Right = 1, .Bottom = 1}};
  auto result = ExcludePrismCells(
      source, invalid, {.InputRecordsMost = 2, .ViewsMost = 10, .IntersectionTestsMost = 10});
  CHECK(!result && result.error() == PrismViewError::InvalidExclusion,
        "an unrelated patch index cannot remove cached content");
  const std::array reversed{PrismCellRect{.Left = 2, .Right = 1, .Bottom = 1}};
  result = ExcludePrismCells(
      source, reversed, {.InputRecordsMost = 2, .ViewsMost = 10, .IntersectionTestsMost = 10});
  CHECK(!result && result.error() == PrismViewError::InvalidExclusion,
        "inverted rectangles are refused");
  source.Patches.front().FirstSample = std::numeric_limits<uint64_t>::max();
  result = ExcludePrismCells(source, {}, {.InputRecordsMost = 1, .ViewsMost = 10});
  CHECK(!result && result.error() == PrismViewError::InvalidSource,
        "invalid shared sample ranges fail without unsigned address overflow");
  return Report();
}

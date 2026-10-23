#include "Check.h"
#include "PrismViewFixture.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <vector>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  auto samples = std::make_shared<std::vector<PrismSpan>>(64);
  for (size_t index = 0; index < samples->size(); ++index) {
    (*samples)[index] = {.Lower = static_cast<float>(index),
                         .Upper = static_cast<float>(index + 2)};
  }
  (*samples)[0].Lower = std::numeric_limits<float>::quiet_NaN();
  PrismGrid source{
      .Samples = samples,
      .Patches = {{.FirstSample = 3, .Columns = 6, .Rows = 5, .PitchX = 2, .PitchZ = 3},
                  {.FirstSample = 12, .Columns = 3, .Rows = 3, .PitchX = 5, .PitchZ = 7}}};
  const std::array cuts{PrismCellRect{.Patch = 0, .Left = 2, .Top = 1, .Right = 4, .Bottom = 4},
                        PrismCellRect{.Patch = 0, .Left = 0, .Top = 0, .Right = 3, .Bottom = 2},
                        PrismCellRect{.Patch = 0, .Left = 100, .Right = 200, .Bottom = 100},
                        PrismCellRect{.Patch = 1, .Left = 1, .Top = 1, .Right = 3, .Bottom = 3}};
  const auto planned = ExcludePrismCells(
      source, cuts, {.InputRecordsMost = 10, .ViewsMost = 100, .IntersectionTestsMost = 100});
  CHECK(planned, "overlapping and outside exclusions produce bounded native views");
  if (!planned) { return Report(); }
  CHECK(
      planned->Geometry.Samples == source.Samples &&
          planned->Placements.size() == planned->Geometry.Patches.size(),
      "views retain the same immutable sample allocation and explicit original placement offsets");
  CHECK(
      InspectPrismGrid(planned->Geometry),
      "the unchanged dense PrismGrid contract accepts cropped views without row-stride extensions");
  std::array<std::vector<uint8_t>, 2> visits{std::vector<uint8_t>(30), std::vector<uint8_t>(9)};
  for (size_t index = 0; index < planned->Placements.size(); ++index) {
    const auto &view = planned->Geometry.Patches[index];
    const auto &placement = planned->Placements[index];
    const auto &original = source.Patches[placement.Patch];
    CHECK(view.PitchX == original.PitchX && view.PitchZ == original.PitchZ &&
              view.FillFraction == original.FillFraction &&
              (view.Rows == 1 || (view.Columns == original.Columns && placement.Column == 0)),
          "partial columns use single-row views; full-width bands preserve dense multirow storage");
    for (uint32_t row = 0; row < view.Rows; ++row) {
      for (uint32_t column = 0; column < view.Columns; ++column) {
        const uint32_t x = placement.Column + column, y = placement.Row + row;
        const size_t cell = static_cast<size_t>(y) * original.Columns + x;
        ++visits[placement.Patch][cell];
        CHECK(view.FirstSample + static_cast<size_t>(row) * view.Columns + column ==
                  original.FirstSample + cell,
              "view sample addresses and original local-frame offsets identify the same prism");
      }
    }
  }
  for (size_t patch = 0; patch < source.Patches.size(); ++patch) {
    const auto &shape = source.Patches[patch];
    for (uint32_t row = 0; row < shape.Rows; ++row) {
      for (uint32_t column = 0; column < shape.Columns; ++column) {
        bool excluded = false;
        for (const auto &cut : cuts) {
          excluded = excluded || (cut.Patch == patch && column >= cut.Left && column < cut.Right &&
                                  row >= cut.Top && row < cut.Bottom);
        }
        CHECK(visits[patch][static_cast<size_t>(row) * shape.Columns + column] ==
                  static_cast<uint8_t>(!excluded),
              "independent per-cell membership finds exactly the complement, without holes or "
              "overlap");
      }
    }
  }
  CHECK(source.Patches.size() == 2 && source.Patches[0].Rows == 5 && source.Samples == samples &&
            planned->IntersectionTests <= 100,
        "the original cached product remains intact and intersection work stays within budget");
  return Report();
}

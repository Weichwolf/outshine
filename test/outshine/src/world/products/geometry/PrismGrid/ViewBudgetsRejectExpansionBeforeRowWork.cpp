#include "Check.h"
#include "PrismViewFixture.h"
#include <array>
#include <cstdint>
#include <limits>

int main() {
  using namespace outshine;
  using namespace outshine::Test;
  const auto source = PrismViewFixture::Grid(2, 4096);
  const std::array partial{PrismCellRect{.Right = 1, .Bottom = 4096}};
  auto views = ExcludePrismCells(
      source, partial, {.InputRecordsMost = 2, .ViewsMost = 64, .IntersectionTestsMost = 3});
  CHECK(!views && views.error() == PrismViewError::OverBudget,
        "a narrow tall window refuses its required 4096 records before enumerating rows");
  const std::array full{PrismCellRect{.Right = 2, .Bottom = 4096}};
  views = ExcludePrismCells(
      source, full, {.InputRecordsMost = 2, .ViewsMost = 0, .IntersectionTestsMost = 3});
  CHECK(views && views->Geometry.Patches.empty() && views->Placements.empty() &&
            views->IntersectionTests == 3 && views->Geometry.Samples == source.Samples,
        "fully replaced fields emit no views and do not iterate their rows or copy their samples");
  const std::array outside{PrismCellRect{.Left = 2,
                                         .Right = std::numeric_limits<uint32_t>::max(),
                                         .Bottom = std::numeric_limits<uint32_t>::max()}};
  views = ExcludePrismCells(
      source, outside, {.InputRecordsMost = 2, .ViewsMost = 1, .IntersectionTestsMost = 2});
  CHECK(views && views->Geometry.Patches.size() == 1 && views->Geometry.Patches[0].Rows == 4096 &&
            views->IntersectionTests == 2,
        "irrelevant exclusions retain the original dense patch without per-row work");
  views = ExcludePrismCells(source, {}, {.InputRecordsMost = 1, .ViewsMost = 1});
  CHECK(views && views->Geometry.Patches.size() == 1 && views->IntersectionTests == 0,
        "no exclusions take the direct identity path");
  CHECK(
      !ExcludePrismCells(source,
                         partial,
                         {.InputRecordsMost = 1, .ViewsMost = 8192, .IntersectionTestsMost = 100}),
      "input record limits apply before allocating a patch index");
  CHECK(!ExcludePrismCells(source,
                           partial,
                           {.InputRecordsMost = 2, .ViewsMost = 8192, .IntersectionTestsMost = 2}),
        "the next intersection band refuses before exceeding its test budget");
  return Report();
}

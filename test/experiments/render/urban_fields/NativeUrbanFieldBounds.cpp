#include "NativeUrbanField.h"
#include "Check.h"
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

int main() {
  using namespace UrbanControl;
  constexpr uint32_t rows = 624;
  constexpr uint32_t columns = 616;
  constexpr double fill = .75;
  CHECK(DimensionsFit(columns, rows), "the full stored field fits the existing byte budget");
  CHECK(!DimensionsFit(0, rows) && !DimensionsFit(columns, 0), "empty dimensions are invalid");
  CHECK(DimensionsFit(static_cast<uint32_t>(kCellsMost), 1), "the largest payload fits");
  CHECK(!DimensionsFit(static_cast<uint32_t>(kCellsMost + 1), 1), "one excess value is refused");
  CHECK(!DimensionsFit(std::numeric_limits<uint32_t>::max(), std::numeric_limits<uint32_t>::max()),
        "large products refuse before multiplication or allocation");
  HeightGrid field{
      .Columns = columns, .Rows = rows, .CellM = 32, .FillFraction = fill, .HeightsM = {}};
  field.HeightsM.resize(static_cast<size_t>(columns) * rows);
  CHECK(field.Valid(), "complete full-field storage is valid");
  field.HeightsM.pop_back();
  CHECK(!field.Valid(), "short storage stays invalid");
  field.HeightsM.resize(kCellsMost);
  outshine::BinaryValueWriter encoded(kBytes);
  CHECK(encoded(kFormat,
                static_cast<uint32_t>(kCellsMost),
                uint32_t{1},
                field.CellM,
                field.FillFraction) &&
            encoded.Array<float>(field.HeightsM),
        "the real binary writer accepts the largest complete payload");
  CHECK(std::move(encoded.Out).TakeBytes().size() == kBytes,
        "the derived capacity includes the actual array count and header");
  return outshine::Test::Report();
}

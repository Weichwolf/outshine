#ifndef OUTSHINE_SCENARIO_TABLES_H
#define OUTSHINE_SCENARIO_TABLES_H

#include <expected>
#include <functional>
#include <mdspan>
#include <string>
#include <string_view>
#include <unordered_map>
#include <span>
#include <vector>

#include <Scenario.h>

namespace outshine {

struct ByName {
  using is_transparent = void;
  [[nodiscard]] size_t operator()(std::string_view spelling) const {
    return std::hash<std::string_view>{}(spelling);
  }
};

class TableBook {
public:
  static constexpr size_t kMostRows = 4096;

  [[nodiscard]] static std::expected<TableBook, std::string> Stand(
      std::span<const Table> declared);

  [[nodiscard]] const double *Number(std::string_view table, std::string_view row,
                                     std::string_view column) const;
  [[nodiscard]] const std::string *Text(std::string_view table, std::string_view row,
                                        std::string_view column) const;

  [[nodiscard]] size_t TableCount() const { return Held_.size(); }

private:
  TableBook() = default;

  struct Cell {
    std::string Spelling;
    double Value = 0.0;
  };
  struct Stood {
    std::vector<std::string> Columns;
    std::vector<bool> Numeric;
    std::vector<Cell> Cells;
    size_t RowCount = 0;
    std::unordered_map<std::string, size_t, ByName, std::equal_to<>> ByKey;

    using Grid = std::mdspan<const Cell, std::dextents<size_t, 2>>;
    [[nodiscard]] Grid Rows() const { return Grid(Cells.data(), RowCount, Columns.size()); }
  };
  [[nodiscard]] const Cell *At(std::string_view table, std::string_view row,
                               std::string_view column, bool wantNumber) const;
  std::unordered_map<std::string, Stood, ByName, std::equal_to<>> Held_;
};

}
#endif

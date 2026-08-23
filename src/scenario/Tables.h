#ifndef OUTSHINE_SCENARIO_TABLES_H
#define OUTSHINE_SCENARIO_TABLES_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <span>
#include <vector>

#include <outshine/Scenario.h>

namespace outshine {

class TableBook {
public:
  static constexpr size_t kMostRows = 4096;

  [[nodiscard]] bool Build(std::span<const Table> declared, std::string &error);

  [[nodiscard]] const double *Number(std::string_view table, std::string_view row,
                                     std::string_view column) const;
  [[nodiscard]] const std::string *Text(std::string_view table, std::string_view row,
                                        std::string_view column) const;

  [[nodiscard]] size_t TableCount() const { return Held_.size(); }

private:
  struct Cell {
    std::string Spelling;
    double Value = 0.0;
  };
  struct Stood {
    std::vector<std::string> Columns;
    std::vector<bool> Numeric;
    std::vector<std::vector<Cell>> Rows;
    std::unordered_map<std::string, size_t> ByKey;
  };
  [[nodiscard]] const Cell *At(std::string_view table, std::string_view row,
                               std::string_view column, bool wantNumber) const;
  std::unordered_map<std::string, Stood> Held_;
};

}
#endif

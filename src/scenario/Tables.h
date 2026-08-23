#ifndef TABLES_H
#define TABLES_H

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <outshine/Scenario.h>

namespace outshine {

// the declared tables, stood up once: rows indexed by their first column, cells typed by
// their COLUMN -- where damage, loot, prices and every number a game balances live, kept
// out of the engine
class TableBook {
public:
  static constexpr size_t kMostRows = 4096; // [SET] a balance table is data, not a database

  [[nodiscard]] bool Build(const std::vector<Table> &declared, std::string &error);

  // a missing row or column answers NOTHING -- a missing damage row and a damage of zero
  // are different facts, so the answer is a pointer and never a default
  [[nodiscard]] const double *Number(std::string_view table, std::string_view row,
                                     std::string_view column) const;
  [[nodiscard]] const std::string *Text(std::string_view table, std::string_view row,
                                        std::string_view column) const;

  [[nodiscard]] size_t TableCount(void) const { return Held_.size(); }

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

#include "Tables.h"

#include <charconv>

namespace outshine {

bool TableBook::Build(const std::vector<Table> &declared, std::string &error) {
  Held_.clear();
  for (const Table &table : declared) {
    if (Held_.count(table.Id) != 0) {
      error = "the table '" + table.Id + "' is declared twice, and a number has one home";
      return false;
    }
    if (table.Rows.size() > kMostRows) {
      error = "the table '" + table.Id + "' declares " + std::to_string(table.Rows.size()) +
              " rows over the bound of " + std::to_string(kMostRows);
      return false;
    }
    Stood stood;
    stood.Columns = table.Columns;
    stood.Numeric.assign(table.Columns.size(), false);
    for (size_t at = 0; at < table.Types.size() && at < stood.Numeric.size(); ++at) {
      stood.Numeric[at] = table.Types[at];
    }
    for (const std::vector<std::string> &row : table.Rows) {
      if (row.size() != table.Columns.size()) {
        error = "the table '" + table.Id + "' has a row of " + std::to_string(row.size()) +
                " cells under " + std::to_string(table.Columns.size()) + " columns";
        return false;
      }
      std::vector<Cell> cells;
      cells.reserve(row.size());
      for (size_t at = 0; at < row.size(); ++at) {
        Cell cell;
        cell.Spelling = row[at];
        if (stood.Numeric[at]) {
          const auto scanned = std::from_chars(
              cell.Spelling.data(), cell.Spelling.data() + cell.Spelling.size(), cell.Value);
          if (scanned.ec != std::errc() ||
              scanned.ptr != cell.Spelling.data() + cell.Spelling.size()) {
            error = "the table '" + table.Id + "' holds '" + cell.Spelling +
                    "' in the number column '" + stood.Columns[at] +
                    "' -- a cell is typed by its column, and this one does not read";
            return false;
          }
        }
        cells.push_back(std::move(cell));
      }
      if (!cells.empty()) {
        if (!stood.ByKey.emplace(cells[0].Spelling, stood.Rows.size()).second) {
          error = "the table '" + table.Id + "' keys two rows by '" + cells[0].Spelling +
                  "', and a lookup with two answers has none";
          return false;
        }
      }
      stood.Rows.push_back(std::move(cells));
    }
    Held_.emplace(table.Id, std::move(stood));
  }
  return true;
}

const TableBook::Cell *TableBook::At(std::string_view table, std::string_view row,
                                     std::string_view column, bool wantNumber) const {
  const auto held = Held_.find(std::string(table));
  if (held == Held_.end()) { return nullptr; }
  const Stood &stood = held->second;
  const auto keyed = stood.ByKey.find(std::string(row));
  if (keyed == stood.ByKey.end()) { return nullptr; }
  for (size_t at = 0; at < stood.Columns.size(); ++at) {
    if (stood.Columns[at] != column) { continue; }
    if (stood.Numeric[at] != wantNumber) { return nullptr; }
    return &stood.Rows[keyed->second][at];
  }
  return nullptr;
}

const double *TableBook::Number(std::string_view table, std::string_view row,
                                std::string_view column) const {
  const Cell *cell = At(table, row, column, true);
  return cell == nullptr ? nullptr : &cell->Value;
}

const std::string *TableBook::Text(std::string_view table, std::string_view row,
                                   std::string_view column) const {
  const Cell *cell = At(table, row, column, false);
  return cell == nullptr ? nullptr : &cell->Spelling;
}

}

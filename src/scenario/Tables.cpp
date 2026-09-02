#include "Tables.h"

#include <charconv>
#include <expected>
#include <string>
#include <span>
#include <cstddef>
#include <vector>
#include <system_error>
#include <utility>
#include <string_view>

namespace outshine {

std::expected<TableBook, std::string> TableBook::Stand(std::span<const Scenario::Table> declared) {
  TableBook standing;
  for (const Scenario::Table &table : declared) {
    if (standing.Held_.contains(table.Id)) {
      return std::unexpected("the table '" + table.Id +
                             "' is declared twice, and a number has one home");
    }
    if (table.Rows.size() > kMostRows) {
      return std::unexpected("the table '" + table.Id + "' declares " +
                             std::to_string(table.Rows.size()) + " rows over the bound of " +
                             std::to_string(kMostRows));
    }
    Stood stood;
    stood.Columns = table.Columns;
    stood.Numeric.assign(table.Columns.size(), false);
    for (size_t at = 0; at < table.Types.size() && at < stood.Numeric.size(); ++at) {
      stood.Numeric[at] = table.Types[at];
    }
    for (const std::vector<std::string> &row : table.Rows) {
      if (row.size() != table.Columns.size()) {
        return std::unexpected("the table '" + table.Id + "' has a row of " +
                               std::to_string(row.size()) + " cells under " +
                               std::to_string(table.Columns.size()) + " columns");
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
            return std::unexpected(
                "the table '" + table.Id + "' holds '" + cell.Spelling +
                "' in the number column '" + stood.Columns[at] +
                "' -- a cell is typed by its column, and this one does not read");
          }
        }
        cells.push_back(std::move(cell));
      }
      if ((!cells.empty()) && (!stood.ByKey.emplace(cells[0].Spelling, stood.RowCount).second)) {
        return std::unexpected("the table '" + table.Id + "' keys two rows by '" +
                               cells[0].Spelling + "', and a lookup with two answers has none");
      }

      for (Cell &one : cells) { stood.Cells.push_back(std::move(one)); }
      ++stood.RowCount;
    }
    standing.Held_.emplace(table.Id, std::move(stood));
  }
  return standing;
}

const TableBook::Cell *TableBook::At(std::string_view table,
                                     std::string_view row,
                                     std::string_view column,
                                     bool wantNumber) const {
  const auto held = Held_.find(table);
  if (held == Held_.end()) { return nullptr; }
  const Stood &stood = held->second;
  const auto keyed = stood.ByKey.find(row);
  if (keyed == stood.ByKey.end()) { return nullptr; }
  for (size_t at = 0; at < stood.Columns.size(); ++at) {
    if (stood.Columns[at] != column) { continue; }
    if (stood.Numeric[at] != wantNumber) { return nullptr; }
    return &stood.Rows()[keyed->second, at];
  }
  return nullptr;
}

const double *
TableBook::Number(std::string_view table, std::string_view row, std::string_view column) const {
  const Cell *cell = At(table, row, column, true);
  return cell == nullptr ? nullptr : &cell->Value;
}

const std::string *
TableBook::Text(std::string_view table, std::string_view row, std::string_view column) const {
  const Cell *cell = At(table, row, column, false);
  return cell == nullptr ? nullptr : &cell->Spelling;
}

} // namespace outshine

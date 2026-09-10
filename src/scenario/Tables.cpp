#include "Tables.h"

#include "Number.h"
#include <algorithm>
#include <expected>
#include <string>
#include <span>
#include <cstddef>
#include <vector>
#include <utility>
#include <string_view>

namespace outshine {

namespace Says {
constexpr auto MissingTableId = "table identifier must not be empty";
constexpr auto MissingColumns = " must declare at least one column";
constexpr auto ExcessTypes = " has more type declarations than columns";
constexpr auto InvalidColumnName = " has an empty or duplicate column name";
constexpr auto ExcessRows = " exceeds the row limit";
constexpr auto ExcessCells = " exceeds cell storage capacity";
constexpr auto InvalidRowWidth = " has a row whose width does not match its columns";
constexpr auto InvalidNumber = " requires a complete finite decimal in column ";
constexpr auto DuplicateKey = " has a duplicate first-column key";
constexpr auto DuplicateTable = " is declared more than once";
}

namespace {
[[nodiscard]] std::expected<void, std::string> ValidateSchema(const Scenario::Table &table) {
  if (table.Id.empty()) { return std::unexpected(Says::MissingTableId); }
  if (table.Columns.empty()) { return std::unexpected(table.Id + Says::MissingColumns); }
  if (table.Types.size() > table.Columns.size()) {
    return std::unexpected(table.Id + Says::ExcessTypes);
  }
  if (table.Rows.size() > TableBook::kMostRows) {
    return std::unexpected(table.Id + Says::ExcessRows);
  }
  std::vector<std::string_view> columns(table.Columns.begin(), table.Columns.end());
  std::ranges::sort(columns);
  if (columns.front().empty() || std::ranges::adjacent_find(columns) != columns.end()) {
    return std::unexpected(table.Id + Says::InvalidColumnName);
  }
  return {};
}
}

std::expected<void, std::string>
TableBook::AppendRow(Stood &stood, std::span<const std::string> row, const std::string &tableId) {
  if (row.size() != stood.Columns.size()) {
    return std::unexpected(tableId + Says::InvalidRowWidth);
  }
  for (size_t at = 0; at < row.size(); ++at) {
    Cell cell{.Spelling = row[at]};
    if (stood.Numeric[at]) {
      const auto value = ParseFiniteNumber(row[at]);
      if (!value) { return std::unexpected(tableId + Says::InvalidNumber + stood.Columns[at]); }
      cell.Value = *value;
    }
    stood.Cells.push_back(std::move(cell));
  }
  if (!stood.ByKey.emplace(row.front(), stood.RowCount).second) {
    return std::unexpected(tableId + Says::DuplicateKey);
  }
  ++stood.RowCount;
  return {};
}

std::expected<TableBook::Stood, std::string> TableBook::PrepareTable(const Scenario::Table &table) {
  auto valid = ValidateSchema(table);
  if (!valid) { return std::unexpected(std::move(valid.error())); }
  Stood stood;
  if (!table.Rows.empty() && table.Columns.size() > stood.Cells.max_size() / table.Rows.size()) {
    return std::unexpected(table.Id + Says::ExcessCells);
  }
  stood.Columns = table.Columns;
  stood.Numeric.assign(table.Columns.size(), false);
  std::ranges::copy(table.Types, stood.Numeric.begin());
  stood.Cells.reserve(table.Rows.size() * table.Columns.size());
  stood.ByKey.reserve(table.Rows.size());
  for (const auto &row : table.Rows) {
    auto added = AppendRow(stood, row, table.Id);
    if (!added) { return std::unexpected(std::move(added.error())); }
  }
  return stood;
}

std::expected<TableBook, std::string> TableBook::Stand(std::span<const Scenario::Table> declared) {
  TableBook standing;
  for (const auto &table : declared) {
    if (standing.Held_.contains(table.Id)) {
      return std::unexpected(table.Id + Says::DuplicateTable);
    }
    auto stood = PrepareTable(table);
    if (!stood) { return std::unexpected(std::move(stood.error())); }
    standing.Held_.emplace(table.Id, std::move(*stood));
  }
  return standing;
}

const TableBook::Cell *TableBook::At(CellAt where, bool wantNumber) const {
  const auto held = Held_.find(where.Table);
  if (held == Held_.end()) { return nullptr; }
  const Stood &stood = held->second;
  const auto keyed = stood.ByKey.find(where.Row);
  if (keyed == stood.ByKey.end()) { return nullptr; }
  for (size_t at = 0; at < stood.Columns.size(); ++at) {
    if (stood.Columns[at] != where.Column) { continue; }
    if (stood.Numeric[at] != wantNumber) { return nullptr; }
    return &stood.Rows()[keyed->second, at];
  }
  return nullptr;
}

const double *TableBook::Number(CellAt where) const {
  const Cell *cell = At(where, true);
  return cell == nullptr ? nullptr : &cell->Value;
}

const std::string *TableBook::Text(CellAt where) const {
  const Cell *cell = At(where, false);
  return cell == nullptr ? nullptr : &cell->Spelling;
}

}

#include "SurfaceHeightProgramInternal.h"
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <limits>
#include <utility>
#include <vector>

namespace outshine::Generators::HeightAssembly {
namespace {
using Column = std::vector<std::pair<uint32_t, double>>;

bool FlightExpressions(const Input &input, std::vector<Column> &expressions, System &system) {
  for (const auto &flight : input.Flights) {
    const auto start =
        static_cast<uint32_t>(input.Points.size() + 3 * static_cast<size_t>(flight.Start));
    const auto end =
        static_cast<uint32_t>(input.Points.size() + 3 * static_cast<size_t>(flight.End));
    for (const auto vertex : input.Sources[flight.Source].Vertices) {
      if (!system.Check()) { return false; }
      const double f = std::clamp(
          (flight.Stations[vertex] - flight.CutStart) / (flight.CutEnd - flight.CutStart), 0., 1.);
      Column expression;
      if (f != 1) { expression.emplace_back(start, 1 - f); }
      if (f != 0) { expression.emplace_back(end, f); }
      std::ranges::sort(expression, {}, &Column::value_type::first);
      if (!expressions[vertex].empty() && expressions[vertex] != expression) {
        return system.Fail(Error::ConflictingOwners);
      }
      expressions[vertex] = std::move(expression);
    }
  }
  return true;
}

bool MakeProjection(const std::vector<Column> &expressions,
                    const std::vector<uint32_t> &map,
                    size_t activeCount,
                    System &system) {
  std::vector<SparseEntry<int>> entries;
  entries.reserve(expressions.size() * 2);
  for (size_t old = 0; old < expressions.size(); ++old) {
    if (expressions[old].empty()) {
      entries.push_back(
          {.Row = static_cast<int>(old), .Column = static_cast<int>(map[old]), .Value = 1});
      continue;
    }
    for (const auto [column, weight] : expressions[old]) {
      if (map[column] == std::numeric_limits<uint32_t>::max()) {
        return system.Fail(Error::ConflictingOwners);
      }
      entries.push_back(
          {.Row = static_cast<int>(old), .Column = static_cast<int>(map[column]), .Value = weight});
    }
  }
  system.Projection = SparseMatrix<int>::Assemble(
      static_cast<int>(expressions.size()), static_cast<int>(activeCount), entries);
  return true;
}

bool ProjectionFitsBudget(System &system) {
  const auto &conditions = system.Program.Conditions;
  const auto &projection = system.Projection;
  for (int column = 0; column < projection.Columns; ++column) {
    const auto index = static_cast<size_t>(column);
    for (int at = projection.ColumnOffsets[index]; at < projection.ColumnOffsets[index + 1]; ++at) {
      const auto source = static_cast<size_t>(projection.RowIndices[static_cast<size_t>(at)]);
      const auto count = static_cast<size_t>(conditions.ColumnOffsets[source + 1] -
                                             conditions.ColumnOffsets[source]);
      if (!system.Count(system.Work.ProjectionProducts, system.Budget.ProjectionProducts, count)) {
        return false;
      }
    }
  }
  return true;
}
}

bool Project(const Input &input, System &system) {
  std::vector<Column> expressions(system.Program.Variables.size());
  if (!FlightExpressions(input, expressions, system)) { return false; }
  std::vector<uint32_t> active;
  active.reserve(expressions.size());
  std::vector<uint32_t> map(expressions.size(), std::numeric_limits<uint32_t>::max());
  for (uint32_t at = 0; at < expressions.size(); ++at) {
    if (expressions[at].empty()) {
      map[at] = static_cast<uint32_t>(active.size());
      active.push_back(at);
    }
  }
  if (!MakeProjection(expressions, map, active.size(), system) || !ProjectionFitsBudget(system)) {
    return false;
  }
  auto matrix = system.Program.Conditions.Multiply(system.Projection);
  if (matrix.Values.size() > system.Budget.Nonzeros) {
    return system.Fail(Error::CapacityExceeded);
  }
  if (!std::ranges::all_of(matrix.Values, [](double value) { return std::isfinite(value); })) {
    return system.Fail(Error::InvalidInput);
  }
  std::vector<RoadProfiles::HeightVariable> bounds;
  bounds.reserve(active.size());
  for (const auto at : active) { bounds.push_back(system.Program.Variables[at]); }
  system.Program.AdjustmentVariable = map[system.Program.AdjustmentVariable];
  system.Program.Variables = std::move(bounds);
  system.Program.Conditions = std::move(matrix);
  return true;
}
}

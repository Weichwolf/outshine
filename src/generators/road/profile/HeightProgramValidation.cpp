#include "HeightProgramInternal.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <utility>

namespace outshine::Generators::RoadProfiles {
namespace {

bool ValidBounds(const HeightVariable &variable) {
  return !std::isnan(variable.Minimum) && !std::isnan(variable.Maximum) &&
         variable.Minimum <= variable.Maximum &&
         variable.Minimum < std::numeric_limits<double>::infinity() &&
         variable.Maximum > -std::numeric_limits<double>::infinity();
}

bool Canonical(const SparseMatrix<int> &matrix) {
  if (matrix.ColumnOffsets.size() != static_cast<size_t>(matrix.Columns) + 1 ||
      matrix.Values.size() != matrix.RowIndices.size() || matrix.ColumnOffsets.front() != 0 ||
      matrix.Values.size() > static_cast<size_t>(std::numeric_limits<int>::max()) ||
      std::cmp_not_equal(matrix.ColumnOffsets.back(), matrix.Values.size())) {
    return false;
  }
  for (size_t column = 0; std::cmp_less(column, matrix.Columns); ++column) {
    const int begin = matrix.ColumnOffsets[column];
    const int end = matrix.ColumnOffsets[column + 1];
    if (begin < 0 || end < begin || std::cmp_greater(end, matrix.Values.size())) { return false; }
    int previous = -1;
    for (int at = begin; at < end; ++at) {
      const auto index = static_cast<size_t>(at);
      if (matrix.RowIndices[index] <= previous || matrix.RowIndices[index] >= matrix.Rows ||
          !std::isfinite(matrix.Values[index])) {
        return false;
      }
      previous = matrix.RowIndices[index];
    }
  }
  return true;
}

}

std::expected<void, HeightFitError> Validate(const HeightProgram &program, HeightFitBudget budget) {
  const auto &matrix = program.Conditions;
  if (matrix.Rows < 0 || matrix.Columns <= 0 ||
      program.AdjustmentVariable >= program.Variables.size() ||
      program.Variables.size() != static_cast<size_t>(matrix.Columns) ||
      program.Upper.size() != static_cast<size_t>(matrix.Rows) || budget.Iterations == 0 ||
      budget.Iterations > static_cast<uint32_t>(std::numeric_limits<int>::max()) ||
      !std::isfinite(budget.Seconds) || budget.Seconds <= 0) {
    return std::unexpected(HeightFitError::InvalidInput);
  }
  if (program.Variables.size() > budget.Variables || program.Upper.size() > budget.Conditions ||
      matrix.Values.size() > budget.Nonzeros) {
    return std::unexpected(HeightFitError::CapacityExceeded);
  }
  if (!Canonical(matrix) || !std::ranges::all_of(program.Variables, ValidBounds) ||
      !std::ranges::all_of(program.Upper, [](double value) { return std::isfinite(value); })) {
    return std::unexpected(HeightFitError::InvalidInput);
  }
  const auto &error = program.Variables[program.AdjustmentVariable];
  if (error.Minimum < 0 || !std::isfinite(error.Maximum)) {
    return std::unexpected(HeightFitError::InvalidInput);
  }
  return {};
}

bool Verify(const HeightProgram &program, HeightFit &fit) {
  if (fit.Values.size() != program.Variables.size() ||
      !std::ranges::all_of(fit.Values, [](double value) { return std::isfinite(value); })) {
    return false;
  }
  const auto actual = program.Conditions.Multiply(fit.Values);
  for (size_t at = 0; at < actual.size(); ++at) {
    if (!std::isfinite(actual[at])) { return false; }
    fit.MaximumViolation = std::max(fit.MaximumViolation, actual[at] - program.Upper[at]);
  }
  for (size_t at = 0; at < fit.Values.size(); ++at) {
    fit.MaximumViolation = std::max({fit.MaximumViolation,
                                     program.Variables[at].Minimum - fit.Values[at],
                                     fit.Values[at] - program.Variables[at].Maximum});
  }
  fit.MaximumAdjustmentM = fit.Values[program.AdjustmentVariable];
  return fit.MaximumViolation <= kHeightProgramTolerance;
}

}

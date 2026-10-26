#include "HeightProgram.h"

#include <charconv>
#include <cstddef>
#include <iostream>
#include <print>
#include <span>
#include <string>

namespace {

bool Read(std::span<double> values) {
  for (double &value : values) {
    std::string token;
    if (!(std::cin >> token) || token.size() > 64) { return false; }
    const auto parsed = std::from_chars(token.data(), token.data() + token.size(), value);
    if (parsed.ec != std::errc{} || parsed.ptr != token.data() + token.size()) { return false; }
  }
  return true;
}

bool Read(std::span<int> values) {
  for (int &value : values) {
    if (!(std::cin >> value)) { return false; }
  }
  return true;
}

}

int main() {
  using namespace outshine::Generators::RoadProfiles;
  int rows = 0, columns = 0, nonzeros = 0;
  HeightProgram program;
  if (!(std::cin >> rows >> columns >> nonzeros >> program.AdjustmentVariable) || rows < 0 ||
      rows > 200000 || columns <= 0 || columns > 4096 || nonzeros < 0 || nonzeros > 1000000) {
    return 2;
  }
  program.Conditions.Rows = rows;
  program.Conditions.Columns = columns;
  program.Conditions.ColumnOffsets.resize(static_cast<size_t>(columns) + 1);
  program.Conditions.RowIndices.resize(static_cast<size_t>(nonzeros));
  program.Conditions.Values.resize(static_cast<size_t>(nonzeros));
  program.Upper.resize(static_cast<size_t>(rows));
  program.Variables.resize(static_cast<size_t>(columns));
  if (!Read(program.Conditions.ColumnOffsets) || !Read(program.Conditions.RowIndices) ||
      !Read(program.Conditions.Values) || !Read(program.Upper)) {
    return 2;
  }
  for (auto &variable : program.Variables) {
    if (!Read(std::span(&variable.Minimum, 1)) || !Read(std::span(&variable.Maximum, 1))) {
      return 2;
    }
  }
  const auto fit = FitHeights(program, {}, {});
  if (!fit) {
    std::println("error {}", static_cast<unsigned>(fit.error()));
    return 1;
  }
  std::println("fit {:.17g} {:.17g} {:.17g} {:.17g} {} {}",
               fit->MaximumAdjustmentM,
               fit->MaximumViolation,
               fit->SetupMs,
               fit->SolveMs,
               fit->Iterations,
               fit->ModelWarning);
  for (const double value : fit->Values) { std::println("{:.17g}", value); }
}

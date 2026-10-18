#ifndef OUTSHINE_EXPERIMENTS_WATER_NATIVEPROFILEOUTPUT_H
#define OUTSHINE_EXPERIMENTS_WATER_NATIVEPROFILEOUTPUT_H
#include <charconv>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <ostream>
#include <span>
#include <string>
#include <string_view>
#include <system_error>

namespace ProfileProbe {
using Clock = std::chrono::steady_clock;

inline double Milliseconds(Clock::time_point began) {
  return std::chrono::duration<double, std::milli>(Clock::now() - began).count();
}

inline bool Number(std::string_view text, auto &value) {
  const auto parsed = std::from_chars(text.data(), text.data() + text.size(), value);
  return parsed.ec == std::errc{} && parsed.ptr == text.data() + text.size();
}

inline void Array(std::ostream &out, std::span<const double> values) {
  out << '[';
  for (size_t at = 0; at < values.size(); ++at) {
    if (at != 0) { out << ','; }
    out << values[at];
  }
  out << ']';
}

struct Result {
  std::string Solver, Version, Status;
  bool Optimal = false;
  double SetupMs = 0, SolverMs = 0;
  int64_t Columns = 0, Rows = 0, Iterations = 0;
  int64_t WorkingRows = 0, WorkingNonzeros = 0, HessianNonzeros = 0;
  std::span<const double> Values, ColumnDual, RowDual;
};

inline bool Write(const char *path, const Result &result) {
  std::ofstream out(path);
  out << std::setprecision(std::numeric_limits<double>::max_digits10)
      << "{\"optimal\":" << (result.Optimal ? "true" : "false")
      << ",\"solver\":" << std::quoted(result.Solver)
      << ",\"status\":" << std::quoted(result.Status)
      << ",\"version\":" << std::quoted(result.Version) << ",\"setup_ms\":" << result.SetupMs
      << ",\"solver_ms\":" << result.SolverMs << ",\"columns\":" << result.Columns
      << ",\"rows\":" << result.Rows << ",\"iterations\":" << result.Iterations
      << ",\"working_rows\":" << result.WorkingRows
      << ",\"working_nonzeros\":" << result.WorkingNonzeros
      << ",\"hessian_nonzeros\":" << result.HessianNonzeros << ",\"values\":";
  Array(out, result.Values);
  out << ",\"column_dual\":";
  Array(out, result.ColumnDual);
  out << ",\"row_dual\":";
  Array(out, result.RowDual);
  out << "}\n";
  return out.good();
}
}
#endif

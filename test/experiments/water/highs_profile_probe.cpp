#include "native_profile_output.h"
#include <Highs.h>
#include <cmath>
#include <cstdint>
#include <string>

using namespace ProfileProbe;

namespace {
constexpr double kTolerance = 1e-8;

bool Settings(Highs &solver, double seconds, HighsInt iterations) {
  return solver.setOptionValue("output_flag", false) == HighsStatus::kOk &&
         solver.setOptionValue("threads", HighsInt{1}) == HighsStatus::kOk &&
         solver.setOptionValue("time_limit", seconds) == HighsStatus::kOk &&
         solver.setOptionValue("simplex_iteration_limit", iterations) == HighsStatus::kOk &&
         solver.setOptionValue("qp_iteration_limit", iterations) == HighsStatus::kOk &&
         solver.setOptionValue("qp_regularization_value", 0.0) == HighsStatus::kOk &&
         solver.setOptionValue("primal_feasibility_tolerance", kTolerance) == HighsStatus::kOk &&
         solver.setOptionValue("dual_feasibility_tolerance", kTolerance) == HighsStatus::kOk;
}
}

int main(int argc, char *const *argv) {
  double seconds = 0;
  HighsInt iterations = 0;
  if (argc != 5 || !Number(argv[3], seconds) || !Number(argv[4], iterations) ||
      !std::isfinite(seconds) || seconds < 0 || iterations < 0) {
    return 2;
  }
  const auto began = Clock::now();
  Highs solver;
  if (!Settings(solver, seconds, iterations) || solver.readModel(argv[1]) != HighsStatus::kOk) {
    return 3;
  }
  if (!solver.getModel().isQp() &&
      solver.setOptionValue("solver", std::string("simplex")) != HighsStatus::kOk) {
    return 3;
  }
  const double setupMs = Milliseconds(began);
  const auto solveAt = Clock::now();
  const auto status = seconds > 0 && iterations > 0 ? solver.run() : HighsStatus::kWarning;
  const double solveMs = Milliseconds(solveAt);
  const auto &solution = solver.getSolution();
  const auto &info = solver.getInfo();
  const auto &model = solver.getModel();
  const bool optimal = status == HighsStatus::kOk &&
                       solver.getModelStatus() == HighsModelStatus::kOptimal &&
                       solution.value_valid && solution.dual_valid;
  Result result;
  result.Solver = "HiGHS";
  result.Version = solver.version();
  result.Status = solver.modelStatusToString(solver.getModelStatus());
  result.Optimal = optimal;
  result.SetupMs = setupMs;
  result.SolverMs = solveMs;
  result.Columns = model.lp_.num_col_;
  result.Rows = result.WorkingRows = model.lp_.num_row_;
  result.WorkingNonzeros = static_cast<int64_t>(model.lp_.a_matrix_.value_.size());
  result.HessianNonzeros = static_cast<int64_t>(model.hessian_.value_.size());
  if (info.valid) {
    result.Iterations = model.isQp() ? info.qp_iteration_count : info.simplex_iteration_count;
  }
  if (solution.value_valid) { result.Values = solution.col_value; }
  if (solution.dual_valid) {
    result.ColumnDual = solution.col_dual;
    result.RowDual = solution.row_dual;
  }
  return Write(argv[2], result) ? 0 : 4;
}

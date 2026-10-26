#include "HeightProgramInternal.h"

#include <interfaces/highs_c_api.h>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <memory>
#include <ratio>
#include <stop_token>
#include <vector>

namespace outshine::Generators::RoadProfiles {
namespace {

using Clock = std::chrono::steady_clock;

double Milliseconds(Clock::time_point began) {
  return std::chrono::duration<double, std::milli>(Clock::now() - began).count();
}

void Interrupt([[maybe_unused]] int kind,
               [[maybe_unused]] const char *message,
               [[maybe_unused]] const HighsCallbackDataOut *output,
               HighsCallbackDataIn *input,
               void *context) {
  const auto *stop = static_cast<std::stop_token *>(context);
  if (input != nullptr && stop->stop_requested()) { input->user_interrupt = 1; }
}

bool Configure(void *solver, HeightFitBudget budget, std::stop_token &stop) {
  return Highs_setBoolOptionValue(solver, "output_flag", 0) == kHighsStatusOk &&
         Highs_setIntOptionValue(solver, "threads", kHeightSolverThreads) == kHighsStatusOk &&
         Highs_setIntOptionValue(solver, "simplex_strategy", kHeightSolverSimplexStrategy) ==
             kHighsStatusOk &&
         Highs_setIntOptionValue(solver, "random_seed", kHeightSolverRandomSeed) ==
             kHighsStatusOk &&
         Highs_setIntOptionValue(solver,
                                 "simplex_iteration_limit",
                                 static_cast<HighsInt>(budget.Iterations)) == kHighsStatusOk &&
         Highs_setStringOptionValue(solver, "parallel", kHeightSolverParallel) == kHighsStatusOk &&
         Highs_setStringOptionValue(solver, "solver", kHeightSolverName) == kHighsStatusOk &&
         Highs_setStringOptionValue(solver, "presolve", kHeightSolverPresolve) == kHighsStatusOk &&
         Highs_setDoubleOptionValue(
             solver, "primal_feasibility_tolerance", kHeightProgramTolerance) == kHighsStatusOk &&
         Highs_setDoubleOptionValue(solver, "time_limit", budget.Seconds) == kHighsStatusOk &&
         Highs_setCallback(solver, Interrupt, &stop) == kHighsStatusOk &&
         Highs_startCallback(solver, kHighsCallbackSimplexInterrupt) == kHighsStatusOk;
}

struct Model {
  std::vector<HighsInt> ColumnOffsets, RowIndices;
  std::vector<double> Cost, Lower, Upper, RowLower;

  explicit Model(const HeightProgram &program)
      : ColumnOffsets(program.Conditions.ColumnOffsets.begin(),
                      program.Conditions.ColumnOffsets.end()),
        RowIndices(program.Conditions.RowIndices.begin(), program.Conditions.RowIndices.end()),
        Cost(program.Variables.size(), 0),
        Lower(program.Variables.size()),
        Upper(program.Variables.size()),
        RowLower(program.Upper.size(), -std::numeric_limits<double>::infinity()) {
    Cost[program.AdjustmentVariable] = 1;
    for (size_t at = 0; at < program.Variables.size(); ++at) {
      Lower[at] = program.Variables[at].Minimum;
      Upper[at] = program.Variables[at].Maximum;
    }
  }
};

std::expected<void, HeightFitError> Solved(const void *solver, HighsInt status) {
  switch (Highs_getModelStatus(solver)) {
    case kHighsModelStatusOptimal:
      if (status == kHighsStatusOk || status == kHighsStatusWarning) { return {}; }
      break;
    case kHighsModelStatusInfeasible: return std::unexpected(HeightFitError::Infeasible);
    case kHighsModelStatusUnbounded: return std::unexpected(HeightFitError::Unbounded);
    case kHighsModelStatusTimeLimit: return std::unexpected(HeightFitError::Deadline);
    case kHighsModelStatusIterationLimit: return std::unexpected(HeightFitError::CapacityExceeded);
    default: break;
  }
  return std::unexpected(HeightFitError::SolverFailure);
}

}

std::expected<HeightFit, HeightFitError>
FitHeights(const HeightProgram &program, HeightFitBudget budget, const std::stop_token &stop) {
  if (stop.stop_requested()) { return std::unexpected(HeightFitError::Canceled); }
  if (const auto valid = Validate(program, budget); !valid) {
    return std::unexpected(valid.error());
  }
  const auto setupAt = Clock::now();
  std::stop_token cancellation = stop;
  const std::unique_ptr<void, decltype(&Highs_destroy)> solver(Highs_create(), Highs_destroy);
  if (!solver || !Configure(solver.get(), budget, cancellation)) {
    return std::unexpected(HeightFitError::SolverFailure);
  }
  const Model model(program);
  const auto &matrix = program.Conditions;
  const auto passed = Highs_passLp(solver.get(),
                                   matrix.Columns,
                                   matrix.Rows,
                                   static_cast<HighsInt>(matrix.Values.size()),
                                   kHighsMatrixFormatColwise,
                                   kHighsObjSenseMinimize,
                                   0,
                                   model.Cost.data(),
                                   model.Lower.data(),
                                   model.Upper.data(),
                                   model.RowLower.data(),
                                   program.Upper.data(),
                                   model.ColumnOffsets.data(),
                                   model.RowIndices.data(),
                                   matrix.Values.data());
  if (passed != kHighsStatusOk && passed != kHighsStatusWarning) {
    return std::unexpected(HeightFitError::SolverFailure);
  }
  HeightFit fit;
  fit.SetupMs = Milliseconds(setupAt);
  fit.ModelWarning = passed == kHighsStatusWarning;
  if (stop.stop_requested()) { return std::unexpected(HeightFitError::Canceled); }
  const auto solveAt = Clock::now();
  const auto result = Solved(solver.get(), Highs_run(solver.get()));
  fit.SolveMs = Milliseconds(solveAt);
  if (stop.stop_requested()) { return std::unexpected(HeightFitError::Canceled); }
  if (!result) { return std::unexpected(result.error()); }
  fit.Values.resize(program.Variables.size());
  HighsInt iterations = 0;
  if (Highs_getSolution(solver.get(), fit.Values.data(), nullptr, nullptr, nullptr) !=
          kHighsStatusOk ||
      Highs_getIntInfoValue(solver.get(), "simplex_iteration_count", &iterations) !=
          kHighsStatusOk ||
      iterations < 0) {
    return std::unexpected(HeightFitError::SolverFailure);
  }
  fit.Iterations = static_cast<uint64_t>(iterations);
  if (!Verify(program, fit)) { return std::unexpected(HeightFitError::VerificationFailed); }
  return fit;
}

}

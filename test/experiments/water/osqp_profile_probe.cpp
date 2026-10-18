#include "native_profile_output.h"
#include <Highs.h>
#include <osqp.h>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

using namespace ProfileProbe;

namespace {
constexpr double kTolerance = 1e-8;
constexpr HighsInt kMaxColumns = 8193, kMaxRows = 131072;
constexpr size_t kMaxNonzeros = 1048576;

struct Shape {
  HighsInt Columns, Rows;
};

struct Sparse {
  OSQPInt Columns = 0, Rows = 0;
  std::vector<OSQPInt> Start, Index;
  std::vector<OSQPFloat> Value;
};

bool ValidEntries(const auto &matrix, Shape shape) {
  if (matrix.start_.size() != static_cast<size_t>(shape.Columns) + 1 ||
      matrix.value_.size() != matrix.index_.size() || matrix.value_.size() > kMaxNonzeros ||
      matrix.start_.front() != 0 ||
      matrix.start_.back() != static_cast<HighsInt>(matrix.value_.size()) ||
      !std::is_sorted(matrix.start_.begin(), matrix.start_.end())) {
    return false;
  }
  for (size_t at = 0; at < matrix.index_.size(); ++at) {
    if (matrix.index_[at] < 0 || matrix.index_[at] >= shape.Rows ||
        !std::isfinite(matrix.value_[at])) {
      return false;
    }
  }
  return true;
}

OSQPCscMatrix View(Sparse &matrix) {
  return {.m = matrix.Rows,
          .n = matrix.Columns,
          .p = matrix.Start.data(),
          .i = matrix.Index.data(),
          .x = matrix.Value.data(),
          .nzmax = static_cast<OSQPInt>(matrix.Value.size()),
          .nz = -1,
          .owned = 0};
}

Sparse Hessian(const HighsModel &model) {
  Sparse result;
  result.Rows = result.Columns = static_cast<OSQPInt>(model.lp_.num_col_);
  const auto &h = model.hessian_;
  if (h.dim_ == 0) {
    result.Start.resize(static_cast<size_t>(result.Columns) + 1);
    return result;
  }
  HighsSparseMatrix lower;
  lower.format_ = MatrixFormat::kColwise;
  lower.num_row_ = lower.num_col_ = h.dim_;
  lower.start_ = h.start_;
  lower.index_ = h.index_;
  lower.value_ = h.value_;
  lower.ensureRowwise();
  result.Start.assign(lower.start_.begin(), lower.start_.end());
  result.Index.assign(lower.index_.begin(), lower.index_.end());
  result.Value.assign(lower.value_.begin(), lower.value_.end());
  return result;
}

Sparse Constraints(const HighsLp &lp) {
  Sparse result;
  result.Rows = static_cast<OSQPInt>(lp.num_row_) + static_cast<OSQPInt>(lp.num_col_);
  result.Columns = static_cast<OSQPInt>(lp.num_col_);
  const auto &a = lp.a_matrix_;
  result.Start.reserve(static_cast<size_t>(lp.num_col_) + 1);
  result.Index.reserve(a.index_.size() + static_cast<size_t>(lp.num_col_));
  result.Value.reserve(a.value_.size() + static_cast<size_t>(lp.num_col_));
  for (HighsInt col = 0; col < lp.num_col_; ++col) {
    result.Start.push_back(static_cast<OSQPInt>(result.Value.size()));
    for (HighsInt at = a.start_[col]; at < a.start_[col + 1]; ++at) {
      result.Index.push_back(static_cast<OSQPInt>(a.index_[at]));
      result.Value.push_back(a.value_[at]);
    }
    result.Index.push_back(static_cast<OSQPInt>(lp.num_row_ + col));
    result.Value.push_back(1.0);
  }
  result.Start.push_back(static_cast<OSQPInt>(result.Value.size()));
  return result;
}

std::vector<OSQPFloat> Bounds(const std::vector<double> &rows, const std::vector<double> &columns) {
  std::vector<OSQPFloat> values(rows.begin(), rows.end());
  values.insert(values.end(), columns.begin(), columns.end());
  for (auto &value : values) { value = std::clamp(value, -OSQP_INFTY, OSQP_INFTY); }
  return values;
}
}

int main(int argc, char *const *argv) {
  double seconds = 0;
  OSQPInt iterations = 0;
  if (argc != 5 || !Number(argv[3], seconds) || !Number(argv[4], iterations) ||
      !std::isfinite(seconds) || seconds < 0 || iterations < 0) {
    return 2;
  }
  const auto began = Clock::now();
  Highs reader;
  if (reader.setOptionValue("output_flag", false) != HighsStatus::kOk ||
      reader.readModel(argv[1]) != HighsStatus::kOk) {
    return 3;
  }
  const auto &model = reader.getModel();
  const auto &lp = model.lp_;
  if (lp.num_col_ <= 0 || lp.num_col_ > kMaxColumns || lp.num_row_ < 0 || lp.num_row_ > kMaxRows ||
      lp.col_cost_.size() != static_cast<size_t>(lp.num_col_) ||
      lp.col_lower_.size() != lp.col_cost_.size() || lp.col_upper_.size() != lp.col_cost_.size() ||
      lp.row_lower_.size() != static_cast<size_t>(lp.num_row_) ||
      lp.row_upper_.size() != lp.row_lower_.size() ||
      lp.a_matrix_.format_ != MatrixFormat::kColwise ||
      !ValidEntries(lp.a_matrix_, {.Columns = lp.num_col_, .Rows = lp.num_row_}) ||
      (model.isQp() &&
       (model.hessian_.dim_ != lp.num_col_ ||
        !ValidEntries(model.hessian_, {.Columns = lp.num_col_, .Rows = lp.num_col_}))) ||
      (model.isQp() && model.hessian_.format_ != HessianFormat::kTriangular)) {
    return 3;
  }
  Result result;
  result.Solver = "OSQP";
  result.Version = osqp_version();
  result.Status = "budget exhausted before solve";
  result.Columns = lp.num_col_;
  result.Rows = lp.num_row_;
  result.SetupMs = Milliseconds(began);
  if (seconds == 0 || iterations == 0) { return Write(argv[2], result) ? 0 : 4; }
  auto hessian = Hessian(model);
  auto constraints = Constraints(lp);
  auto lower = Bounds(lp.row_lower_, lp.col_lower_);
  auto upper = Bounds(lp.row_upper_, lp.col_upper_);
  const auto p = View(hessian);
  const auto a = View(constraints);
  OSQPSettings settings{};
  osqp_set_default_settings(&settings);
  settings.verbose = 0;
  settings.polishing = 1;
  settings.eps_abs = settings.eps_rel = kTolerance;
  settings.max_iter = iterations;
  settings.time_limit = seconds;
  OSQPSolver *solver = nullptr;
  const auto setup = osqp_setup(
      &solver, &p, lp.col_cost_.data(), &a, lower.data(), upper.data(), a.m, a.n, &settings);
  const std::unique_ptr<OSQPSolver, decltype(&osqp_cleanup)> lifetime(solver, osqp_cleanup);
  if (setup != 0 || solver == nullptr) { return 3; }
  result.SetupMs = Milliseconds(began);
  const auto solveAt = Clock::now();
  const auto status = osqp_solve(solver);
  result.SolverMs = Milliseconds(solveAt);
  result.Status = solver->info->status;
  result.Optimal = status == 0 && solver->info->status_val == OSQP_SOLVED;
  result.Iterations = solver->info->iter;
  result.WorkingRows = a.m;
  result.WorkingNonzeros = a.nzmax;
  result.HessianNonzeros = p.nzmax;
  std::vector<double> columnDual;
  std::vector<double> rowDual;
  if (result.Optimal) {
    result.Values = {solver->solution->x, static_cast<size_t>(lp.num_col_)};
    rowDual.resize(static_cast<size_t>(lp.num_row_));
    columnDual.resize(static_cast<size_t>(lp.num_col_));
    for (size_t row = 0; row < rowDual.size(); ++row) { rowDual[row] = -solver->solution->y[row]; }
    for (size_t col = 0; col < columnDual.size(); ++col) {
      columnDual[col] = -solver->solution->y[rowDual.size() + col];
    }
    result.ColumnDual = columnDual;
    result.RowDual = rowDual;
  }
  return Write(argv[2], result) ? 0 : 4;
}

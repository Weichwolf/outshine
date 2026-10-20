#include "WaterHeightProgram.h"
#include <algorithm>
#include <array>
#include <cassert>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <memory>
#include <ratio>
#include <span>
#include <stop_token>
#include <utility>
#include <vector>

namespace outshine::Generators {
namespace {
using Clock = std::chrono::steady_clock;
using Error = WaterHeightError;
constexpr double kSolverTolerance = 1e-9;
constexpr double kCertificateTolerance = 1e-7;
constexpr double kPhysicalTolerance = 1e-9;
constexpr OSQPInt kRhoInterval = 50;

double Milliseconds(Clock::time_point began) {
  return std::chrono::duration<double, std::milli>(Clock::now() - began).count();
}

OSQPCscMatrix View(WaterSparseMatrix &matrix) {
  assert(matrix.isCompressed());
  return {.m = static_cast<OSQPInt>(matrix.rows()),
          .n = static_cast<OSQPInt>(matrix.cols()),
          .p = matrix.outerIndexPtr(),
          .i = matrix.innerIndexPtr(),
          .x = matrix.valuePtr(),
          .nzmax = static_cast<OSQPInt>(matrix.nonZeros()),
          .nz = -1,
          .owned = 0};
}

size_t Bytes(const WaterSparseMatrix &matrix) {
  return static_cast<size_t>(matrix.data().allocatedSize()) * (sizeof(double) + sizeof(OSQPInt)) +
         static_cast<size_t>(matrix.cols() + 1) * sizeof(OSQPInt);
}

bool Certificate(const WaterHeightProgram &program,
                 const OSQPSolution &solution,
                 WaterHeightFit &fit) {
  const Eigen::Map<const Eigen::VectorXd> x(solution.x, program.Mass.rows());
  const Eigen::Map<const Eigen::VectorXd> dual(solution.y, program.Constraints.rows());
  if (!x.allFinite() || !dual.allFinite()) { return false; }
  const Eigen::VectorXd slack = program.Upper - program.Constraints * x;
  for (Eigen::Index row = 0; row < slack.size(); ++row) {
    const bool inequality = std::cmp_less(row, program.Inequalities);
    fit.PrimalViolation =
        std::max(fit.PrimalViolation, inequality ? -slack[row] : std::abs(slack[row]));
    if (inequality) { fit.DualViolation = std::max(fit.DualViolation, -dual[row]); }
  }
  const Eigen::VectorXd product = program.Mass * x;
  const Eigen::VectorXd stationarity =
      product + program.Linear + program.Constraints.transpose() * dual;
  fit.StationarityResidual = stationarity.cwiseAbs().maxCoeff();
  fit.DualGap = std::abs(x.dot(product) + program.Linear.dot(x) + program.Upper.dot(dual));
  fit.ComplementarityResidual = std::abs(slack.dot(dual));
  const std::array residuals{fit.PrimalViolation,
                             fit.DualViolation,
                             fit.StationarityResidual,
                             fit.DualGap,
                             fit.ComplementarityResidual};
  return std::ranges::all_of(residuals, [](double value) {
    return std::isfinite(value) && value <= kCertificateTolerance;
  });
}

std::expected<std::vector<Vec3>, Error> FacetNormals(const WaterHeightProblem &problem,
                                                     const WaterHeightProgram &program,
                                                     WaterHeightFit &fit) {
  std::vector<Vec3> normals;
  normals.reserve(problem.Faces.size());
  for (size_t face = 0; face < problem.Faces.size(); ++face) {
    const auto index = problem.Faces[face].Vertices;
    const auto &origin = fit.VerticesEnuM[index[0]];
    const auto normal =
        Cross(fit.VerticesEnuM[index[1]] - origin, fit.VerticesEnuM[index[2]] - origin);
    if (!std::isfinite(normal[2]) || normal[2] == 0) {
      return std::unexpected(Error::VerificationFailed);
    }
    normals.push_back(normal * (1 / normal[2]));
    const double excess = Length(normals.back()) - program.Facets[face].NormalBound;
    if (!std::isfinite(excess)) { return std::unexpected(Error::VerificationFailed); }
    fit.NormalBoundExcess = std::max(fit.NormalBoundExcess, excess);
  }
  return normals;
}

bool WholeFaces(const WaterHeightProblem &problem,
                const WaterHeightProgram &program,
                std::span<const Vec3> normals,
                WaterHeightFit &fit) {
  for (size_t at = 0; at < problem.Grades.size(); ++at) {
    const auto &grade = problem.Grades[at];
    const auto &cap = program.Facets[grade.Face].Cap;
    const auto normal = normals[grade.Face];
    const double size = Length(normal);
    const double vertical = Dot(normal, cap.Up);
    const double lowDenominator =
        (vertical - size * cap.Delta) * std::sqrt(1 - cap.Delta * cap.Delta);
    const double highDenominator = vertical + size * cap.Delta;
    if (!std::isfinite(lowDenominator) || lowDenominator <= 0 || !std::isfinite(highDenominator)) {
      return false;
    }
    const double center = -Dot(normal, program.Tangents[at]);
    const double low = center - size * cap.Delta;
    const double high = center + size * cap.Delta;
    const double minimum = std::min(low / lowDenominator, low / highDenominator);
    const double maximum = std::max(high / lowDenominator, high / highDenominator);
    if (!std::isfinite(minimum) || !std::isfinite(maximum)) { return false; }
    fit.WholeFaceViolation =
        std::max({fit.WholeFaceViolation, grade.Lower - minimum, maximum - grade.Upper});
  }
  return fit.WholeFaceViolation <= kPhysicalTolerance &&
         fit.NormalBoundExcess <= kPhysicalTolerance;
}

bool ContactsAndBounds(const WaterHeightProblem &problem, WaterHeightFit &fit) {
  for (const auto &point : fit.VerticesEnuM) {
    fit.HeightBoxViolationM = std::max({fit.HeightBoxViolationM,
                                        problem.UpBoundsM[0] - point[2],
                                        point[2] - problem.UpBoundsM[1]});
  }
  for (const auto &contact : problem.Contacts) {
    double height = 0;
    for (size_t at = 0; at < contact.Barycentric.size(); ++at) {
      height +=
          contact.Barycentric[at] * fit.VerticesEnuM[problem.Faces[contact.Face].Vertices[at]][2];
    }
    fit.MaximumContactErrorM = std::max(fit.MaximumContactErrorM, std::abs(height - contact.UpM));
  }
  return fit.HeightBoxViolationM <= kPhysicalTolerance &&
         fit.MaximumContactErrorM <= kPhysicalTolerance;
}

bool Geometry(const WaterHeightProblem &problem,
              const WaterHeightProgram &program,
              const OSQPSolution &solution,
              WaterHeightFit &fit) {
  Eigen::VectorXd error(static_cast<Eigen::Index>(problem.VerticesXY.size()));
  fit.VerticesEnuM.reserve(problem.VerticesXY.size());
  for (size_t at = 0; at < problem.VerticesXY.size(); ++at) {
    const double height = solution.x[at] + program.DatumM;
    if (!std::isfinite(height)) { return false; }
    fit.VerticesEnuM.push_back({{problem.VerticesXY[at][0], problem.VerticesXY[at][1], height}});
    error[static_cast<Eigen::Index>(at)] = height - problem.TargetUpM[at];
    fit.MaximumTargetErrorM =
        std::max(fit.MaximumTargetErrorM, std::abs(height - problem.TargetUpM[at]));
  }
  const double squared = error.dot(program.Mass * error);
  if (!std::isfinite(squared) || squared < 0) { return false; }
  fit.IntegratedRmsM = std::sqrt(squared);
  const auto normals = FacetNormals(problem, program, fit);
  return normals && WholeFaces(problem, program, *normals, fit) && ContactsAndBounds(problem, fit);
}

std::expected<void, Error> Solved(const OSQPSolver &solver, OSQPInt status) {
  if (status != 0 || solver.info == nullptr || solver.solution == nullptr ||
      solver.solution->x == nullptr || solver.solution->y == nullptr) {
    return std::unexpected(Error::SolverFailure);
  }
  switch (solver.info->status_val) {
    case OSQP_SOLVED:
    case OSQP_SOLVED_INACCURATE: return {};
    case OSQP_PRIMAL_INFEASIBLE:
    case OSQP_PRIMAL_INFEASIBLE_INACCURATE: return std::unexpected(Error::Infeasible);
    case OSQP_MAX_ITER_REACHED:
    case OSQP_TIME_LIMIT_REACHED: return std::unexpected(Error::CapacityExceeded);
    default: return std::unexpected(Error::SolverFailure);
  }
}
}

std::expected<WaterHeightFit, WaterHeightError> FitWaterSurfaceHeights(
    const WaterHeightProblem &problem, WaterHeightBudget budget, const std::stop_token &stop) {
  const auto began = Clock::now();
  auto program = BuildWaterHeightProgram(problem, budget, stop);
  if (!program) { return std::unexpected(program.error()); }
  WaterHeightFit fit;
  fit.AssemblyMs = Milliseconds(began);
  fit.Rows = static_cast<size_t>(program->Constraints.rows());
  fit.Nonzeros = static_cast<size_t>(program->Constraints.nonZeros());
  fit.MatrixBytes =
      Bytes(program->Mass) + Bytes(program->Hessian) + Bytes(program->Constraints) +
      static_cast<size_t>(program->Linear.size() + program->Lower.size() + program->Upper.size()) *
          sizeof(double);
  const auto hessian = View(program->Hessian);
  const auto constraints = View(program->Constraints);
  OSQPSettings settings{};
  osqp_set_default_settings(&settings);
  settings.verbose = 0;
  settings.polishing = 1;
  settings.eps_abs = settings.eps_rel = kSolverTolerance;
  settings.adaptive_rho_interval = kRhoInterval;
  settings.max_iter = static_cast<OSQPInt>(budget.Iterations);
  settings.time_limit = budget.SolveSeconds;
  OSQPSolver *solver = nullptr;
  const auto setupAt = Clock::now();
  const auto status = osqp_setup(&solver,
                                 &hessian,
                                 program->Linear.data(),
                                 &constraints,
                                 program->Lower.data(),
                                 program->Upper.data(),
                                 constraints.m,
                                 constraints.n,
                                 &settings);
  const std::unique_ptr<OSQPSolver, decltype(&osqp_cleanup)> lifetime(solver, osqp_cleanup);
  fit.SetupMs = Milliseconds(setupAt);
  if (status != 0 || solver == nullptr) { return std::unexpected(Error::SolverFailure); }
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  const auto solveAt = Clock::now();
  const auto solved = Solved(*solver, osqp_solve(solver));
  fit.SolveMs = Milliseconds(solveAt);
  if (stop.stop_requested()) { return std::unexpected(Error::Canceled); }
  if (!solved) { return std::unexpected(solved.error()); }
  fit.Iterations = static_cast<uint64_t>(solver->info->iter);
  if (!Certificate(*program, *solver->solution, fit) ||
      !Geometry(problem, *program, *solver->solution, fit)) {
    return std::unexpected(Error::VerificationFailed);
  }
  return fit;
}
}

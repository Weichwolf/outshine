#ifndef OUTSHINE_GENERATORS_ROAD_PROFILE_HEIGHTPROGRAM_H
#define OUTSHINE_GENERATORS_ROAD_PROFILE_HEIGHTPROGRAM_H

#include <cstddef>
#include <cstdint>
#include <expected>
#include <limits>
#include <stop_token>
#include <vector>

#include "math/SparseMatrix.h"

namespace outshine::Generators::RoadProfiles {

struct HeightVariable {
  double Minimum = -std::numeric_limits<double>::infinity();
  double Maximum = std::numeric_limits<double>::infinity();
};

struct HeightProgram {
  SparseMatrix<int> Conditions;
  std::vector<double> Upper;
  std::vector<HeightVariable> Variables;
  uint32_t AdjustmentVariable = 0;
};

struct HeightFitBudget {
  static constexpr size_t kDefaultConditions = 200000;
  static constexpr size_t kDefaultNonzeros = 1000000;
  static constexpr uint32_t kDefaultIterations = 100000;
  static constexpr double kDefaultSeconds = 5.0;

  size_t Variables = 4096;
  size_t Conditions = kDefaultConditions;
  size_t Nonzeros = kDefaultNonzeros;
  uint32_t Iterations = kDefaultIterations;
  double Seconds = kDefaultSeconds;
};

enum class HeightFitError : uint8_t {
  InvalidInput,
  CapacityExceeded,
  Canceled,
  Deadline,
  Infeasible,
  Unbounded,
  SolverFailure,
  VerificationFailed
};

struct HeightFit {
  std::vector<double> Values;
  double MaximumAdjustmentM = 0.0;
  double MaximumViolation = 0.0;
  double SetupMs = 0.0;
  double SolveMs = 0.0;
  uint64_t Iterations = 0;
  bool ModelWarning = false;
};

[[nodiscard]] std::expected<HeightFit, HeightFitError>
FitHeights(const HeightProgram &program, HeightFitBudget budget, const std::stop_token &stop);

}

#endif

#ifndef OUTSHINE_GENERATORS_WATER_WATERHEIGHTFIT_H
#define OUTSHINE_GENERATORS_WATER_WATERHEIGHTFIT_H

#include "TangentFrame.h"
#include "WaterSurfaceMesh.h"
#include "Vec3.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <expected>
#include <span>
#include <stop_token>
#include <vector>

namespace outshine::Generators {
struct WaterGradeConstraint {
  uint32_t Face = 0;
  Vec3 DirectionEnu;
  double Lower = 0;
  double Upper = 0;
};

struct WaterHeightContact {
  uint32_t Face = 0;
  std::array<double, 3> Barycentric{};
  double UpM = 0;
};

struct WaterHeightProblem {
  const TangentFrame &Frame;
  std::span<const std::array<double, 2>> VerticesXY;
  std::span<const WaterSurfaceMesh::Face> Faces;
  std::span<const double> TargetUpM;
  std::span<const WaterGradeConstraint> Grades;
  std::span<const std::array<uint32_t, 2>> PrimaryGrades;
  std::array<double, 2> UpBoundsM{};
  std::span<const WaterHeightContact> Contacts;
};

struct WaterHeightBudget {
  static constexpr uint32_t DefaultIterations = 50000;
  uint32_t Vertices = 8192;
  uint32_t Faces = 16384;
  uint32_t Rows = 65536;
  uint32_t Contacts = 4096;
  uint32_t Iterations = DefaultIterations;
  double SolveSeconds = 5;
};

enum class WaterHeightError {
  InvalidInput,
  OutsidePhysicalDomain,
  CapacityExceeded,
  Canceled,
  Infeasible,
  SolverFailure,
  VerificationFailed
};

struct WaterHeightFit {
  std::vector<Vec3> VerticesEnuM;
  uint64_t Iterations = 0;
  size_t Rows = 0;
  size_t Nonzeros = 0;
  size_t MatrixBytes = 0;
  double AssemblyMs = 0;
  double SetupMs = 0;
  double SolveMs = 0;
  double IntegratedRmsM = 0;
  double MaximumTargetErrorM = 0;
  double PrimalViolation = 0;
  double DualViolation = 0;
  double StationarityResidual = 0;
  double DualGap = 0;
  double ComplementarityResidual = 0;
  double WholeFaceViolation = 0;
  double NormalBoundExcess = 0;
  double HeightBoxViolationM = 0;
  double MaximumContactErrorM = 0;
};

[[nodiscard]] std::expected<WaterHeightFit, WaterHeightError>
FitWaterSurfaceHeights(const WaterHeightProblem &problem,
                       WaterHeightBudget budget = {},
                       const std::stop_token &stop = {});
}
#endif

#ifndef OUTSHINE_GENERATORS_WATER_WATERHEIGHTPROGRAM_H
#define OUTSHINE_GENERATORS_WATER_WATERHEIGHTPROGRAM_H

#include "WaterHeightFit.h"
#include "WaterPhysicalFrame.h"
#include "SparseMatrix.h"
#include <osqp.h>
#include <array>
#include <vector>

namespace outshine::Generators {
using WaterSparseMatrix = SparseMatrix<OSQPInt>;

struct WaterFitFacet {
  std::array<std::array<double, 2>, 3> Gradients;
  WaterNormalCap Cap;
  double AreaM2 = 0;
  double NormalBound = 0;
};

struct WaterHeightProgram {
  WaterSparseMatrix Mass, Hessian, Constraints;
  std::vector<double> Linear, Lower, Upper;
  std::vector<WaterFitFacet> Facets;
  std::vector<Vec3> Tangents;
  double DatumM = 0;
  size_t Inequalities = 0;
};

[[nodiscard]] std::expected<WaterHeightProgram, WaterHeightError>
BuildWaterHeightProgram(const WaterHeightProblem &problem,
                        const WaterHeightBudget &budget,
                        const std::stop_token &stop);
}
#endif

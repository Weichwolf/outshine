#ifndef OUTSHINE_GENERATORS_WATER_RIVERBODYHEIGHT_H
#define OUTSHINE_GENERATORS_WATER_RIVERBODYHEIGHT_H

#include "RiverBodyPlan.h"
#include "WaterHeightFit.h"
#include <expected>
#include <stop_token>

namespace outshine::Generators {
struct RiverHeightBudget {
  static constexpr double DefaultDownGrade = .1;
  static constexpr double DefaultCrossGrade = .005;
  static constexpr double DefaultTargetMarginM = 20;
  WaterHeightBudget Fit;
  double DownGrade = DefaultDownGrade;
  double CrossGrade = DefaultCrossGrade;
  double TargetMarginM = DefaultTargetMarginM;
};

[[nodiscard]] std::expected<WaterHeightFit, WaterHeightError>
FitRiverBody(const WaterAsset &water,
             const TangentFrame &frame,
             const RiverBody &body,
             RiverHeightBudget budget = {},
             const std::stop_token &stop = {});
}
#endif

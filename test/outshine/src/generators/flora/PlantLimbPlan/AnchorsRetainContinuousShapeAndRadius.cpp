#include "Check.h"
#include "PlantLimbPlan.h"
#include "math/Vec3.h"
#include <array>
#include <cmath>
#include <cstddef>

int main() {
  using namespace outshine;
  using namespace outshine::Generators;
  using namespace outshine::Test;
  constexpr std::array anchors{
      PlantLimbAnchor{.Node = 23, .PositionM = {{0, 0, 0}}, .RadiusM = 0.2},
      PlantLimbAnchor{.Node = 81, .PositionM = {{0, 1, 0}}, .RadiusM = 0.1},
      PlantLimbAnchor{.Node = 206, .PositionM = {{0.2, 2, 0.3}}, .RadiusM = 0.05}};
  const auto plan = PlantLimbPlan::Create(anchors);
  CHECK(plan.has_value(), "original spatial anchors form a reusable limb without depth IDs");
  if (!plan) { return Report(); }
  CHECK(plan->Nodes().size() == anchors.size() && plan->Nodes()[1] == anchors[1].Node,
        "arbitrary original source node IDs survive preparation");
  constexpr double kToleranceM = 1e-12, kNearJoin = 1e-8;
  for (size_t at = 0; at < anchors.size(); ++at) {
    const auto sample = plan->Sample(static_cast<double>(at) / (anchors.size() - 1));
    CHECK(sample && Length(sample->CentreM - anchors[at].PositionM) < kToleranceM &&
              std::abs(sample->RadiusM - anchors[at].RadiusM) < kToleranceM,
          "both the spatial centreline and PCHIP radius pass through every source anchor");
  }
  const auto left = plan->Sample(0.5 - kNearJoin), right = plan->Sample(0.5 + kNearJoin);
  CHECK(left && right && Length(left->Tangent - right->Tangent) < 1e-6 &&
            std::abs(left->SpeedM - right->SpeedM) < 1e-6 &&
            std::abs(left->RadiusSlopeM - right->RadiusSlopeM) < 1e-6,
        "position and radius derivatives remain continuous across a curved source join");
  double previous = anchors.front().RadiusM;
  constexpr size_t kSamples = 1000;
  for (size_t at = 0; at <= kSamples; ++at) {
    const auto sample = plan->Sample(static_cast<double>(at) / kSamples);
    CHECK(sample && sample->RadiusM <= previous + kToleranceM &&
              sample->RadiusM >= anchors.back().RadiusM - kToleranceM,
          "monotone taper has no cubic overshoot");
    if (sample) { previous = sample->RadiusM; }
  }
  return Report();
}
